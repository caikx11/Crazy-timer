# 03: Gerenciar e recuperar a lista de agendamentos

**What to build:** Criar, editar, pausar, reativar e excluir várias durações e reencontrá-las após fechar e reabrir o host.

**Blocked by:** 02.

**Status:** ready-for-agent
**Execution:** resolved

**Histórias da especificação:** 11, 12, 13, 14, 15, 16, 17.

## Critérios de aceite

- [x] Lista reflete as alterações de ponta a ponta e mostra próxima ocorrência/estado.
- [x] Persistência atômica/versionada; gravação que falha não aparece como salva; dados inválidos não disparam ações nem são sobrescritos silenciosamente.
- [x] Suspensão/reinício conta como tempo corrido; recuperar antes do prazo preserva a ocorrência e depois do prazo a expira.
- [x] Consumir intenção uma única vez e persistir resultado incerto para não reenviar ao reiniciar; simular falhas entre gravação e entrega.
- [x] Reconciliar inicialização e retomada antes de processar timers; comprovar com relógio/armazenamento falsos.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
- 08/10/2026: concluída em simulação, com aceite em revisão. Evidências: docs/validation/task-03.md; revisão: docs/reviews/task-03.md; retrospectiva: docs/retro/task-03.md. Testes públicos, arquivo real e preview cobrem os critérios; retomada e falhas usam dependências controladas. A instalação Windhawk 0.2.0 permanece pendente por falta de controle do editor elevado e não é considerada validada.
