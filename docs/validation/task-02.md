# Validação — tarefa 02 (parcial)

Data: 08/10/2026. Ambiente: Windows atual da sessão, Windhawk 1.7.3 x86, Clang 20, PowerShell 7. Nenhuma operação real de energia foi executada.

## Verificado

- `scripts/test.ps1`: **241 verificações aprovadas**. Novos casos foram escritos antes da implementação: API ausente, entrada vazia e espaços produziram falhas observadas, seguidas de correção. Limites 1/10080, sinais, decimais, lixo, dígitos Unicode, overflow e preservação do agendamento anterior cobertos.
- `scripts/build-preview.ps1`: compilação C++23 com warnings como erros aprovada.
- `scripts/build-mod.ps1`: fonte monolítica e DLL x86 compiladas para API/engine 1.7.3. Um warning de campo não inicializado vem do launcher oficial preservado; nenhuma alteração nesse snippet.
- Interface real do preview: início em 1 minuto, aviso final, edição para 0 rejeitada sem perder a contagem; fechar esconde e mantém processo; nova abertura recupera a mesma janela; conclusão simulada ocorreu escondida, sem clique de confirmação.
- Preset 15 minutos e cancelamento conferidos. Tab percorreu campo e presets; foco visível. Menu Opções abriu Configurações e Sair.
- Após correções de ciclo de vida: recompilação aprovada, preview iniciou 30 minutos, cancelou e saiu por seu menu; consulta de processos confirmou ausência de `power-timer-preview`.
- [Captura real do painel](../screenshots/task02-simulation.png), sem mockup.

Revisão intermediária de ciclo de vida identificou e corrigiu duas questões: permitir somente a mensagem inerte TaskbarCreated de Explorer menos privilegiado; encerrar o processo dedicado após limpeza quando o usuário escolhe Sair. Nova leitura do revisor não encontrou problemas concretos nessas correções. Isso não substitui a prova no engine.

## Ainda não comprovado

- Carregamento no Windhawk, callbacks de desabilitação, reativação e saída do host dedicado.
- Clique/menu diretamente no ícone e recuperação após reinício do Explorer.
- Instância única entre sessões diferentes do mesmo usuário; reabertura na mesma sessão foi observada no preview.
- Matriz Windows 10/11, DPI, tema completo, acessibilidade e idiomas das tarefas posteriores.

A UI do Windhawk está elevada e não respondeu aos inputs disponíveis. Verificação de token confirmou integridade alta; tentativa de abrir a chave de mods com permissão de criação retornou acesso negado. Nenhum registro/configuração/mod foi alterado. Elevar a execução fora do sandbox não equivale a privilégio de administrador Windows. Continuar pelo editor oficial quando o usuário estiver disponível; roteiro em [instalação local](../installation-local.md).

Tarefa 02 permanece aberta. A tarefa 03 depende da prova de integração. Nenhuma nova quarentena observada nesta etapa; não há evidência para atribuir o bloqueio administrativo atual ao antivírus.

## Revisão final da fatia local

Revisão Standards/Spec em docs/reviews/task-02.md: melhoria P3 dos atalhos corrigida e reavaliada sem novos achados; uma lacuna de integração permanece. Após a refatoração da tabela, preview e DLL recompilaram, e o atalho 2 horas foi observado preenchendo 120 minutos. O núcleo permaneceu idêntico ao que passou 241 verificações. A consulta final confirmou ausência do processo de preview; a última instância não tinha ocorrência ativa.
