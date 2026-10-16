# Início oculto durante recuperação do host

**Status:** ready-for-agent — escopo e limite dos testes confirmados pelo usuário em 16/10/2026.

## Problem Statement
O usuário atualizou o mod no Windhawk, reiniciou o computador e relata que o painel abriu sem erro. A fonte instalada já chama início oculto. Um ensaio no host real confirmou que a segunda instância envia uma ordem de abertura mesmo quando solicita início oculto. Não há confirmação de que esse foi o caminho ocorrido no reboot.

## Solution
Respeitar a solicitação de início oculto ao encontrar uma instância existente. Manter tentativas automáticas de criação do ícone da bandeja sem abrir o painel por falha temporária. O usuário continua podendo abrir o painel deliberadamente pela bandeja.

## User Stories
1. Como usuário, quero iniciar o Windows com o painel oculto.
2. Como usuário, quero que uma segunda inicialização automática preserve o painel oculto.
3. Como usuário, quero que a recuperação do ícone da bandeja preserve a visibilidade escolhida.
4. Como usuário, quero continuar abrindo o painel pela bandeja.
5. Como usuário, quero que uma inicialização explicitamente visível continue solicitando a abertura da instância existente.
6. Como usuário, quero que falhas da bandeja permaneçam diagnosticáveis sem abertura automática do painel.

## Implementation Decisions
- Ajustar somente as decisões de visibilidade no host Windows compartilhado.
- Condicionar a ordem de abertura da instância existente à solicitação de início visível.
- Manter mensagem de erro e nova tentativa de registro da bandeja, removendo a abertura automática por esse erro.
- Preservar o launcher oficial, o mutex de instância única e a lógica de agendamento.

## Testing Decisions
- Limite proposto: inicialização pública do host real, interceptando exclusivamente as APIs externas do Windows necessárias para reproduzir duplicidade e indisponibilidade temporária da bandeja.
- Asserções sobre ordens de abertura do painel; não usar comparação de texto-fonte como prova de comportamento.
- Dados em diretório isolado, modo simulado e nenhuma ação real de energia.
- Comprovar falha antes da correção e sucesso depois; compilar mod e preview e executar as suítes existentes.
- O ensaio determinístico não comprova o resultado de um reboot real; registrar essa limitação.

## Out of Scope
Publicação, atualização remota, alterações nos agendamentos, executor de energia e instalação silenciosa no Windhawk.

## Further Notes
Ensaio diagnóstico executado com compilador Windhawk e host real: uma inicialização oculta duplicada produziu uma ordem de abertura; esperado zero. O usuário confirmou os testes e a tarefa única.
