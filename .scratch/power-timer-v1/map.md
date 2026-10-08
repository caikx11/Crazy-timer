# Mapa das tarefas — Power Timer v1

Divisão aprovada pelo usuário em 08/10/2026. to-tickets concluída. [Especificação](spec.md) preservada.

## Execução

Tarefa 01 resolved: critérios técnicos validados, revisão em dois eixos sem achados e retrospectiva registrada. Tarefa 02 agora disponível, ainda open. Iniciar uma tarefa apenas quando suas dependências estiverem resolved. Os tickets individuais são a fonte de verdade.

As tarefas 04, 06, 10 e 11 ficam disponíveis após 03; coordenar alterações em arquivos compartilhados. Apenas 02 pode começar agora. Na criação, todas tinham Status: ready-for-agent, o que não elimina bloqueios por dependências.

| Nº | Tarefa | Bloqueada por | Estado na criação |
|---|---|---|---|
| 01 | [Tornar o ciclo de testes reproduzível](issues/01-testes-reproduziveis.md) | Nenhuma | open |
| 02 | [Agendar uma duração pelo ícone, em simulação](issues/02-icone-agendamento-simulado.md) | 01 | open |
| 03 | [Gerenciar e recuperar a lista de agendamentos](issues/03-lista-persistente-recuperacao.md) | 02 | open |
| 04 | [Agendar horários e repetições semanais](issues/04-horarios-repeticoes.md) | 03 | open |
| 05 | [Impedir programações conflitantes](issues/05-conflitos.md) | 04 | open |
| 06 | [Receber os cinco avisos e configurar som](issues/06-avisos-som.md) | 03 | open |
| 07 | [Cancelar e adiar apenas uma ocorrência](issues/07-cancelar-adiar-ocorrencia.md) | 05, 06 | open |
| 08 | [Executar energia com capacidades e erros visíveis](issues/08-energia-capacidades-erros.md) | 07 | open |
| 09 | [Permitir fechamento forçado por agendamento](issues/09-desligamento-forcado.md) | 08 | open |
| 10 | [Aplicar temas e acabamento acessível](issues/10-temas-acessibilidade.md) | 03 | open |
| 11 | [Usar o produto em português ou inglês](issues/11-idiomas.md) | 03 | open |
| 12 | [Validar o candidato completo no Windhawk e Windows 10/11](issues/12-validacao-windhawk-windows.md) | 09, 10, 11 | open |
| 13 | [Preparar demonstração e material do portfólio](issues/13-portfolio-documentacao.md) | 12 | open |

## Verificação

- 13 arquivos de tarefa, cada um com entrega, dependências e critérios.
- 40 histórias da especificação cobertas.
- Dependências apontam a números anteriores; não há ciclos.
- Nenhuma tarefa executada ou encerrada nesta etapa.
- Sem publicação remota; aprovação para GitHub/catalogo e testes reais de energia não é inferida da criação dos tickets.
