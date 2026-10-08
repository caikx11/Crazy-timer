# 01: Tornar o ciclo de testes reproduzível

**What to build:** Compilar e executar o harness existente com resultado verificável, preservando o isolamento das ações reais de energia.

**Blocked by:** Nenhuma.

**Status:** ready-for-agent
**Execution:** resolved

**Estado atual:** concluída. Ciclo de testes validado, revisão independente sem achados nos dois eixos e retrospectiva registrada. Evidências: docs/validation/task-01.md, docs/reviews/task-01.md e docs/retro/task-01.md. Base vazia e primeira versão preservadas em Git local.

**Histórias da especificação:** Preparação do harness e prefactoring mínimo.

## Critérios de aceite

- [x] Diagnosticar e corrigir Acesso negado com evidência, sem desabilitar proteções do Windows por conveniência; se o ambiente bloquear, documentar o impedimento e validar em ambiente apropriado autorizado.
- [x] Uma execução limpa deve compilar e executar testes; uma asserção intencionalmente falha deve tornar o comando malsucedido, sendo depois restaurada.
- [x] Testes não vinculam executor real de energia; registrar comando, ambiente e resultado reproduzíveis.
- [x] Fazer apenas o prefactoring necessário para preservar a interface de tempo/intenção existente; não antecipar funcionalidades.

## Comments

- 08/10/2026: revisão contra base vazia aprovada concluída em dois subagentes GPT-6.1 Sol/high: Standards 0, Spec 0. Nenhuma correção de código necessária. Retrospectiva registrada. Tarefa resolvida; 02 desbloqueada.

- 08/10/2026: após reinicialização informada pelo usuário, três compilações no mesmo caminho funcionaram: suíte original 130/exit 0; expectativa intencionalmente falsa detectada/exit 1; fonte restaurada byte a byte e suíte 130/exit 0. Fonte e imports PE conferidos, sem APIs de energia. Critérios técnicos atendidos; causa específica do bloqueio anterior não comprovada. Revisão/commit pendentes: base inicial e autoria solicitadas. Ver docs/validation/task-01.md.

- 08/10/2026: diagnóstico de acesso aprofundado, sem novas compilações. CreateFileW/OPEN_EXISTING com compartilhamento read/write/delete abre scheduler_test.exe para leitura, mas nega GENERIC_WRITE e DELETE com WinError 5; nenhum conteúdo alterado. Mesmo resultado fora do sandbox sob CAIO_PC\caio_. ACL permite FullControl ao usuário; atributo 0x20 (Archive). FileStandardInfo retornou DeletePending=False, um link, 1510912 bytes. Arquivo de texto temporário na mesma pasta foi criado, sobrescrito, relido e excluído com sucesso. scripts/test.ps1 abre para escrita; antiga .runner.lock nega leitura/escrita/exclusão. Restart Manager fora do sandbox não reportou processo comum usando o executável; consulta dentro do sandbox falhou com código 29 (não usar essa falha como prova de ausência de bloqueio). Diagnóstico confirma restrição específica no executável, mas não identifica o componente responsável. Próximo passo depende do usuário: salvar trabalho e reiniciar Windows, mantendo proteção ativada; depois repetir ./scripts/test.ps1 no mesmo caminho, sem mudar flags ou criar variantes. Reinicialização é teste diagnóstico, não correção garantida. Ainda faltam compilação completa e asserção intencionalmente falsa. Tarefa 01 ready-for-human/open; 02 bloqueada. Nenhuma proteção/ACL alterada, nenhuma operação real de energia, nenhum processo deixado em andamento. Recomendação Astra/high.

- 08/10/2026: arquivo existente no caminho fixo passou 130 verificações novamente. Runner passou a mutex nomeado (lockfile estava inacessível); teste com processo filho confirmou serialização. Linker ainda não pode sobrescrever scheduler_test.exe, mesmo fora do sandbox. Restart Manager não identificou processo segurando arquivo; causa dessa negativa não comprovada. Nenhuma mudança de proteção/ACL. Tarefa incompleta.

- 08/10/2026: após restauração, o build c81 executou 130 verificações com código zero. Nova compilação bloqueada. Padronizada saída build/tests/scheduler_test.exe com lock; verificados recusa de execução concorrente, liberação após erro e sintaxe. Build no caminho fixo compila, mas não inicia; sem metadado correspondente nos oito registros recentes consultados. Nenhuma proteção alterada; tarefa permanece incompleta.

- 08/10/2026: usuário pediu nova tentativa e aviso em caso de quarentena. Mesmo executável ausente; metadado agora confirma quarentena desse build, Gen:Variant.Application.Fragtor.851, 16:01:11. Usuário avisado; sem recompilar, executar ou restaurar. Tarefa permanece needs-info/open.

- 08/10/2026: após mudança relatada pelo usuário, nova compilação passou, mas execução falhou dentro/fora do sandbox. Restart Manager identificou VSSERV/PID 2988 usando o novo binário. Nenhum teste ou asserção falha executado; serviço não alterado. Tarefa retorna a needs-info/open.

- 08/10/2026: Acesso negado reproduzido. Metadado de quarentena aponta para o scheduler_test.exe desta sessão, detecção Gen:Variant.Application.Fragtor.851. Nenhuma proteção alterada. Script melhorado para separar erro ao iniciar de falha de teste; sintaxe PowerShell validada. Suíte e asserção intencionalmente falha ainda não executadas; tarefa não concluída. Diagnóstico detalhado no documento de diagnóstico do harness. WSL não instalado e Docker não encontrado. Pergunta sobre próximo ambiente de validação enviada ao usuário; arquivo liberado de claimed enquanto aguarda decisão externa.

- 08/10/2026: tarefa assumida pelo agente principal na etapa implement; reproduzir bloqueio antes de alterar o harness.

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.


Retentativa controlada em 08/10/2026: desativação temporária do Bitdefender informada pelo usuário; mesmo runner/caminho falhou dentro e fora do sandbox ao sobrescrever o executável (Permission denied, exit 1). Não chegou aos testes. Causa atual continua não comprovada; tarefa permanece needs-info/open. Detalhes em docs/test-harness-diagnosis.md.
