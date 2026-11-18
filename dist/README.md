# Tab5PDA 1.4.0 — pacote de firmware

[Baixar binários e checksums da release](https://github.com/tpklge/Tab5PDA/releases/tag/v1.4.0).
Os `.bin` ficam nos assets da release; o Git contém fontes, documentação e manifestos.
Compilar e executar o empacotador recria os binários nesta pasta.

- `Tab5PDA-1.4.0.bin`: aplicação ESP32-P4, sem bootloader/partições.
- `Tab5PDA-1.4.0-factory.bin`: imagem combinada, com bootloader, partições e aplicação.
- `.sha256`: checksum de cada imagem.
- `manifest.json`: alvo, layout, UI, versão, fingerprint dos fontes e validação.

A imagem factory usa referência de gravação em **0x0**. Nesse pacote, o
bootloader está em **0x2000**, partições em **0x8000** e aplicação em **0x10000**.
O comando de upload do PlatformIO usa os offsets da configuração automaticamente.

Alvo: **ESP32-P4 ES anterior à revisão 300**, flash 16 MB. A identificação de
painel é delegada ao M5GFX; há suporte de código para ILI9881C/GT911, ST7123 e ST7121,
com validação física pendente. Conferir CPU e perfil no diagnóstico do aparelho.

Persistência no microSD, pasta `/Tab5PDA`, com perfil ampliado de 1.024 registros. Bases com campos novos ou acima dos limites antigos exigem firmware compatível.
Copie a pasta do SD antes de atualizar. A imagem factory pode sobrescrever o
layout técnico da flash; não há dados do PDA na flash. Slots de launchers não
foram homologados nesta entrega.

[Controles e instalação](../README.md) · [Validação](../docs/VALIDACAO.md)
[Interface, temas e calculadoras](../docs/INTERFACE-E-CALCULADORAS.md)

A versão 1.4.0 inclui a preferência de tema na base do microSD. Bases com essa
preferência exigem firmware que a reconheça. O pacote 1.3.1 foi preservado no acervo local e seu manifesto está no Git; voltar à versão anterior exige uma base compatível com ela.

Após compilar, `python3 tools/package_firmware.py` atualiza estes arquivos;
o script não grava aparelhos nem abre portas.

A versão 1.0.0 foi preservada no acervo local; seu manifesto permanece para comparação.
Os binários 1.3.0 ficam no acervo local; `manifest-1.3.0.json` também foi preservado. O livro
financeiro usa `PDA-LEDGER-2` desde 1.3.1 e continua lendo `PDA-LEDGER-1`.
A preferência de tema da base exige 1.4.0 ou firmware compatível.

`assistant_bridge.py`: ponte opcional para um servidor de modelos; configuração e requisitos no [guia](../docs/CONEXAO-E-ASSISTENTE.md). Não é um modelo de IA embutido.
