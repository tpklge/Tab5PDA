# Validação — Tab5PDA 1.4.0

## Interface e calculadoras 1.4.0 — 18/11/2026

**Onze suítes do Tab5 passaram**, incluindo a nova
`presentation_science_finance_and_names`; os executáveis C++ usam ASan/UBSan.
As quatro suítes do Cardputer passaram após a adição da preferência `theme`
ao núcleo compartilhado. Há **62 capturas** do LVGL real em 1280 × 720.

Verificado no simulador e nos modelos:

- Cursor transparente sem foco e borda esquerda fina no foco, sem contorno
  em todos os valores. Capitalização de nomes e iniciais com acentos, preservando
  siglas, e-mails, SSIDs, senhas e o editor de documentos.
- Teclado físico simulado: tocar no campo não abre o virtual; desconectar abre
  o virtual no campo ativo; reconectar oculta o virtual sem perder o texto.
  Enter do teclado físico e do virtual executa a calculadora.
- Fichas com título em 28 px, rótulos em fonte negrito e observações justificadas.
  A verificação de rolagem continua conferindo que um gesto não troca a seleção.
- Ajustes diretamente editáveis; seleção, gravação e releitura dos três temas.
  Cores de fundo/texto dos seletores e fundo da tela são conferidas após trocar
  entre Sereno, Areia e Ardósia. Contraste das combinações principais >= 4,5:1.
- Calculadora sem botões de dígitos ou quatro operações básicas. Modos distintos,
  trigonometria em graus/radianos, potência, fatorial, logaritmos, constantes,
  memória e erros de domínio. Aceita vírgula decimal e nomes de funções em ambas
  as caixas. Os testes recusam aninhamento excessivo e resultados não finitos.
- Financeira: VP, PMT, VF, n e taxa; parcelas no início/fim, taxa zero, sinais e
  casos indeterminados. **500 casos de conferência** calculam VF e recuperam VP
  e taxa, além de exemplos com parcelas e empréstimos.
- Conversor com seletores reais, conversão de temperatura/comprimento, taxa
  manual, inversão de unidades/taxa e identidade ao selecionar a mesma moeda.
- Relógios digitais em 48 px e cidade acima do horário; Wi-Fi com status compacto
  e redes selecionáveis. As telas foram inspecionadas nas capturas geradas.

Build ESP32-P4: **54.712 bytes de RAM estática**, **1.994.064 bytes de flash**.
Aplicação: **2.150.368 bytes**; factory: **2.215.904 bytes**. Imagens e SHA256
estão em `dist/`, com fingerprint dos fontes e versão 1.4.0 no manifesto.
A análise estática dos arquivos de modelo/interface alterados não emitiu novos diagnósticos.
O [guia](INTERFACE-E-CALCULADORAS.md) descreve operação, limites numéricos e
compatibilidade da nova preferência de tema com firmwares antigos.

Não houve upload em aparelho. Detecção/desconexão física do teclado I2C, latência
de cálculo no P4 e contraste/cores do painel real continuam pendentes de bancada.
O simulador usa o driver de teclado simulado e executa o LVGL real; ele não
emula o ESP32-P4 nem valida eletricamente o conector.

## Preparação do repositório — 18/11/2026

A publicação inclui `lib/PDACommon` com cópia dos mesmos fontes e headers do
núcleo compartilhado. PlatformIO, CMake e o empacotador usam a biblioteca local;
não há dependência de pastas irmãs no clone. O build ESP32-P4 e as onze suítes
foram executados novamente com essa disposição. Binários ficam nos assets da
release; fontes, guias, capturas, checksums e manifestos ficam no Git.

## Revisão corretiva 1.3.1 — 17/11/2026

**Dez suítes do Tab5 passaram**, com ASan/UBSan nos executáveis C++.
As quatro suítes do Cardputer também passaram após a extensão compatível da
API de leitura do núcleo compartilhado. O analisador estático do Clang foi
executado sobre os oito arquivos C++ de modelos/interface desta aplicação,
sem diagnósticos adicionais. Esses resultados cobrem os cenários abaixo;
não demonstram ausência de outros defeitos.

Falhas corrigidas e regressões exercitadas no simulador LVGL:

- **Limites de digitação:** o teclado virtual usava 96 bytes fora do editor
  original, e a paleta de acentos usava 128. Documentos e pedidos ao assistente
  paravam antes do limite. Os três caminhos de entrada consultam o limite real
  do campo. Teste digita texto/acentos após 200 bytes e recusa exceder 128 KiB.
- **Documentos grandes:** inicializar um campo com limite já ativo fazia o LVGL
  inserir o texto caractere por caractere, recalculando o layout. Agora a carga
  é feita de uma vez. Coordenadas de 32 bits evitam truncar a altura de textos
  longos. Teste abre e salva exatamente 128 KiB, com 21.845 quebras de linha,
  verifica o conteúdo integral e a altura do texto.
- **Teclado sobre campos:** as áreas novas não tinham contêiner de formulário
  redimensionável. Agora o campo ativo fica em uma área rolável acima do teclado;
  fechar o teclado restaura sua altura.
- **Interrupções:** um alarme recriava os campos financeiros com valores padrão
  e descartava a fórmula ainda não aplicada. Os valores e seleções são capturados
  e retomados. Testes disparam e dispensam alarmes durante ambos os formulários.
- **Recuperação de documento vazio:** retornar da busca após apagar o texto podia
  recarregar uma cópia antiga. A recuperação é consultada só uma vez por sessão.
  O teste apaga todo o texto, abre a busca, volta e confirma que continua vazio.
- **Arquivos de outros sistemas:** textos/CSV UTF-8 com BOM ou CRLF eram recusados
  ou carregavam BOM na primeira célula. A abertura normaliza BOM e quebras de linha.
- **CSV:** a última linha sem terminador escapava dos limites de colunas/tamanho;
  a prévia podia cortar no meio de um caractere UTF-8. Os limites agora incluem
  essa linha e a prévia preserva a fronteira de caracteres.
- **Eventos atrasados:** comandos de salvar repetidos podiam acessar os campos
  de uma tela que já havia fechado. A interface valida tela e quantidade de campos
  antes de executar eventos de finanças, documentos, ajustes, lembretes,
  assistente e confirmação de importação. Há regressões para eventos repetidos.
- **Leitura financeira:** erro de I/O não é tratado como arquivo ausente, mesmo
  se a listagem do diretório não estiver disponível. Um banco ilegível bloqueia
  gravações até nova abertura; nenhum lançamento é exportado sobre erro de leitura.
- **Integridade financeira:** a geração não era protegida pelo checksum. Novas
  gravações usam `PDA-LEDGER-2`, com CRC da geração e do CSV. Teste altera apenas
  o número da geração e verifica a recuperação da outra cópia válida. Leitura de
  `PDA-LEDGER-1` e sua atualização na gravação seguinte também foram verificadas.
- **Galeria:** abrir uma imagem por Arquivos enviava seu conteúdo binário ao
  visualizador de texto. Agora abre a galeria. Buffers RGB565 de tamanho incorreto
  são recusados e o cache é invalidado antes da troca de pixels. O teste alterna
  repetidamente duas imagens simuladas com cores distintas.

A nova suíte `malformed_inputs_and_boundaries` usa uma semente fixa para
**5.000 entradas malformadas/fórmulas/CSVs**, **200 textos UTF-8 paginados** e
limites financeiros de quantidade/IDs. Arquivos inválidos não substituem
planilhas ou livros financeiros já carregados. Há **48 capturas** do LVGL real.

Build ESP32-P4: **54.456 bytes de RAM estática**, **1.880.134 bytes de flash**.
Aplicação: **2.033.536 bytes**; factory: **2.099.072 bytes**. Pacote 1.3.1 em
`dist/`, com SHA256 e fingerprint dos fontes. Os binários e o manifesto 1.3.0
foram preservados.

Não houve upload no dispositivo. O simulador executa o LVGL real, os modelos e
o armazenamento com injeção de falhas; não emula o ESP32-P4 nem o cartão físico.
Imagens usam um decodificador simulado: o M5GFX real, seleção por gesto no touch,
tempo/consumo no P4 e cortes de energia no microSD continuam exigindo teste físico.

Reprodução:

```sh
cmake --build tests/build -j8
ctest --test-dir tests/build --output-on-failure
pio run -e tab5
python3 tools/package_firmware.py
```

O diretório CMake deste ambiente está configurado com ASan/UBSan e com
`PDA_JS_PYTHON` apontando para a instalação local de Python/QuickJS.

## Produtividade 1.3.0 — 17/11/2026

**Nove suítes passaram**, com ASan/UBSan nos executáveis C++. A suíte nova
`productivity_sheet_reader_finance` verifica fórmulas, referências inválidas,
ciclos, profundidade combinada de referências/parênteses, divisão por zero, CSV com aspas, limites de célula, paginação UTF-8,
recorrências mensais idempotentes, ajuste de fim de mês, totais por conta/moeda,
validação de entradas e limites de cabeçalhos de imagens.

A suíte do LVGL percorre os cinco aplicativos: grava e relê a planilha CSV,
substitui texto acentuado, desfaz e salva o documento, abre livro, grava página
e marcador, reabre em outra instância da UI e confere a retomada. A galeria usa
um decodificador simulado que fornece pixels RGB565. Nas finanças, registra
despesa, recorrência e orçamento, exporta CSV e simula escrita truncada no banco
inativo; outra instância recupera a geração válida sem o lançamento incompleto.
Também recusa nomes reservados à recuperação. Há **45 capturas** do LVGL real.

Build final ESP32-P4: **54.392 bytes de RAM estática**, **1.889.582 bytes de flash**.
Aplicação: **2.043.024 bytes**; factory: **2.108.560 bytes**. Imagens e checksums
foram gerados em `dist/`, versão 1.3.0, com fingerprint dos fontes no manifesto.

Não houve gravação em aparelho. O decodificador M5GFX real, cores e desempenho
de JPEG/PNG/BMP, gestos de seleção de texto, uso dos limites máximos no P4 e
recuperação após corte de energia no cartão físico permanecem pendentes.
O [guia](PRODUTIVIDADE.md) descreve limites e os arquivos auxiliares, que exigem
cópia da pasta completa para backup.


## Revisão corretiva 1.2.1 — 17/11/2026

Oito suítes passaram com os novos testes de regressão. Build ESP32-P4:
**50.112 bytes de RAM estática**, **1.814.368 bytes de flash**. Aplicação:
**1.959.408 bytes**; factory: **2.024.944 bytes**, com checksums no manifesto.

Falhas corrigidas na revisão:

- **Rascunhos incompletos:** conversão sem checagem de IDs parciais podia lançar
  exceção ou exceder `unsigned long` de 32 bits no dispositivo. A coleta de
  dependências agora faz parsing limitado sem exceção; o texto incompleto é
  preservado no rascunho. Testados texto não numérico e valores acima de UINT32.
- **Retomada de rascunhos:** campos de áudio/desenho antigos podiam desfazer um
  vínculo atualizado. A retomada mantém os anexos atuais e a conclusão atual;
  prazos de tarefa avançados após arquivamento são preservados quando o prazo
  não havia sido editado no rascunho. Texto em edição continua preservado.
- **Sincronização desatualizada:** avisos, ajustes ou conclusão de áudio entre
  prévia e confirmação podiam ser sobrescritos pela cópia antiga. Aplicar agora
  compara a geração da base e exige refazer a prévia se houve alteração. O teste
  de interface modifica um ajuste durante a prévia e confere sua retenção.
- **IDs:** a combinação preserva o maior contador entre local, remoto e ancestral,
  inclusive quando IDs remotos já foram apagados, evitando sua reutilização.
- **Destino de gravação:** abrir ferramentas sem um registro selecionado limpa
  o destino anterior. Teste por comandos da interface confere gravação avulsa
  sem alteração do anexo da nota anterior.
- **Finalização de áudio:** gravações vazias ou com falha de captura/escrita não
  são publicadas como novo anexo. Prefixos recuperáveis podem permanecer no SD.
- **Downloads:** tratamento de escritas parciais e encerramento de conexão em
  leitura/envio incompleto, para o navegador não receber sucesso aparente.

A revisão não altera o esquema nem reduz os limites. Alterações de áudio e
transporte foram compiladas para o P4; a validação física desses drivers continua
pendente. Não houve gravação em aparelho. A revisão foi focada nos fluxos acima;
não representa garantia de ausência de outros defeitos.

## 1.2.0 — prioridades 2 e 3, 17/11/2026

Build ESP32-P4: **50.096 bytes de RAM estática**, **1.813.786 bytes de flash**.
Aplicação: **1.958.880 bytes**; factory: **2.024.416 bytes**. O pacote 1.2.0 e
seus checksums/fingerprint estão em `dist/`; inclui a ponte Python do assistente.
Não houve upload em aparelho nem instalação/ativação de modelo de IA.

**Oito suítes passaram**: as cinco anteriores, workspace (cadernos/desenhos/
sincronização), ponte HTTP e JavaScript da página de transferência. Os cinco
executáveis C++ usam ASan/UBSan. A suíte JavaScript é opcional no CMake, ativada
neste ambiente com `PDA_JS_PYTHON` apontando a Python com QuickJS 1.19.4.
As quatro suítes do Cardputer passaram com o núcleo compartilhado atualizado.
Há **39 capturas** do LVGL real em 1280 × 720.

Verificado no computador:

- Metadados de notas, favoritos, etiquetas e cadernos; rascunho independente
  com CRC, retomada, associação original e retenção após falhas. Patch de anexo
  preserva o rascunho automático em vez de concluí-lo inadvertidamente.
- Dois rascunhos guardados/retomada por toque; desenho por ponteiro com vários
  pontos, salvamento e vínculo; tela do gravador e retorno de anexo por uma
  plataforma simulada. A simulação não captura som.
- Cabeçalho e tamanho do PCM/WAV usados pelo driver; canais/formato/tamanho
  inválidos recusados. Desenho com coordenadas e quantidade de pontos inválidas
  recusado. Horário silencioso atravessando meia-noite, limite e adiamento.
- Leitor CSV real do LVGL, configurações de lembretes por toque e bloqueio
  encerrando as funções externas na plataforma simulada.
- Comparação entre base local, recebida e ancestral: mudanças independentes,
  conflito concorrente, colisão de ID, exclusão, descendentes locais alterados,
  resolução e preservação de credenciais locais. Exportação de sincronização
  não inclui PIN/senha Wi-Fi. Aplicação por prévia na interface.
- Ponte HTTP Python autenticada até um servidor de modelo simulado: pedido,
  hora local, limite, formato, validação de propostas e rejeição de acesso sem
  código. O JSONL resultante também é lido pelo núcleo C++ real.
- JavaScript **extraído da página embarcada**, executado em QuickJS com DOM/fetch
  simulados: checksum UTF-8, abertura da base, edição de nota, criação de tarefa,
  exclusão recuperável e envio de JSONL válido. Não é um ensaio em navegador real
  conectado ao ESP32.

Os adaptadores M5 de gravação/reprodução e o servidor WebServer compilam para
P4. A implementação usa tarefa de rede separada, mutex no SD, recebimento em
`.part`, publicação após conferir tamanho, autenticação por código e geração
por sessão para interromper transporte anterior ao fechar/reabrir. Essa
inspeção e o build não comprovam transferência no hardware.

**Bancada pendente:** iniciar/parar captura e reprodução no Tab5, verificar
amostras, continuidade e qualidade, esgotamento/remoção do SD, corte de energia,
limites de cinco minutos e concorrência com Wi-Fi/avisos. Validar a página em
navegador real, cancelamento de uploads/downloads, perda de rede, bloqueio,
retomada e remount. Exercitar um modelo real com a ponte escolhida; qualidade,
latência, limites de contexto e configuração do endpoint não foram homologados.

JSONL não embute áudio, desenhos, rascunhos guardados ou configuração do
assistente: preservar **toda a pasta `/Tab5PDA/`** antes de atualizar. Dados novos
exigem firmware compatível; o retorno à versão anterior usa a cópia prévia.
O [guia de uso e configuração](CONEXAO-E-ASSISTENTE.md) documenta os limites.

## 1.1.0 — prioridade 1 e capacidade ampliada, 17/11/2026

Firmware ESP32-P4 compilado: **46.816 bytes de RAM estática**, **1.661.026 bytes
de flash**. Aplicação: **1.787.232 bytes**; factory: **1.852.768 bytes**.
As imagens e seus SHA256 estão em `dist/`, com manifesto e fingerprint dos
fontes. A compilação dos novos arquivos de aplicação passou; os headers do
LVGL podem emitir o aviso já conhecido de operações entre enums do SDK.
Nenhum aparelho foi gravado.

Passaram **cinco suítes** no Tab5: recuperação/regras, agenda/migração/backup,
protocolo USB, interface por toque e prioridade/capacidade. Os quatro
executáveis C++ usam ASan/UBSan. As **quatro suítes existentes do Cardputer**
também passaram após as mudanças no núcleo compartilhado, mantendo o perfil
pequeno sem definir `PDA_TARGET_TAB5`.

Verificações adicionais:

- Snapshot e JSONL com 1.024 registros; nota de 16 KiB; rejeição de registro,
  nota ou texto agregado acima dos limites; base íntegra após falha no SD.
- Hoje interativo, conclusão por caixa, criação de subtarefa herdando projeto,
  lista de subtarefas e conclusão por toque.
- Grade diária/semanal, seleção de evento, prévia e confirmação de reagendamento,
  duração preservada, ocorrência isolada com exceção na série e conversão de fuso.
- Tarefas recorrentes mensais em dia 31, bloqueio da conclusão com subtarefa
  pendente, ciclos recusados e reabertura dos itens da próxima ocorrência.
- Exclusão recuperável, grupo de subtarefas, preservação de exclusão anterior,
  restauração após reinício simulado e exclusão definitiva sem vínculo órfão.
- Backup diário idempotente, data inválida recusada, nomes de arquivo restritos
  à pasta da aplicação e verificação após escrita.
- Round-trip de ICS (UTC, recorrência, exceção, aviso e dia inteiro), VCF UTF-8
  e CSV com vírgulas, aspas e múltiplas linhas; formatos inválidos recusados.
  Importação acrescenta em uma transação e falha sem alteração parcial.
- Interface de prévia/importação CSV por toque. Paginação da lista com 1.024
  contatos e seleção do contato 1.024 em formulário, sem criar milhares de
  widgets simultaneamente. O transporte Python aceita payload acima de 32 KiB.

Há **30 capturas** LVGL em `docs/previews/`, incluindo Hoje interativo, agenda
diária/semanal, subtarefas, lixeira e prévia de importação.

Pendências físicas: memória/latência com dados próximos de 1 MiB, PSRAM usada
pelo heap padrão, transferências USB grandes, velocidade de escrita/backup no
cartão real, alimentação durante gravação e ergonomia dos novos controles.
O teste no computador não mede autonomia nem comprova recuperação de cortes de
energia no cartão físico. A base continua residente em RAM; não há journal ou
carregamento incremental do SD nesta versão.

Antes de atualizar, copie `/Tab5PDA/`. Bases ampliadas ou campos novos não devem
ser restaurados no firmware antigo do Tab5 ou no CardputerPDA 1.0.0. A leitura
dos dados antigos foi preservada; o retorno ao firmware anterior exige a cópia
prévia do cartão.

## Histórico 1.0.4

Implementação e verificações no computador em **13/10/2026**. Fonte 16 px,
17 menus visíveis e formulários completos seguem as instruções do usuário.

## Seletores por toque — 17/11/2026

Implementação adicional compilada para Tab5: **41.920 bytes de RAM estática**
e **1.435.792 bytes de flash**, conforme PlatformIO. As quatro suítes passaram.
O build continua emitindo o aviso de conversão entre enums no header do LVGL.

As novas capturas `date-selector.png` e `option-selector.png` são do LVGL real
em 1280 × 720. Os testes acionam os botões por coordenadas de toque e verificam
seleção de dia, retenção do minuto escolhido após mudar o mês, 31/01 → 29/02,
mudança para ano não bissexto, cancelamento sem alterar o campo, fim baseado
no início, recorrência diária, data limite sem hora, limpeza de campo opcional,
conversão BR para ISO e releitura do snapshot após reinicialização simulada.

A inspeção do adaptador confirmou criação de `/Tab5PDA/` na raiz do microSD e
mapeamento dos caminhos compartilhados para essa pasta, inclusive backup USB.
Não foi necessário alterar o armazenamento nem migrar os dados.
Os seletores novos ainda aguardam validação no Tab5 físico. Os números e o
manifesto da distribuição 1.0.4 abaixo se referem ao pacote anterior.

## Computador

Build final: **41.824 bytes de RAM estática** e **1.427.836 bytes de flash**.
As imagens possuem **1.501.968 bytes** (aplicação) e
**1.567.504 bytes** (factory); checksums constam no manifesto.
O build passou, com avisos apenas nos headers de uma dependência LVGL;
a compilação dos testes/código de aplicação não produziu avisos.
A heap LVGL e os buffers usam PSRAM no dispositivo, sem ocupar esse total
de RAM estática. Há **22 capturas** geradas pelo LVGL real.

- Build PlatformIO para ESP32-P4, com aplicação e imagem factory.
- Quatro suítes: regras/recuperação, migração/agenda/backup, protocolo USB e
  touch/formulários completos. Os três executáveis C++ usam ASan/UBSan.
- A suíte touch usa ponteiro real do LVGL: press/release, coordenadas de botões,
  tecla do teclado virtual e gesto de rolagem. Confere fonte/limites da tela,
  menus, todos os campos antes de abrir teclado, redução/restauração da área e cursor visível em texto longo,
  Shift/símbolos/acentos, CRUD, rascunho, busca, totais, PIN/timeout, aviso durante
  edição e retenção após falha de gravação.
- O núcleo usa fixture independente do esquema 1, arquivos corrompidos/truncados,
  falhas de confirmação e restauração. O utilitário USB é testado com transporte
  falso e fixture JSONL exportado pelo C++ real.
- As capturas são feitas pelo LVGL em 1280 × 720; não comprovam drivers físicos.
- NTP, RTC e hash do PIN usam substitutos na suíte de UI; suas implementações
  reais são compiladas com o SDK e precisam de teste no dispositivo.

## Bancada pendente

| Revisão de painel | Boot/tela | Touch | SD/RTC/áudio/C6 |
| --- | --- | --- | --- |
| ILI9881C + GT911 | Pendente | Pendente | Pendente |
| ST7123 | Pendente | Pendente | Pendente |
| ST7121 | Pendente | Pendente | Pendente |

O alvo atual é P4 ES anterior à revisão 300. A revisão de CPU também precisa
ser conferida; cobertura de painel e cobertura de silício são requisitos distintos.

1. Conferir perfil/CPU/PSRAM no diagnóstico, tela 1280 × 720 e acentos a 16 px.
2. Tocar nos cantos e nas 17 aplicações; conferir alinhamento em paisagem.
3. Abrir contatos/agenda/notas/despesas: todos os campos devem aparecer juntos.
   Tocar no último campo, abrir teclado e conferir foco visível; fechar deve
   restaurar toda a área com os mesmos valores. Testar símbolos, Shift e acentos.
4. Arrastar listas/observações; o gesto não deve selecionar nem salvar registros.
5. Criar, editar, excluir, reiniciar; verificar persistência e rascunho. Testar
   SD ausente, cheio, removido e reinserido, preservando o texto em caso de erro.
6. Ajustar hora/fuso; conferir RX8130CE em UTC, retenção e tensão baixa. Sem
   fonte de hora válida, agenda continua disponível e lembretes por data não disparam.
7. Conferir som/volume zero, antecipação, dispensar/adiar, reinício, relógio
   avançado/retrocedido, tela apagada e aviso durante edição sem perda de campos.
8. Testar C6/NTP com rede, sem rede e coprocessador incompatível; interface deve
   continuar ativa. Conferir timeout, desligamento do rádio e aplicação da hora.
9. Criar backup no Cardputer 1.0.0, restaurar no Tab5, exportar de volta e restaurar
   no Cardputer. Comparar IDs, campos, rascunhos, PIN e estados de lembretes.
10. Testar USB get/put, checksum, interrupção e confirmação na tela. Nenhum envio
    deve substituir a base automaticamente. Conferir PIN errado/correto, timeout
    e bloqueio da exportação. Arquivos do SD permanecem sem criptografia.
11. Medir heap interno/PSRAM, latência, consumo e uso prolongado com base cheia.
    Corte real de energia/FAT e slots de launchers ainda não foram homologados.

Nenhum upload ou teste físico foi realizado nesta implementação.

## Correção de orientação e resíduos — 1.0.1

O usuário executou a 1.0.0 no Tab5 e enviou fotos da interface: orientação
oposta ao teclado físico e riscos na borda que somem com os redesenhos.
A revisão do painel não foi identificada pelas fotos.

A 1.0.1 usa rotação 3, mantendo 1280 × 720 e a transformação de touch do SDK.
O flush usa `pushImage` com pixels RGB565 tipados, em vez de janela + `writePixels`.
No M5GFX 0.2.32, `Panel_FrameBufferBase::writePixels` registra a região suja
antes da troca de eixos e usa extremos invertidos em certas rotações;
`writeImage` transforma a região antes de registrá-la para o writeback de cache.
A tela também é limpa na inicialização e o primeiro quadro LVGL é redesenhado
por completo. O núcleo e os dados no microSD não foram alterados.

Pendente no aparelho: reiniciar várias vezes, verificar a borda inferior,
alternar aplicações, tocar nos quatro cantos e usar o teclado touch.
Os testes no computador não validam o cache/DSI nem a orientação física.

## Teclado físico — 1.0.2

A 1.0.1 tinha apenas entrada touch. A 1.0.2 configura o conector do Tab5
Keyboard no barramento I²C externo GPIO0/1, endereço 0x6D, modo HID.
O barramento interno do M5Unified permanece em seu próprio controlador.
A leitura ocorre no loop principal, sem callbacks LVGL em outra tarefa,
com até oito eventos por ciclo e reconexão quando o periférico reaparece.
O diagnóstico informa presença; nenhuma tecla/credencial é registrada na serial.

Quatro suítes passaram, incluindo novos testes com relatórios HID para letras,
Aa/Shift, Sym/pontuação, releases, setas, Backspace/Delete, Tab/Shift+Tab
na ordem visual dos campos, formulário completo sem teclado touch e PIN por
digitação física + Enter. O build ESP32-P4 passou e ambas as imagens foram
geradas. A comunicação I²C e o teclado físico ainda precisam ser confirmados
no aparelho: digitar em contatos/notas, alternar campos, Ctrl+S, reconectar
o teclado e verificar wake da tela sem inserir a tecla de despertar.

Referências primárias: documentação M5Stack Tab5 Keyboard e firmware oficial
`M5Tab5-Keyboard-Internal-FW`, arquivos `user_keyboard_handle.c` (HID
pressionado/solto, Aa/Sym/modificadores) e `user_i2c_callback.c` (FIFO/registradores).

## Máscaras e conflito de rascunhos — 1.0.3

Quatro suítes passaram com ASan/UBSan nos executáveis C++. A suíte LVGL verifica
a digitação numérica por HID e teclado touch, inserção automática de separadores,
rejeição de letras, Backspace até campo vazio, dinheiro com vírgula, rejeição da
terceira casa e de separador duplicado, armazenamento com duas casas decimais e
recusa de 30/02 ao salvar. Campos numéricos de ajustes, PIN e registros têm
listas de caracteres aceitos e continuam usando as validações do núcleo.

A recuperação verifica despesa pendente ao solicitar um novo contato, retomada
da despesa original e descarte explícito seguido da abertura do contato. Também
verifica contato pendente ao solicitar despesa, retomando os campos e o nome do
contato original. O esquema mantém um único rascunho global; a tela agora mostra
o tipo real, a escolha solicitada e a ação explícita de descarte/criação.

Não foi alterado código do PDACommon/Cardputer nem o esquema de persistência.
O build ESP32-P4 passou e as imagens possuem checksums no manifesto. Foram
atualizadas as 18 capturas do LVGL real. Pendente no Tab5: editar datas no meio
do texto, limpar/preencher valores pelos dois teclados, conferir campos de
ajustes e retomar/descartar rascunhos de tipos distintos após reiniciar.

## Controles, Wi-Fi e máscaras brasileiras — 1.0.4

Ajustes: sliders de brilho/volume; dropdowns para timeout, formato ISO/BR,
fuso UTC em horas inteiras e timeout do bloqueio. Os três fusos dos relógios
world também usam horas. A persistência continua expressa em minutos; não
houve mudança de esquema. Valores antigos de timeout fora das opções comuns
são mantidos como opção adicional. Fusos fracionários são truncados para a
hora inteira exibida e só gravados assim ao salvar os ajustes.

A suíte LVGL verifica slider acionado por touch, alteração física com setas,
Tab entre controles, abertura real do dropdown de fuso e salvamento/preservação
de um timeout personalizado de 20 s. Confere que ajustes sem alterar hora
não acionam o relógio, e a conversão dos fusos world para minutos.

Máscaras BR: entrada de data/hora por dígitos, recorrência só com data, valores
com vírgula, salvamento em ISO/decimal canônico e reabertura no formato BR.
O modo ISO continua coberto pelos testes anteriores. O filtro de totais
permanece AAAA-MM/AAAA.

Wi-Fi: worker C6/SDIO separado da UI para busca, conexão, desconexão e NTP.
Resultados são publicados via fase atômica com release/acquire; o worker não
chama LVGL/SD/RTC. A busca é limitada a 32 SSIDs únicos e o estado da conexão
é exibido com IP. Conexão manual mantém o rádio ativo; desconectar desliga.
Conexão/NTP têm limite de tentativa de 30 s, além da inicialização interna
do SDK; a busca usa timeout de 10 s após inicialização.

A suíte usa uma plataforma simulada para conferir seleção do SSID, gravação
da senha e comando de conexão, desconexão, recusa durante operação ocupada,
rede aberta sem senha e retenção de texto quando o estado muda. Comunicação
real C6/SDIO, sinais/RSSI, senha errada, scan vazio, perda de conexão e NTP
ainda requerem teste no aparelho. O código real foi compilado para ESP32-P4.

As 22 capturas são imagens do LVGL real; sliders/dropdowns e telas de Wi-Fi
foram inspecionados visualmente. O hardware permanece pendente para estas
alterações. As correções de orientação, teclado e rascunhos foram preservadas.
