"""Authenticated local bridge: text -> model -> bounded PDA proposals. Never writes PDA data.
Ollama protocol: https://github.com/ollama/ollama/blob/main/docs/api.md
"""
import argparse
from datetime import datetime
from decimal import Decimal, InvalidOperation
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import hmac
import json
import os
import secrets
import urllib.request
import zlib

SYSTEM = """Você ajuda um PDA em português brasileiro. Retorne SOMENTE JSON no formato
{"records":[{"kind":"task|appointment|note|expense","title":"...","detail":"...","when":"AAAA-MM-DD HH:MM ou vazio","fields":{}}]}.
Até 16 registros. Nunca execute ações ou afirme que algo foi salvo. Resumos e respostas a perguntas viram uma proposta de nota.
Tarefa pode ter fields priority (1/2/3), project e category. Compromisso pode ter until (data/hora), location, reminder (-1 ou minutos antes), repeat (none/daily/weekly/monthly/yearly).
Despesa precisa de fields amount (decimal com ponto) e currency (BRL/USD/EUR ou três letras maiúsculas).
Não invente datas sem referência; se faltar informação obrigatória, proponha uma nota pedindo esclarecimento. Não use IDs, vínculos, anexos ou exclusões."""
LIMITS = {"category": 40, "priority": 1, "project": 40, "until": 16,
          "location": 120, "reminder": 5, "repeat": 7, "repeatEnd": 10,
          "amount": 16, "currency": 3}
KINDS = {"task": 3, "appointment": 2, "note": 4, "expense": 6}


def checked_text(value, limit):
    if not isinstance(value, str) or len(value.encode("utf-8")) > limit:
        raise ValueError("Texto inválido ou grande demais")
    if any(ord(c) < 32 and c not in "\n\t" for c in value):
        raise ValueError("Caractere de controle inválido")
    return value


def date(value):
    if len(value) != 16 or datetime.strptime(value, "%Y-%m-%d %H:%M").strftime("%Y-%m-%d %H:%M") != value:
        raise ValueError("Data inválida")
    if not 2000 <= int(value[:4]) <= 2099:
        raise ValueError("Data fora do intervalo")


def proposals(payload):
    records = payload.get("records") if isinstance(payload, dict) else None
    if not isinstance(records, list) or not 1 <= len(records) <= 16:
        raise ValueError("O modelo deve propor entre 1 e 16 registros")
    output = []
    for index, source in enumerate(records, 1):
        if not isinstance(source, dict) or source.get("kind") not in KINDS:
            raise ValueError("Tipo de proposta inválido")
        kind = KINDS[source["kind"]]
        title = checked_text(source.get("title", ""), 80)
        detail = checked_text(source.get("detail", ""), 16384)
        when = checked_text(source.get("when", ""), 16)
        if not title.strip() or "\n" in title or "\t" in title:
            raise ValueError("Título inválido")
        if when:
            date(when)
        if kind in (2, 6) and not when:
            raise ValueError("Proposta exige data/hora")
        if kind == 4 and when:
            raise ValueError("Notas não têm prazo")
        fields = source.get("fields", {})
        if not isinstance(fields, dict):
            raise ValueError("Campos inválidos")
        allowed = {4: {"category"}, 3: {"category", "priority", "project"},
                   2: {"category", "until", "location", "reminder", "repeat", "repeatEnd"},
                   6: {"category", "amount", "currency"}}[kind]
        for key, value in fields.items():
            if key not in allowed:
                raise ValueError("Campo não permitido na proposta")
            checked_text(value, LIMITS[key])
        if "until" in fields:
            date(fields["until"])
            if fields["until"] < when:
                raise ValueError("Fim anterior ao início")
        if "repeat" in fields and fields["repeat"] not in ("none", "daily", "weekly", "monthly", "yearly"):
            raise ValueError("Recorrência inválida")
        if "repeatEnd" in fields:
            date(fields["repeatEnd"] + " 00:00")
            if fields["repeatEnd"] < when[:10]:
                raise ValueError("Término de recorrência inválido")
        if "reminder" in fields:
            value = fields["reminder"]
            if value != "-1" and (not value.isascii() or not value.isdigit() or int(value) > 10080):
                raise ValueError("Aviso inválido")
        if "priority" in fields and fields["priority"] not in ("1", "2", "3"):
            raise ValueError("Prioridade inválida")
        if kind == 6:
            try:
                money = Decimal(fields.get("amount", ""))
                if not money.is_finite() or abs(money) > Decimal("1000000000000") or money.as_tuple().exponent < -2:
                    raise ValueError("Valor inválido")
            except InvalidOperation as exc:
                raise ValueError("Valor inválido") from exc
            fields = dict(fields, amount=f"{money:.2f}")
            currency = fields.get("currency", "")
            if len(currency) != 3 or not currency.isascii() or not currency.isupper() or not currency.isalpha():
                raise ValueError("Moeda inválida")
        output.append(dict(fields, type="record", id=str(index), kind=str(kind), title=title,
                           detail=detail, when=when, completed="0"))
    return output


def framed(records):
    def row(value):
        return (json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n").encode()
    data = row(dict(format="pda-backup-v1", schema="2", records=str(len(records)), nextId=str(len(records) + 1)))
    data += row(dict(type="settings", brightness="160", volume="100", timeout="60", offset="480", lastClock=""))
    data += b"".join(row(record) for record in records)
    data += row(dict(type="commit", crc32=f"{zlib.crc32(data) & 0xffffffff:08x}"))
    if len(data) > 65536:
        raise ValueError("Prévia maior que 64 KiB")
    return data


def generate(endpoint, model, protocol, key, prompt, now):
    messages = [{"role": "system", "content": SYSTEM + "\nHora local informada pelo PDA: " + now},
                {"role": "user", "content": prompt}]
    body = dict(model=model, messages=messages, stream=False)
    if protocol == "ollama":
        body["format"] = "json"
    else:
        body["response_format"] = {"type": "json_object"}
    headers = {"Content-Type": "application/json"}
    if key:
        headers["Authorization"] = "Bearer " + key
    request = urllib.request.Request(endpoint, data=json.dumps(body).encode(), headers=headers)
    with urllib.request.urlopen(request, timeout=50) as response:
        raw = response.read(131073)
    if len(raw) > 131072:
        raise ValueError("Resposta do modelo grande demais")
    result = json.loads(raw)
    content = result["message"]["content"] if protocol == "ollama" else result["choices"][0]["message"]["content"]
    return framed(proposals(json.loads(content)))


def handler(token, generator):
    class Handler(BaseHTTPRequestHandler):
        def setup(self):
            super().setup()
            self.connection.settimeout(10)

        def reply(self, code, data, mime="text/plain; charset=utf-8"):
            self.send_response(code)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_POST(self):
            if self.path != "/assistant":
                self.reply(404, b"Not found")
                return
            if not hmac.compare_digest(self.headers.get("Authorization", ""), "Bearer " + token):
                self.reply(403, b"Invalid access code")
                return
            try:
                size = int(self.headers.get("Content-Length", "0"))
                if not 0 < size <= 4096:
                    self.reply(413, b"Prompt too large or empty")
                    return
                prompt = self.rfile.read(size).decode("utf-8")
                now = checked_text(self.headers.get("X-PDA-Now", ""), 16)
                if now:
                    date(now)
                data = generator(prompt, now)
                self.reply(200, data, "application/x-ndjson; charset=utf-8")
            except (ValueError, KeyError, TypeError):
                self.reply(422, b"Invalid model proposal; nothing was applied")
            except Exception:
                self.reply(502, b"Model server unavailable; nothing was applied")

        def log_message(self, fmt, *args):
            pass  # Do not write prompts or tokens into access logs.
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--listen", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    parser.add_argument("--endpoint", default="http://127.0.0.1:11434/api/chat")
    parser.add_argument("--model", required=True)
    parser.add_argument("--protocol", choices=("ollama", "chat-completions"), default="ollama")
    args = parser.parse_args()
    token = os.environ.get("PDA_ASSISTANT_TOKEN") or secrets.token_urlsafe(24)
    key = os.environ.get("PDA_UPSTREAM_KEY", "")
    generator = lambda prompt, now: generate(args.endpoint, args.model, args.protocol, key, prompt, now)
    server = ThreadingHTTPServer((args.listen, args.port), handler(token, generator))
    print(f"Assistant bridge on {args.listen}:{args.port}; enter the PC's LAN address in the PDA.")
    print(f"Session code: {token}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
