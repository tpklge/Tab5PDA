# Produtividade — Tab5PDA 1.3.1

Abra **Backup → Mais ferramentas → Planilha, documentos, livros, galeria e
finanças**. O botão **Novos aplicativos** alterna entre as cinco áreas. Elas
também funcionam sem Wi-Fi; o armazenamento exige microSD disponível.

## Miniplanilha

Toque na célula, digite seu conteúdo no campo à direita e pressione **Aplicar
célula**. Há 20 linhas e oito colunas, de A1 a H20, com até 128 bytes UTF-8 por
célula. Arraste a grade para alcançar as outras linhas e colunas. **Salvar CSV**
grava o nome informado; **Abrir CSV** importa uma tabela compatível com esses
limites. A edição do campo só entra na planilha ao aplicar a célula.

Exemplos: `=A1+B1*2`, `=(A1-5)/2`, `=SUM(A1:B5)` e `=SOMA(A1:A5)`.
Números nas fórmulas usam ponto decimal. Células vazias valem zero no cálculo;
referências a texto não numérico, ciclos e divisão por zero exibem `#ERRO`.
O avaliador limita o trabalho e a profundidade de referências a 24 células.
O CSV conserva as fórmulas, além dos textos e números. Não abre XLS/XLSX.

## Documentos

Edite arquivos `.txt` e `.md` em UTF-8, com até 128 KiB. BOM UTF-8 e quebras de
linha Windows/Mac são normalizados na abertura. Markdown aparece como
texto simples. Há localizar, substituir todas as ocorrências, selecionar,
copiar, recortar, colar e desfazer até oito estados. Digitação próxima no tempo
é agrupada em um estado de desfazer. A busca diferencia maiúsculas e acentos.

Arraste para selecionar um trecho. **Copiar** sem seleção copia o documento
inteiro; **Recortar** exige seleção. **Colar** substitui a seleção ou insere no
cursor. A área de transferência e o histórico de desfazer ficam na memória
durante a sessão. **Salvar** confirma a gravação do documento no microSD.

Documentos e planilhas alterados precisam ser salvos antes de abrir ou criar
outro arquivo do mesmo tipo. Ao sair da área, bloquear ou receber um alarme,
o aplicativo tenta gravar uma cópia de recuperação. Salvar o arquivo também
escreve essa cópia antes de substituir o destino. A recuperação é retirada
após a gravação confirmada; esses nomes são reservados ao aplicativo.

A recuperação é oferecida ao voltar à área depois de reiniciar. Ela recupera
o conteúdo; confira o nome do arquivo antes de salvar. Não há gravação a cada
tecla: use **Salvar** durante o trabalho. Não abre DOC/DOCX nem formata fontes.

## Livros

**Abrir livro TXT / Markdown** lista os arquivos `.txt`/`.md` UTF-8 de até
512 KiB. Cada página contém aproximadamente 2.200 bytes, sem cortar caracteres
UTF-8; use a rolagem para ler seu conteúdo completo.

**Anterior/Próxima** salva a posição. **Marcar / desmarcar** mantém até 16
marcadores por livro; **Ir ao marcador** percorre os marcadores. Ao reabrir,
o leitor restaura a página e os marcadores do mesmo arquivo. Se o conteúdo
mudar, a posição anterior é descartada. Não abre EPUB/PDF.

## Galeria

**Abrir galeria** lista `.jpg`, `.jpeg`, `.png` e `.bmp` na pasta do aplicativo.
As extensões devem estar em minúsculas. Imagens são centralizadas e reduzidas
proporcionalmente para até 900 × 420, sem ampliação acima do tamanho original.
Os botões anterior/próxima percorrem os arquivos da pasta.

O limite é 4 MiB por arquivo e 4096 × 4096 pixels. Formatos/variantes recusados
pelo decodificador geram uma mensagem. A câmera não faz parte desta versão.
O decodificador M5GFX foi compilado para o Tab5; sua exibição ainda exige
validação física. Os testes de interface usam pixels fornecidos pela simulação.

## Finanças

**Novo lançamento** registra descrição, data pelo calendário, conta, valor
positivo, moeda, categoria, tipo (despesa/receita) e repetição mensal opcional.
A conta é um nome livre; use o mesmo nome para agrupar seus lançamentos.
As moedas usam três letras maiúsculas, como BRL/USD/EUR. Não há conversão
automática; o resumo separa conta e moeda. Os saldos exibidos são os movimentos
do mês escolhido, não um saldo bancário acumulado.

Escolha o mês em `AAAA-MM` e toque **Atualizar resumo**. **Definir orçamento
mensal** estabelece um teto por categoria/moeda no mês. Categoria vazia reúne
todas as categorias. O orçamento é comparado às despesas de todas as contas
daquela moeda; repetir a mesma categoria/moeda/mês atualiza o teto existente.

**Lançar recorrências do mês** cria os lançamentos mensais a partir das regras
registradas em meses anteriores. Não é agendado em segundo plano. O dia é
ajustado ao último dia disponível no mês, e repetir a operação não duplica
o mesmo lançamento. Meses intermediários não são preenchidos automaticamente.

Até 1.000 linhas financeiras, incluindo orçamentos e recorrências geradas.
**Exportar CSV** grava `finance-export.csv` para consulta em outro programa.
O livro é separado da aplicação **Despesas** existente; não migra nem altera
seus registros. Nesta versão, lançamentos confirmados não têm edição/exclusão
na interface; uma correção de valor pode ser registrada como contrapartida.

Um formulário interrompido por um alarme preserva seus campos. Sair para outra
área mantém o formulário na memória; **Novo lançamento** retoma o que estava
pendente. **Cancelar lançamento** descarta esse formulário. Esses campos pendentes
não sobrevivem a uma reinicialização; confirme com **Salvar lançamento**.

## Arquivos e cópias

Tudo que é persistente nestas áreas fica em **`/Tab5PDA/`**, uma pasta na raiz
do microSD. O adaptador existente converte os caminhos internos do núcleo
compartilhado para essa pasta no Tab5; nenhum desses documentos é salvo na flash.

| Arquivo | Conteúdo |
| --- | --- |
| Nome escolhido `.csv` | Planilha, incluindo fórmulas |
| Nome escolhido `.txt`/`.md` | Documento ou livro |
| `sheet-recovery.csv` / `document-recovery.txt` | Recuperação temporária de edição |
| `reading-<CRC do nome>.cfg` | Página, marcadores e identificação do conteúdo |
| `finance-a.dat` / `finance-b.dat` | Livro financeiro em gerações alternadas com checksum |
| `finance-export.csv` | Exportação financeira legível |
| `.jpg`/`.jpeg`/`.png`/`.bmp` | Imagens fornecidas pelo usuário |

O livro financeiro só substitui a cópia em memória após confirmar a escrita.
Na abertura, escolhe a geração válida mais recente; se ambas estiverem inválidas,
recusa novos lançamentos e preserva os arquivos para recuperação.
Erro de leitura de um banco também bloqueia gravações até nova abertura.
Desde 1.3.1, as novas gravações financeiras usam `PDA-LEDGER-2`, com checksum
da geração e do conteúdo. A leitura aceita o formato anterior. Depois de gravar
no formato novo, use firmware 1.3.1 ou compatível para ler a versão mais recente
do livro; o firmware 1.3.0 não lê esse formato.

Para uma cópia completa, **copie a pasta `/Tab5PDA/` inteira**: o backup JSONL
da agenda e a sincronização dos registros não incluem estes arquivos adicionais.
Textos, planilhas e imagens também podem ser enviados pela transferência Wi-Fi
existente. A tela de produtividade trabalha com os arquivos diretamente na pasta,
sem percorrer subpastas. Arquivos no cartão não são criptografados pelo aplicativo.
