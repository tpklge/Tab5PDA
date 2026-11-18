# Tab5PDA 1.2.0 — cadernos, anexos e computador

Abra **Backup → Mais ferramentas** ou **Notas → Mais ferramentas**.
Tudo continua em `/Tab5PDA/`, na raiz do microSD. Nenhum servidor é aberto
antes de uma ação explícita na tela.

## Cadernos e rascunhos

No formulário de nota, informe **Caderno / pasta**, **Etiquetas** e **Favorita**.
Os cadernos são agrupamentos lógicos da base; não criam diretórios no FAT.
A busca consulta também esses campos. Em **Cadernos**, escolha a pasta,
as favoritas ou um modelo de reunião, lista ou diário.

**Guardar rascunho** salva uma cópia independente e libera o rascunho automático
para outro documento. Ao criar um registro com edição pendente, a recuperação
agora também oferece **Guardar rascunho e continuar**. Toque no título do
rascunho em Cadernos para retomá-lo. Ao salvar o registro, a cópia retomada é
removida; as outras permanecem. Excluir uma cópia pede confirmação.
O rascunho automático após 1,5 segundo continua existindo para a edição ativa.
Rascunhos guardados usam snapshots com CRC; dados inválidos ou cujo registro
original foi removido não são aplicados silenciosamente.

## Voz e desenhos

No detalhe de nota ou compromisso, **Áudio** abre o gravador com esse registro
como destino. Toque **Iniciar gravação**, depois **Parar e salvar áudio**.
O WAV usa PCM mono de 16 bits a 16 kHz, até aproximadamente cinco minutos,
com escrita por blocos no SD. Finalização confere o cabeçalho e o tamanho
por releitura. O player aceita esse formato simples, não MP3 nem todo WAV.
O driver utiliza um buffer por vez; continuidade e qualidade precisam de
medição no Tab5 real. Uma falha de SD é informada; arquivo parcial pode
permanecer para recuperação manual e não é confirmado como anexo válido.

A gravação concluída é vinculada ao registro de origem. O vínculo aponta ao
áudio mais recente; os WAV anteriores continuam no cartão. Para usar um áudio
recebido, abra as ferramentas a partir da nota/compromisso de destino, vá a
Arquivos, abra o WAV e toque **Vincular arquivo atual**. Arquivo avulso pode
ser reproduzido sem vínculo. Bloquear encerra a captura salvando o trecho
confirmado. Avisos com som interrompem o áudio; avisos silenciosos não o fazem.

**Desenhar** abre uma área de 900 × 420 com preto, azul, borracha e desfazer.
Até 20.000 pontos e 2.048 traços por desenho. **Salvar desenho** cria uma nova
versão `.draw`; se há registro de destino, atualiza o vínculo após a escrita
verificada. Versões anteriores ficam disponíveis em Arquivos. Ao sair com
alterações, o app tenta salvar; falha de gravação impede a saída normal.
Lembretes e bloqueio preservam o desenho em andamento. Não há reconhecimento
de escrita, PDF, PNG nem suporte a pressão de caneta nesta entrega.

## Lembretes e arquivos

Na central, configure adiamento de 1–1440 minutos, silêncio de/até `HH:MM` e
som Silencioso/Suave/Padrão/Agudo. Campos de horário vazios ou iguais desligam
o intervalo silencioso. O intervalo pode atravessar a meia-noite; os avisos
visuais continuam. A lista mostra até 64 avisos pendentes; tocar abre as ações
de dispensar/adiar. O botão de adiamento usa o valor salvo.

Arquivos navega apenas pela pasta do PDA e por extensões conhecidas. Há páginas
de 16 itens; a enumeração do cartão tem teto de 4.096 entradas. TXT, Markdown,
ICS, VCF e JSONL têm prévia de texto até 32 KiB. CSV tem tabela com até 100
linhas de dados e 12 colunas, com rolagem em ambos os eixos; não calcula fórmulas.
Desenhos e áudio têm visualizadores próprios. A leitura de arquivos de texto
usa o limite de 4 MiB do adaptador. Não é um navegador geral da flash ou do SD.

**Backup JSONL inclui registros e seus vínculos, mas não embute WAV, desenhos,
rascunhos guardados ou `assistant.cfg`. Para backup completo, copie toda a
pasta `/Tab5PDA/`**. A lixeira não apaga os anexos automaticamente. Remover um
arquivo na área Arquivos exige confirmação e pode deixar um vínculo sem arquivo.

## Transferência pela rede local

1. Conecte ao Wi-Fi na aplicação Wi-Fi/hora.
2. Abra **Mais ferramentas → Transferência Wi-Fi → Abrir sessão Wi-Fi**.
3. No computador da mesma rede, abra o endereço `http://IP-DO-TAB5:8080` mostrado
   e digite o código da sessão. O código é novo a cada abertura.
4. Liste/baixe arquivos ou envie um arquivo. Upload tem teto de 10 MiB por arquivo.
   O servidor grava `.part` e só publica `incoming-TEMPO-NOME` após conferir o
   tamanho e concluir a renomeação. Não substitui o arquivo original nem a base.
5. Importe o recebido em Backup ou abra-o na área Arquivos. Para sincronização,
   use o nome completo exibido ao final do envio.

Os endpoints de listagem, download e upload exigem `X-PDA-Token`. A página
inicial não revela dados pessoais. Estado interno, rascunhos guardados,
configuração do assistente e base auxiliar de sincronização não são publicados.
Backups JSONL são arquivos pessoais e podem incluir PIN e senha Wi-Fi.
A sessão termina ao bloquear, perder o Wi-Fi ou tocar Encerrar sessão.
A comunicação é HTTP para rede local confiável; não exponha a porta na Internet.
O servidor usa uma tarefa separada e acesso serializado ao SD. Reabrir o cartão
é recusado enquanto há uma transferência em curso. A validação física ainda
precisa verificar concorrência com captura, avisos e recuperação após queda.

## Edição e sincronização

Abrir a sessão prepara `sync-export.jsonl` sem preferências, PIN, senha Wi-Fi,
rascunho ativo ou estados de lembretes. No navegador, **Abrir base para edição**
confere o checksum e permite editar título/texto/data dos registros, criar notas
e tarefas e mover itens para a lixeira da cópia. **Enviar cópia para revisão**
recalcula o checksum e envia um arquivo preparado. Nada é aplicado ao PDA nessa
etapa. Mudanças ainda não enviadas ficam só na página; não feche a aba antes
de enviar se quiser mantê-las.

No Tab5, abra **Sincronização**, informe o arquivo `incoming-...` e prepare a
prévia. A comparação usa a base atual, o recebido e `sync-base.jsonl` da última
sincronização aplicada. Alteração de um lado é combinada; mudança concorrente
ou colisão de ID pede **Manter local / Usar recebido**. Na primeira sincronização
não há ancestral e diferenças com o mesmo ID são tratadas como conflitos.
Resolva todos e toque **Aplicar sincronização**. As configurações locais são
preservadas. A gravação dos registros é uma transação; falhas não aplicam parte
da base. O ancestral e a nova exportação são atualizados em seguida. Se essa
segunda etapa falhar, o aviso distingue os registros já salvos da base auxiliar
não atualizada; uma nova tentativa pode exigir revisão de conflitos novamente.
Se o arquivo auxiliar ficar inválido após interrupção, preserve a pasta e
restaure ou remova apenas `sync-base.jsonl` pelo computador/cartão. Sem ancestral,
a próxima prévia trata diferenças com o mesmo ID como conflitos explícitos.

Salve ou guarde o rascunho ativo antes de sincronizar. Há um ancestral para o
par de trabalho, sem identidade de dispositivo ou sincronização automática de
múltiplos computadores. Exclusões são recuperáveis na lixeira e incluem
subtarefas; alterações locais em descendentes geram conflito antes da exclusão.
Anexos são arquivos separados: transfira-os à parte e vincule o nome recebido
pelo player/desenho ou mantenha os nomes ao copiar diretamente pelo SD.
Não há CalDAV, contatos de nuvem, e-mail ou sincronização automática em segundo plano.

## Assistente opcional

O firmware é um cliente; o modelo roda em um computador/servidor. A ponte
`tools/assistant_bridge.py` (também em `dist/`) usa apenas Python 3 padrão.
É necessário ter um modelo instalado em um servidor de inferência, informando
seu nome e endpoint. Nenhum modelo ou serviço externo foi instalado/ativado
automaticamente nesta entrega.

Para Ollama, com o servidor de modelos já funcionando:

```sh
python3 tools/assistant_bridge.py --listen 0.0.0.0 --model NOME-DO-SEU-MODELO
```

A ponte imprime um código de acesso. No Tab5 → Assistente, informe
`http://IP-DO-COMPUTADOR:8787/assistant` e esse código. Toque **Salvar servidor e
código** para persistir em `assistant.cfg` ou use a configuração só nesta sessão.
Digite um pedido e toque **Gerar prévia**. Só o texto digitado e a hora local
são enviados, não a agenda inteira nem as configurações. Para resumir uma nota,
inclua seu texto no pedido. A conversa é por pedidos independentes, sem histórico
persistente e sem entrada/saída de voz.

O pedido tem teto de 4.096 bytes. A resposta pode propor até 16 notas, tarefas,
compromissos ou despesas; uma resposta textual/resumo vira uma proposta de nota.
A ponte e o firmware validam os tipos/campos e o JSONL com checksum. A prévia
não altera registros. **Confirmar importação** os acrescenta; cancelar descarta
a proposta. Exclusões, vínculos e anexos vindos do assistente são recusados.
Solicitações têm timeout e o bloqueio descarta resultados ainda não aplicados.

O modo Ollama usa `/api/chat`, JSON e resposta sem streaming, conforme a
[documentação do projeto](https://github.com/ollama/ollama/blob/main/docs/api.md).
Para outro servidor que implemente chat completions compatível:

```sh
python3 tools/assistant_bridge.py --listen 0.0.0.0 --model NOME-DO-SEU-MODELO --protocol chat-completions --endpoint https://SEU-SERVIDOR/v1/chat/completions
```

`PDA_ASSISTANT_TOKEN` fixa o código local se desejado; `PDA_UPSTREAM_KEY` fornece
uma chave ao servidor de inferência. A chave do provedor fica no ambiente do
computador, sem ser enviada ao Tab5. A ponte verifica TLS nos endpoints HTTPS;
a conexão do Tab5 à ponte continua HTTP local. A ponte autentica cada pedido e
não registra prompts/códigos em logs de acesso. Não publique a porta 8787.
Os testes exercitam uma requisição HTTP real até um modelo simulado; qualidade,
latência e compatibilidade com um modelo real dependem da configuração escolhida.
