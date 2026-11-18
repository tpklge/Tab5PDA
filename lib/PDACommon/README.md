# PDACommon

Núcleo C++17 incorporado ao [Tab5PDA](../../README.md), originado do código
compartilhado com o CardputerPDA. Esta cópia faz parte deste repositório: não é
necessário clonar outro projeto nem manter pastas irmãs para compilar.

Inclui registros, validação UTF-8, busca, datas, recorrências, tarefas,
calculadora/conversores, despesas, lembretes e snapshots com recuperação.
O código independe de Arduino, M5 e LVGL; o adaptador do Tab5 grava em microSD.
O perfil `PDA_TARGET_TAB5` seleciona os limites ampliados e os campos do Tab5.

A interface pública está em `include/pda/`; a implementação está em `src/`.
Os testes do projeto compilam esta mesma cópia. O [roteiro histórico](docs/ROTEIRO_IMPLANTACAO.md)
registra a pesquisa e o planejamento dos dois dispositivos.

[Formato de armazenamento](../../docs/ARMAZENAMENTO.md) · [Validação](../../docs/VALIDACAO.md)
