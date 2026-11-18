# Interface e calculadoras — Tab5PDA 1.4.0

## Formulários e teclado

O contorno quadrado que aparecia no fim dos valores era o cursor do LVGL.
Agora ele é uma linha fina e aparece apenas no campo com foco. Tocar em outro
campo transfere o foco; os demais valores ficam sem cursor.

Títulos, observações e descrições recebem inicial maiúscula. Nomes de contatos,
aniversários, empresas, locais, contas e cidades recebem inicial maiúscula em
cada palavra, incluindo letras acentuadas: `joão da ávila` vira `João Da Ávila`.
As demais letras não são convertidas para minúsculas, preservando siglas e
grafias como `NASA` e `McDonald`. Moedas são convertidas para maiúsculas.

E-mails, senhas, SSIDs, nomes de arquivos, fórmulas e o editor de documentos
mantêm a grafia digitada. Alterar um cadastro não muda a caixa desses campos.
A capitalização dos cadastros acontece durante a edição; abrir um registro
apenas para leitura não regrava seus dados.

Com o **teclado físico detectado**, tocar em campos não abre o teclado virtual.
Conectar o teclado oculta o virtual e devolve a altura completa ao formulário.
Se o teclado for desconectado durante a edição, o virtual aparece no campo
ativo, preservando o texto. Sem teclado físico, o teclado virtual abre por toque.
A detecção usa o estado do driver I2C existente; consulte **Diagnóstico** para
verificar se o teclado foi reconhecido.

Os formulários continuam roláveis quando o teclado virtual está aberto.
`Ctrl+S` salva cadastros, ajustes, documentos ou planilhas, conforme a área ativa.

## Leitura e temas

Contatos, agenda, tarefas, notas, aniversários e despesas usam título maior,
rótulos em negrito e valores distribuídos pela ficha. Observações e endereços
são justificados na largura disponível; a última linha fica alinhada à esquerda.
Essa apresentação não altera o texto guardado no microSD.

**Ajustes** abre diretamente o formulário. Em **Tema visual**, escolha:

| Tema | Aparência |
| --- | --- |
| Sereno | Fundo claro, azul e verde suaves |
| Areia | Fundo creme, tons de oliva e marrom |
| Ardósia | Fundo escuro, texto claro e azul acinzentado |

Toque **Salvar** para aplicar e guardar o tema no microSD. O formulário de
ajustes continua disponível após salvar. **Cancelar** sai sem salvar as mudanças.
Os testes conferem contraste mínimo de 4,5:1 para as combinações principais de
texto/fundo das paletas. A reprodução de cores depende do painel físico.

## Calculadora

A calculadora tem três modos. A expressão é digitada no teclado e o resultado
aparece em um visor maior. Não há painel de dígitos nem botões das quatro
operações básicas. Sem teclado físico, toque na expressão e use **Enter** no
teclado virtual; com o físico, pressione **Enter**.

### Básica

Aceita `+`, `-`, `*`, `/`, parênteses, percentual e decimais com ponto ou vírgula.
Exemplos: `2+3*4` resulta em `14`; `200*10%` resulta em `20`.
A expressão e a memória da calculadora são mantidas durante a sessão.

### Científica

Oferece botões para seno, cosseno, tangente, raiz quadrada, quadrado, inverso,
logaritmos natural/decimal, exponencial, fatorial e constantes π/e, além de
memória M+/M-/MR/MC. O botão aplica a função à expressão atual e calcula o resultado.
**Graus (DEG)** ou **Radianos (RAD)** define a unidade das funções trigonométricas.

Também aceita expressões como `sqrt(81)+log(100)`, `sin(90)`, `2^3^2` e `fact(6)`.
Nomes das funções podem ser digitados em maiúsculas ou minúsculas. Potenciação é
associativa à direita; `-2^2` vale `-4`. Expressões têm limite de 256 bytes e
profundidade limitada. Divisão por zero, resultados não finitos e valores fora
do domínio geram uma mensagem, preservando a entrada.

### Financeira

Calcula uma incógnita do valor do dinheiro no tempo, usando os outros campos:

| Campo | Significado |
| --- | --- |
| n | Número de períodos |
| Taxa | Percentual por período |
| VP | Valor presente |
| PMT | Parcela constante por período |
| VF | Valor futuro |

Preencha os valores conhecidos e use o botão **Calcular** correspondente à
incógnita. Selecione se a parcela ocorre no início ou no fim do período.
Use sinais opostos para entradas e saídas. Taxa e quantidade de períodos
precisam usar a mesma frequência; não há conversão automática de taxa anual
para mensal, nem inclusão automática de impostos, tarifas ou calendário.

Exemplo de operação: VP `10000`, n `12`, taxa `1` por período, VF `0` e pagamento
no fim do período produzem PMT de aproximadamente `-888,49`. O campo calculado
preserva mais casas para os cálculos seguintes; o visor monetário mostra duas.

Taxa zero é tratada separadamente. O cálculo de taxa exige uma única mudança
de sinal na sequência dos fluxos não nulos VP/PMT/VF; fluxos ambíguos ou sem
solução são recusados. A busca é limitada ao intervalo acima de -100% até
10.000% por período. Valores, quantidade de períodos e resultados têm limites
numéricos; a interface informa quando eles são ultrapassados.

## Conversor, relógios e Wi-Fi

O conversor oferece seletores de **Grandeza**, **Unidade de origem** e **Unidade
de destino**, um campo de valor e o botão **Inverter unidades**. Há comprimento,
massa, volume, temperatura e moedas. Para moedas diferentes, informe uma taxa
manual de unidades de destino por unidade de origem. Inverter troca as unidades
e usa a taxa recíproca; converter uma moeda para ela mesma preserva o valor.
Não há cotações obtidas pela internet.

Relógios mostra três cartões com a cidade acima da hora digital em 48 px, data
e offset UTC abaixo. **Editar cidades e fusos** configura os nomes e offsets.
A atualização segue a hora local do dispositivo; o horário de verão continua
sendo ajustado manualmente nos offsets.

Wi-Fi mostra a rede salva e um status compacto, seguidos dos comandos de busca,
conexão, desconexão e sincronização. As redes aparecem em linhas com nome,
intensidade do sinal e indicação de proteção. Selecione uma rede para informar
a senha e **Salvar e conectar**.

## Arquivos e compatibilidade

Os cadastros continuam na base existente em `/Tab5PDA/`; o tema é uma preferência
da mesma base, gravada no microSD. Os cálculos e a memória da calculadora são
temporários e não geram lançamentos financeiros automaticamente.

A versão 1.4.0 lê as bases anteriores. Bases salvas com a preferência de tema
exigem firmware que reconheça essa preferência; versões anteriores do Tab5PDA
podem recusá-las. Preserve uma cópia da pasta do cartão para voltar a uma versão
antiga. O formato do livro financeiro separado não foi alterado nesta versão.

[Capturas do simulador](previews/) · [Validação](VALIDACAO.md)
