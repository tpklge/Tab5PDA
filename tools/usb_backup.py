"""Transfer Tab5PDA JSONL backups through an explicitly enabled USB session."""
import argparse
import json
import time
import zlib
from pathlib import Path

LIMIT = 4 * 1024 * 1024


def validate(data):
    if not data or len(data) > LIMIT or not data.endswith(b"\n"):
        raise ValueError("Backup incompleto ou maior que 4 MiB")
    rows = data.splitlines(keepends=True)
    header = json.loads(rows[0])
    commit = json.loads(rows[-1])
    if header.get("format") != "pda-backup-v1" or header.get("schema") != "2":
        raise ValueError("Formato incompatível")
    for row in rows:
        json.loads(row)
    crc = f"{zlib.crc32(b''.join(rows[:-1])) & 0xffffffff:08x}"
    if commit != {"crc32": crc, "type": "commit"}:
        raise ValueError("Checksum do backup inválido")


def response(port, prefix, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode("utf-8", errors="replace").strip()
        if line.startswith("PDA ERROR"):
            raise RuntimeError(line)
        if line.startswith(prefix):
            return line
    raise TimeoutError("Sem resposta. Abra Backup e ative a sessão USB no Tab5.")


def receive(port):
    port.write(b"PDA GET\n")
    header = response(port, "PDA DATA ").split()
    if len(header) != 4:
        raise ValueError("Cabeçalho USB inválido")
    size, crc = int(header[2]), int(header[3], 16)
    if not 0 < size <= LIMIT:
        raise ValueError("Tamanho USB inválido")
    data = bytearray()
    deadline = time.monotonic() + 120
    while len(data) < size and time.monotonic() < deadline:
        data.extend(port.read(min(128, size - len(data))))
    if len(data) != size or zlib.crc32(data) & 0xffffffff != crc:
        raise ValueError("Transferência incompleta ou corrompida")
    response(port, "PDA END")
    validate(data)
    return bytes(data)


def send(port, data):
    validate(data)
    port.write(f"PDA PUT {len(data)} {zlib.crc32(data) & 0xffffffff:08x}\n".encode())
    response(port, "PDA READY")
    for start in range(0, len(data), 128):
        port.write(data[start:start + 128])
    response(port, "PDA STAGED")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("get", "put"))
    parser.add_argument("port", help="Porta serial explícita do aparelho")
    parser.add_argument("file", type=Path)
    args = parser.parse_args()
    import serial  # pip install pyserial; no automatic port selection or reset.
    with serial.Serial(port=None, baudrate=115200, timeout=.25, write_timeout=10) as port:
        port.dtr = False
        port.rts = False
        port.port = args.port
        port.open()
        if args.action == "get":
            data = receive(port)
            args.file.write_bytes(data)
            print(f"Backup verificado: {args.file} ({len(data)} bytes)")
        else:
            send(port, args.file.read_bytes())
            print("Arquivo validado no SD. Toque Restaurar recebido por USB em Backup e confirme na tela.")


if __name__ == "__main__":
    main()
