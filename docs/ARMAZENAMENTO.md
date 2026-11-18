> Documento do formato compartilhado, originalmente escrito para o Cardputer.
> No Tab5, o adaptador usa `/Tab5PDA/` na raiz do microSD e os limites ampliados
> descritos no [README](../README.md). A release inclui o núcleo em `lib/PDACommon`.

# Armazenamento e backup — esquema 2

Toda persistência do PDA reside no microSD, sem banco em NVS/SPIFFS/LittleFS.
O adaptador é do CardputerPDA; modelos, codecs e regras são do PDACommon.

## Snapshots

`/CardputerPDA/state-a.pda` e `state-b.pda` alternam gerações. Os inteiros são
little-endian; strings UTF-8 têm prefixo uint16 de tamanho em bytes.

| Parte | Conteúdo |
| --- | --- |
| Assinatura | ASCII `CPDA`, quatro bytes |
| Versão / reservado | byte 2 + três bytes zero; o leitor aceita também versão 1 |
| Geração | uint64, maior que zero |
| Tamanho do payload | uint32 |
| Payload | Estado abaixo |
| CRC32 | IEEE sobre cabeçalho e payload |
| Confirmação | ASCII `PDACOMIT`, oito bytes |

O prefixo do payload mantém o esquema 1: nextId uint32, brilho uint8, volume
uint8, timeout uint16, offset int16, último ajuste string, quantidade uint16,
registros, rascunho ativo uint8 e, se ativo, campo uint8 + registro.

Registro: ID uint32, tipo uint8 (1 contato, 2 compromisso, 3 tarefa, 4 nota,
5 aniversário, 6 despesa), concluído uint8, título, texto e data/hora strings.

A extensão do esquema 2 acrescenta mapas nesta ordem: preferências, campos de
cada registro, campos do rascunho ativo, estados de alarmes. Mapa: quantidade
uint16 e pares chave/valor como strings. Os campos permitidos e seus limites
são validados; não há serialização de structs ou ponteiros.

Datas são minutos civis `AAAA-MM-DD HH:MM`. Compromissos/tarefas preservam o
offset usado ao gravar; recorrências operam nesse calendário civil. O instante
é derivado com aritmética de 64 bits. Dia inteiro e aniversário mantêm data civil.
Estado de alarme: chave `ID@ocorrência`, valor `dismissed` ou minuto **UTC** de
adiamento. O lembrete único de cada registro tem identidade estável por ocorrência.

Gravação: validar o estado, escrever/flush/fechar o banco inativo, liberar o
buffer de saída, reler e conferir tamanho, CRC32 esperado e marcador final.
Só então atualizar geração e estado em RAM. Essa estratégia evita manter dois
buffers completos ao mesmo tempo. Erros de leitura/gravação bloqueiam commits;
reabertura durante edição exige a mesma base confirmada.

O boot escolhe a maior geração válida. Truncamento/corrupção recupera a outra
cópia. Esquema desconhecido ou erro de I/O bloqueia escrita; arquivos inválidos
não são apagados e o cartão não é formatado automaticamente.

A migração lê esquema 1 e grava esquema 2 no próximo commit. A 0.1.0 recusa
esquema 2; retorno exige a cópia da pasta feita **antes** da atualização.

## JSONL `pda-backup-v1`

O pacote da 1.0.0 é um fluxo UTF-8, limitado a 32 KiB, com uma linha JSON por
objeto, chaves/valores string e newline final. Não há anexos nessa versão.

1. Manifesto: `format=pda-backup-v1`, `schema=2`, `nextId`, `records`.
2. Preferências: `type=settings`, brilho, volume, timeout, offset armazenado
   como deslocamento +720, último ajuste e campos adicionais.
3. Registros: `type=record`, ID, kind, completed, title, detail, when e campos.
4. Rascunho opcional: `type=draft`, mesmos dados e `field`.
5. Alarmes: `type=alarm`, `key`, `until`.
6. Commit: `type=commit`, `crc32` hexadecimal, oito caracteres, sobre todos os
   bytes anteriores à linha do commit, inclusive seus newlines.

O exportador usa UTF-8 literal e escapes `\"`, `\\`, `\n`, `\t`. O importador
aceita esse subconjunto de objetos string e whitespace entre elementos; não
aceita números JSON, objetos aninhados, escapes `\u` ou um formato arbitrário
de terceiros. O esquema comum está disponível para o futuro Tab5PDA.

Importação verifica tamanho, manifesto, contagens, checksum, identificadores,
UTF-8, campos, links, datas e limites antes de modificar a base. Substituição
usa o mesmo commit A/B; falha mantém o estado confirmado anterior. Não há mesclagem.

Arquivos:

- `backup-GERAÇÃO.jsonl`: cópia versionada, com geração decimal de 16 dígitos.
- `backup.jsonl`: cópia mais recente para restauração pela tela.
- `incoming.jsonl`: arquivo recebido por USB; precisa de I + Enter para restaurar.

Cada cópia de backup é relida e comparada. Uma falha ao atualizar `backup.jsonl`
não apaga a cópia versionada já confirmada. Versões antigas não são removidas
automaticamente; copiar/excluir os arquivos pelo computador libera espaço.

## Transporte USB

Somente após U em Backup e enquanto desbloqueado. Comandos ASCII delimitados:

```text
PC: PDA GET\n
PDA: PDA DATA <bytes> <crc32>\n
PDA: <bytes JSONL>\nPDA END\n

PC: PDA PUT <bytes> <crc32>\n
PDA: PDA READY\n
PC: <bytes JSONL>
PDA: PDA STAGED\n
```

CRC do transporte cobre o arquivo inteiro. Entradas/saídas são processadas em
blocos de até 128 bytes por volta do laço. Timeout sem progresso: 15 segundos.
Fechar a tela, bloquear ou apresentar um alarme interrompe o transporte.
O utilitário não escolhe porta, não grava firmware e não pede reset por DTR/RTS.
Recebimento inválido/interrompido não altera os snapshots da agenda.

## Limites e evidências

Os limites constam no README. O orçamento de estruturas usa uma estimativa
conservadora independente da arquitetura, além do limite de texto/snapshot;
a presença de muitos campos ou alarmes pode reduzir a capacidade disponível.
A agenda permanece integralmente em RAM. Journal, streaming integral da base
e anexos ficam no backlog P2.

Os testes nativos cobrem um fixture real do esquema 1, migração, truncamentos,
corrupção, falha de confirmação, rascunhos, backups e restauração. Corte real
de energia/FAT, remoção física do SD e consumo exigem a bancada dos aparelhos.
