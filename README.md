# Tab5PDA — 1.4.0

PDA para **M5Stack Tab5**, em projeto separado do CardputerPDA, com **LVGL**, fonte
Montserrat nominal de **16 px** com acentos e persistência no **microSD**.
A tela funciona em paisagem, **1280 × 720**, com operação integral por toque.

## Interface e calculadoras — 1.4.0

- Campos com cursor fino apenas no foco, inicial maiúscula e nomes próprios
  capitalizados automaticamente, com suporte a acentos.
- Teclado virtual disponível quando o teclado físico não é detectado; conectar
  ou desconectar preserva o campo em edição.
- Leitura com título maior, rótulos em negrito e observações justificadas.
- Calculadora com modos básica, científica e financeira; entrada pelo teclado,
  funções específicas por toque e visor amplo. Sem painel de dígitos/operações básicas.
- Conversor com seletores de grandeza, origem/destino e taxa manual para moedas.
- Relógios digitais grandes, com cidade acima da hora; Wi-Fi com status compacto.
- Ajustes abre diretamente para editar. **Tema visual → Sereno / Areia / Ardósia → Salvar**.

[Guia da interface e calculadoras](docs/INTERFACE-E-CALCULADORAS.md).

## Revisão corretiva — 1.3.1

Correções no teclado de documentos, campos sob o teclado, retomada após alarmes,
importação de textos/CSV do Windows, limites do parser e proteção do livro
financeiro. Os testes incluem edição e salvamento de documento de 128 KiB,
comandos repetidos, falhas de leitura/escrita e entradas malformadas.
Detalhes e limites na [validação](docs/VALIDACAO.md).

## Novos aplicativos — 1.3.0

Em **Backup → Mais ferramentas → Planilha, documentos, livros, galeria e finanças**:

- Miniplanilha de 20 × 8 células, fórmulas aritméticas, SUM/SOMA e importação/exportação CSV.
- Editor de documentos TXT/Markdown até 128 KiB, localizar/substituir, seleção,
  copiar/recortar/colar e oito estados para desfazer.
- Leitor TXT/Markdown até 512 KiB, retomada da página e até 16 marcadores por livro.
- Galeria JPEG/PNG/BMP, ajuste proporcional à tela e navegação entre imagens.
- Finanças com contas, receitas, despesas, categorias, orçamento mensal,
  lançamentos mensais recorrentes e exportação CSV, separando as moedas.

Arquivos e posições de leitura ficam em `/Tab5PDA/` no microSD. O livro financeiro
usa dois arquivos alternados com checksum. O [guia de produtividade](docs/PRODUTIVIDADE.md)
explica salvamento, recuperação, formatos e limites. [Capturas das novas telas](docs/previews/).

## Tela maior, menus e formulários

**17 aplicações ficam visíveis ao mesmo tempo**, em duas colunas laterais:
Hoje, Contatos, Agenda, Tarefas, Notas, Aniversários, Despesas, Calculadora,
Conversores, Relógios, Calendário, Busca geral, Backup, Ajustes, Wi-Fi/hora,
Bloqueio e Diagnóstico. As cores seguem o tema visual selecionado.

Listas e detalhes aparecem lado a lado. O calendário mostra o mês inteiro
com marcadores e os compromissos do dia. Os formulários mostram **todos os
campos juntos**, em duas colunas, com observações ocupando a largura disponível.
Nas notas, o texto recebe a maior parte da área de edição.

O teclado não aparece ao abrir um formulário. Sem teclado físico detectado, ao
**tocar em um campo**, o teclado virtual abre e o formulário diminui, mantendo o campo ativo visível e permitindo
rolagem dos demais campos. **Fechar teclado** devolve toda a área ao formulário,
com os valores preservados. Não existe rotação de campos por Tab.

O teclado tem números, Shift, símbolos, espaços, apagar, nova linha e paleta
com acentos/ç. Salvar é uma ação explícita da barra do formulário. A rolagem de
listas não seleciona registros nem confirma ações.

As [capturas de interface](docs/previews/) vêm do LVGL real, executado no
computador em 1280 × 720; não são maquetes HTML nem desenhos simulados.

## Funções

O núcleo [PDACommon](lib/PDACommon/README.md) compartilha as funções com o
CardputerPDA 1.0.0:

- Contatos com empresa, telefones, e-mail, endereço, categoria e observações.
- Agenda com início/fim, local, categoria, dia inteiro e confirmação de sobreposição.
- Lembretes visuais/sonoros, antecipação, dispensar e adiar cinco minutos.
- Recorrência diária/semanal/mensal/anual, término, edição da série/ocorrência
  e cancelamento isolado; meses sem o dia solicitado são pulados.
- Tarefas com prioridade, prazo e conclusão; notas com rascunho persistente.
- Aniversários com ano opcional, vínculo por ID e opção `feb28`/`skip` para 29/02.
- Despesas em centavos inteiros e totais separados por moeda/período.
- Busca geral por todos os campos, ignorando caixa e acentos.
- Calculadora com percentual/memória, conversores e três relógios mundiais.
- Backup versionado no SD e por USB, com checksum e confirmação de restauração.
- PIN de 4–8 dígitos, bloqueio por inatividade e exportação bloqueada enquanto travado.
- Hora/fuso, brilho, volume, timeout e formato de data ISO/BR.

Datas de edição usam `AAAA-MM-DD HH:MM`, de 2000 a 2099. O formato BR só altera
a exibição. Para aniversário anterior a 2000, informe uma data base válida e
use o campo separado de ano de nascimento. IDs aparecem no detalhe dos registros.

Recorrência: `none`, `daily`, `weekly`, `monthly`, `yearly`. Prioridade: `1` alta,
`2` média, `3` baixa. Aviso: vazio/`-1` desliga; `0` avisa na hora; até 10080 minutos
antes. No detalhe de uma série, **Editar ocorrência** oferece edição ou cancelamento
apenas da data escolhida. **Editar** altera a série.

Percentual é uma fração: `200*10%=20`; memória M+/M−/MR/MC dura durante a sessão.
Conversões: `1;km;m`, `32;F;C`, `10;USD;BRL;5.25`. Unidades:
`mm cm m km in ft mi`, `g kg lb oz`, `ml l galUS`, `C F K`.
Moedas usam taxa manual. Fusos mundiais são offsets, sem ajuste automático de
horário de verão. Em Despesas, o período de totais é `AAAA-MM`, `AAAA` ou vazio.

## Hardware

O firmware usa o ESP32-P4; o rádio C6 embarcado se comunica por SDIO. M5GFX
identifica o painel/touch na inicialização, com caminhos para ILI9881C/GT911,
ST7123 e ST7121. O diagnóstico deriva o nome do perfil escolhido pelo SDK,
sem decidir painel apenas por ACK no endereço I2C.

[Hardware e revisões oficiais](https://docs.m5stack.com/en/core/Tab5),
[API touch](https://docs.m5stack.com/en/arduino/m5tab5/touch),
[Wi-Fi/SDIO](https://docs.m5stack.com/en/arduino/m5tab5/wifi).

LVGL e dois buffers parciais usam PSRAM. M5GFX é o único responsável pela tela;
a entrada usa o estado touch já atualizado por M5Unified, com um dedo. A fonte
nominal de 16 px tem altura de linha de 21 px para acomodar acentos.

O microSD usa SPI (SCK43/MISO39/MOSI44/CS42), separado do SDIO do C6. O relógio
usa o RTC RX8130CE quando válido, armazenado em UTC, ou ajuste manual/NTP.
Uma tarefa de rede executa conexão e sincronização; só o laço principal atualiza
RTC, estado de hora e interface. A conexão manual permanece ativa até
Desconectar; a sincronização automática de boot desliga o rádio ao concluir
se não havia uma conexão manual ativa. O limite de tentativa é 30 segundos, além de eventual demora interna
na inicialização do driver C6, sem bloquear a interface.

Tela apagada mantém MCU ativo e lembretes; o primeiro toque apenas acorda a
iluminação. Não há promessa de alarme com desligamento físico. A versão 1.2.0 integra gravador de voz e notas desenhadas.
Câmera, IMU/gestos e teclado USB externo permanecem no backlog.

## Persistência e intercâmbio com o Cardputer

Os arquivos ficam em **`/Tab5PDA/`**, em cartão FAT32 compatível com o driver.
Não há formatação automática nem base alternativa na flash. O adaptador mapeia
os caminhos lógicos do núcleo para essa pasta; a pasta `/CardputerPDA` permanece
separada. Snapshots A/B, geração, CRC32, confirmação por releitura e rascunhos
usam o mesmo esquema 2, com leitura do esquema 1.

O backup `pda-backup-v1` preserva IDs,
campos, configurações, rascunho e estados de lembretes. Para transferir pelo SD,
crie o backup na origem, copie `backup.jsonl` para a pasta da aplicação no
aparelho de destino e confirme **Restaurar backup.jsonl** na tela.

Para USB, abra Backup e toque **Ativar sessão USB** (após NTP concluir):

```sh
python3 -m pip install pyserial==3.5
python3 tools/usb_backup.py get /dev/cu.PORTA backup.jsonl
python3 tools/usb_backup.py put /dev/cu.PORTA backup.jsonl
```

O envio apenas valida/grava `incoming.jsonl`. A substituição exige **Restaurar
recebido por USB → Confirmar restauração**. Fechar Backup, bloquear, apresentar
um aviso ou entrar em confirmação encerra o transporte. PIN restringe a interface;
**arquivos não são cifrados**. Backup inclui credenciais Wi-Fi e preferências.
Copie a pasta do SD antes de atualizar/restaurar. Não há mesclagem de agendas.

A versão 1.1.0 amplia o perfil do Tab5 para **1.024 registros** (incluindo
lixeira e subtarefas), **1 MiB de texto agregado**, títulos de **80 bytes**,
observações/notas de **16 KiB**, snapshots/JSONL de **4 MiB** e até **8.192
estados de lembrete**. O orçamento estimado das estruturas/textos é **4 MiB**;
os limites são compartilhados e podem ser atingidos antes de 1.024 registros.
As estruturas e buffers grandes usam PSRAM; a base continua em RAM, com
snapshots A/B no SD. As listas mostram páginas de 32 registros.

Dados antigos continuam sendo lidos. **Bases ampliadas, campos de tarefas novos
e lixeira não devem ser restaurados em firmwares antigos**, inclusive o
CardputerPDA 1.0.0. O perfil do Cardputer mantém os limites anteriores. Use os
formatos externos para transferir registros selecionados por tipo; não há
redução ou descarte automático para caber no Cardputer. Faça uma cópia da pasta
antes de atualizar; para voltar ao firmware anterior, restaure essa cópia.
[Formato compartilhado](docs/ARMAZENAMENTO.md).

## Código e downloads

[Código no GitHub](https://github.com/tpklge/Tab5PDA) ·
[Release 1.4.0 e binários](https://github.com/tpklge/Tab5PDA/releases/tag/v1.4.0)

O repositório é autossuficiente: o núcleo compartilhado está em `lib/PDACommon`.
Não é necessário ter o CardputerPDA nem outra pasta ao lado do clone. Binários
são distribuídos nos assets das releases; `dist/` é a saída local do empacotador.

## Compilar, verificar e instalar

```sh
pio run -e tab5
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/build -j 8
ctest --test-dir tests/build --output-on-failure
python3 tools/package_firmware.py
```

No macOS, caso o SDK padrão seja incompatível com o linker, acrescente
`-DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk`.

[Dependências fixadas](docs/DEPENDENCIAS.md). A [release](https://github.com/tpklge/Tab5PDA/releases/tag/v1.4.0) contém imagem
de aplicação e imagem factory com bootloader/partições. O empacotador também gera essas imagens localmente em `dist/`. Para gravação pelo
PlatformIO, escolha a porta do seu Tab5:

```sh
pio run -e tab5 -t upload --upload-port /dev/cu.PORTA
```

Nenhum aparelho foi gravado automaticamente. Slots de launchers não foram
homologados. O alvo de build é `m5stack-tab5-p4` para silício P4 ES anterior à
revisão 300; outras revisões de CPU exigem alvo/SDK apropriado, além da
compatibilidade de painel. Conferir a revisão no diagnóstico e em bancada.

A 1.0.1 corrige a orientação em paisagem para o teclado físico e troca o envio
de pixels por `pushImage`, com limpeza e redesenho completo na inicialização,
para corrigir os resíduos de atualização observados na 1.0.0. A correção
da tela ainda precisa ser confirmada no aparelho.

O estado de compilação/testes e os itens de bancada estão em
[VALIDACAO.md](docs/VALIDACAO.md). A versão 1.2.0 foi compilada e testada no
computador; a operação física no Tab5 ainda aguarda validação.

## Teclado físico — 1.0.2

Suporte ao Tab5 Keyboard acoplado, I²C 0x6D no conector GPIO0/GPIO1.
Leitura HID a cada 20 ms, até oito eventos por ciclo, com detecção/reconexão
a cada segundo quando ausente. Soltar uma tecla não repete a digitação.
Aa e Sym são resolvidos pelo firmware do teclado; o aplicativo traduz o HID
em letras, números, pontuação e comandos. Teclados USB/Bluetooth não estão
incluídos nesta integração.

- Tab / Shift+Tab: próximo / anterior campo, todos continuam na mesma tela.
- Enter: próximo campo; quebra de linha em texto longo; confirma busca e PIN.
- Backspace / Delete e setas: edição do campo; Home / End: início / fim.
- Ctrl+S: salvar formulário ou ajustes; Ctrl+F: buscar na tela normal.
- Esc: voltar; setas na tela sem campo: alternar aplicações.

Ao digitar fisicamente, o teclado touch fecha e o formulário recupera a área
completa. Um toque no campo permite abrir o teclado touch novamente. A primeira
tecla com tela apagada apenas desperta a tela. O diagnóstico mostra a conexão.
Referência: https://docs.m5stack.com/en/arduino/projects/tab5/tab5_keyboard

## Máscaras e rascunhos — 1.0.3

Data/hora: digite `202610131430` para obter `2026-10-13 14:30`.
Datas sem hora recebem `AAAA-MM-DD`; período de totais recebe `AAAA-MM`
ou apenas o ano. A entrada aceita dígitos e insere os separadores; Backspace
permite apagar até deixar o campo vazio. Datas inexistentes, horários inválidos
e campos obrigatórios incompletos continuam sendo recusados ao salvar.

Valores monetários aceitam ponto ou vírgula, sinal negativo inicial e até duas
casas decimais, sem separadores de milhar. Por exemplo, `12,3` é salvo como
`12.30`, mantendo o formato compatível do microSD/backup. Letras, sinal no meio,
segundo separador decimal e terceira casa decimal são recusados. IDs, PIN,
ano de nascimento, prioridade, aviso, brilho, volume, timeout e fusos também
restringem caracteres numéricos. Seus limites e regras são verificados ao salvar.
O teclado touch mostra apenas dígitos e controles adequados ao tipo do campo;
as mesmas restrições valem para o teclado físico.

O rascunho automático continua único no esquema da base. A 1.2.0 acrescenta
cópias independentes no SD por Guardar rascunho, permitindo vários documentos
em andamento. Ao criar ou
editar um registro com rascunho pendente, a tela identifica seu tipo e título.
É possível retomar o tipo original, manter e voltar ou descartar explicitamente
e abrir o novo registro solicitado. Um rascunho de despesas não se transforma
em contato; nenhum rascunho é apagado automaticamente ao trocar de aplicação.

## Ajustes, Wi-Fi e formato brasileiro — 1.0.4

Em **Ajustes → Editar todos os ajustes**, brilho e volume usam sliders. O tempo
de tela e o formato ISO/BR usam seletores. O fuso é selecionado em **horas
inteiras de UTC−12 a UTC+14**, tanto nos ajustes quanto nos relógios mundiais.
O núcleo continua armazenando o fuso em minutos para compatibilidade. Valores
antigos fracionários são apresentados truncados para a hora inteira; salvar
os ajustes confirma a opção exibida. Timeouts personalizados antigos também
continuam disponíveis no seletor. O timeout do bloqueio usa um seletor; o PIN
continua sendo digitado com máscara numérica. Tab/setas/Ctrl+S operam os controles
com o teclado físico. As alterações são aplicadas ao salvar.

Escolha **BR** e salve para usar máscaras `DD/MM/AAAA` e `DD/MM/AAAA HH:MM`,
incluindo agendas, aniversários, despesas, final da recorrência, ocorrência
e hora manual. Por exemplo, `131020261430` vira `13/10/2026 14:30`.
Valores monetários são exibidos com vírgula e salvos com duas casas decimais
no formato interno. O filtro de totais permanece `AAAA-MM` ou `AAAA`.
O relógio não é reajustado por mudanças em brilho, fuso ou formato de data.

Na aplicação **Wi-Fi/hora**:

1. Toque **Buscar redes**. A busca roda fora da tarefa da interface.
2. Selecione um SSID; a lista informa sinal em dBm e rede aberta/protegida.
3. Digite a senha e toque **Salvar e conectar**. Para redes abertas a senha
   é limpa automaticamente. Redes ocultas podem ter SSID digitado manualmente.
4. O estado informa conexão e IP. Use **Sincronizar hora NTP** quando necessário
   e **Desconectar** para desligar o rádio. **Conectar** reutiliza os dados salvos.

A busca limita resultados a 32 SSIDs únicos, priorizados pelo sinal, e mantém
uma conexão já ativa. A interface recusa uma nova operação enquanto outra está
em andamento e preserva a senha digitada quando o estado da rede muda.
A perda da conexão é informada; toque Conectar para tentar novamente.
NTP sem resposta mantém uma conexão manual ativa.

## Seletores de formulários — 17/11/2026

Toque **Selecionar** ao lado de uma data para abrir o calendário mensal.
Use **Mês −/+** e **Ano −/+**, toque no dia e escolha hora/minuto nas listas.
**Aplicar data** preenche o campo; **Cancelar seleção** mantém o valor anterior;
**Limpar campo** deixa-o vazio, sujeito à validação dos campos obrigatórios.
O fim usa o início do compromisso como referência quando estiver vazio.
O seletor respeita anos bissextos e os formatos ISO/BR; o formato salvo continua
sendo ISO. Sem data existente nem relógio válido, a referência é 01/01/2000
para escolha explícita. O final da recorrência e a ocorrência usam só a data.
A hora manual em Ajustes também tem seletor.

Recorrência, prioridade, dia inteiro, lembretes, moeda, regra de 29/02,
categorias existentes e contatos têm listas de seleção por toque. Categorias
novas e valores personalizados continuam podendo ser digitados no campo.
A seleção preenche o formulário: toque **Salvar** para gravar o registro.
Registros, preferências, rascunhos e backups continuam em **`/Tab5PDA/`**, na
raiz do microSD, com snapshots A/B e confirmação por releitura.

## Prioridade 1 — 1.1.0

**Hoje:** compromissos abrem o registro, caixas de tarefas concluem sem sair da
tela, atrasos são destacados e botões criam agenda/tarefa/nota. A tela mostra
até 12 próximos eventos e 12 tarefas; a lista completa continua nas aplicações.

**Calendário:** Mês, Semana e Dia. A semana começa na segunda; a grade inteira
vai de 00:00 a 23:00 com rolagem e abre inicialmente às 07:00. A altura do evento
representa a duração. Toque no evento e em **Abrir compromisso** ou
**Reagendar**; depois toque no horário de destino e confirme a prévia com as
sobreposições encontradas. Em série recorrente, só a ocorrência selecionada é
alterada. Dia inteiro é alterado pelo formulário. Toque no espaço livre para
criar já com data/hora e duração inicial de uma hora. A grade mostra até três
faixas simultâneas; a lista inferior mostra os primeiros 128 eventos do período.
O cálculo tem teto de 4.096 ocorrências por período; para períodos densos, use
Dia ou a lista de Agenda, que permite acessar todos os registros por páginas.

**Tarefas:** projeto/lista, tarefa principal e recorrência. **Nova subtarefa**
herda o projeto; caixas no detalhe concluem os itens de uma lista de compras.
O botão ao lado de Concluir alterna Todas, Pendentes, Atrasadas, Prioridade alta
e Concluídas. A busca também consulta projeto e categoria. A conclusão exige
subtarefas concluídas. Em tarefa recorrente com prazo, concluir avança para a
próxima data válida e reabre as subtarefas; meses sem o dia são pulados. A data
de conclusão registra a ocorrência anterior. O detalhe mostra até 24 subtarefas;
todas permanecem acessíveis na lista de Tarefas.

**Lixeira:** Excluir agora move o registro para a lixeira, oculta-o das listas,
totais e lembretes e mantém o ID. Em Backup, **Desfazer exclusão** recupera a
última exclusão da sessão; **Lixeira → Restaurar** funciona após reiniciar.
Excluir uma tarefa inclui as subtarefas ativas; restaurar recupera o grupo
excluído junto, preservando itens que já estavam na lixeira antes. **Apagar**
exige confirmação e libera capacidade. Não há limpeza automática da lixeira.

**Backup diário:** com hora válida e a interface em estado normal, a primeira
verificação de cada dia grava `daily-AAAA-MM-DD.jsonl`. Uma cópia válida já
existente não é substituída. Falhas são informadas e tentadas novamente após
cinco minutos. Os arquivos são mantidos no SD, sem expurgo automático.
Em Backup, **Backup por data** abre o nome para edição; toque **Restaurar backup
por nome** e confirme a substituição. O backup manual versionado continua
existindo. Não há backup diário sem data válida ou com aparelho desligado.

**Intercâmbio:** em Backup → Importar / exportar, escolha o formato e o nome do
arquivo na raiz de `/Tab5PDA/`. Exportar grava e verifica o arquivo. Importar
mostra prévia e só acrescenta os registros após confirmação, em uma transação;
um arquivo inválido ou falha no SD não deixa uma importação parcial. Importar
novamente cria duplicatas. Itens da lixeira não são exportados por esses formatos.

| Formato | Suporte |
| --- | --- |
| ICS | Compromissos; data local ou UTC, dia inteiro, diária/semanal/mensal/anual, término, exceções e aviso em minutos. Exporta horários em UTC. Importação exige segundos `00`; recusa TZID, BYDAY/COUNT, INTERVAL diferente de 1, RDATE, DURATION e RECURRENCE-ID. Não é um cliente CalDAV. |
| VCF | Contatos UTF-8 com nome, telefones, e-mail, empresa, endereço, categoria e notas. Exporta vCard 3.0; não lê quoted-printable/base64. |
| CSV | Despesas; cabeçalho `title,when,amount,currency,category,detail`, data ISO e valor decimal. Aspas, vírgulas e quebras de linha entre aspas são aceitas. |
| TXT/MD | Cada nota exporta como `note-ID.txt`/`note-ID.md`. Ao importar, informe esse nome; a primeira linha é o título (com `# ` opcional em MD), as demais são o texto. Não há renderização Markdown nem leitura de Word/Excel. |

Os formatos externos não levam preferências, PIN ou senha Wi-Fi. Backup JSONL
continua sendo a forma de preservar toda a base, os vínculos e as configurações.
Os novos fluxos e o desempenho perto dos limites ainda precisam de bancada.

## Prioridades 2 e 3 — 1.2.0

Em **Backup → Mais ferramentas** (ou no botão das Notas):

- Cadernos/pastas lógicas, etiquetas, favoritas e modelos de reunião/lista/diário.
- Vários rascunhos guardados, com retomada pelo título e exclusão confirmada.
- Gravador/player WAV por blocos no SD, com vínculo a nota ou compromisso.
- Desenho livre com borracha, desfazer e versões salvas como anexos.
- Central de lembretes com adiamento, sons e horário silencioso configuráveis.
- Arquivos, leitor de texto e tabelas CSV com rolagem.
- Sessão Wi-Fi com código para enviar/baixar arquivos pelo navegador e editar
  uma cópia da base no computador.
- Sincronização com comparação de três bases e resolução explícita de conflitos.
- Assistente opcional por servidor configurável, sem alterar registros antes
  da confirmação da prévia.

O [guia de conexão, anexos e assistente](docs/CONEXAO-E-ASSISTENTE.md) explica
os fluxos, configuração da ponte Python e limites. O assistente requer um
servidor/modelo; ele não roda um LLM no ESP32-P4. Microfone, qualidade do áudio,
rede/SD concorrentes e desempenho no aparelho ainda aguardam bancada.
**Backups JSONL não embutem anexos ou rascunhos guardados: copie toda a pasta
`/Tab5PDA/` para preservar também esses arquivos.**

A página de transferência vem de `assets/transfer.html`; após editá-la,
execute `python3 tools/embed_assets.py` antes de compilar. A verificação opcional
de seu JavaScript usa QuickJS (`quickjs==1.19.4`) e pode ser ativada no CMake com
`-DPDA_JS_PYTHON=/caminho/do/python-com-quickjs`; as outras suítes não dependem dele.

## Revisão corretiva — 1.2.1

Corrigidos vínculos parciais de rascunhos que podiam causar conversão numérica
inválida, retomada que sobrescrevia anexos ou prazos recorrentes atualizados,
prévia de sincronização desatualizada e reutilização de IDs já alocados no
outro lado. A sincronização exige nova prévia se a geração da base mudar.
O gravador deixa de usar um destino antigo ao abrir ferramentas sem seleção;
áudio vazio ou com falha não substitui o anexo anterior. Downloads tratam
escritas parciais e fecham a conexão quando não completam o arquivo.
