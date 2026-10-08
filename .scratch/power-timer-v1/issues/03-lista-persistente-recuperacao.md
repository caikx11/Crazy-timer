# 03: Gerenciar e recuperar a lista de agendamentos

**What to build:** Criar, editar, pausar, reativar e excluir várias durações e reencontrá-las após fechar e reabrir o host.

**Blocked by:** 02.

**Status:** ready-for-agent
**Execution:** claimed

**Histórias da especificação:** 11, 12, 13, 14, 15, 16, 17.

## Critérios de aceite

- [ ] Lista reflete as alterações de ponta a ponta e mostra próxima ocorrência/estado.
- [ ] Persistência atômica/versionada; gravação que falha não aparece como salva; dados inválidos não disparam ações nem são sobrescritos silenciosamente.
- [ ] Suspensão/reinício conta como tempo corrido; recuperar antes do prazo preserva a ocorrência e depois do prazo a expira.
- [ ] Consumir intenção uma única vez e persistir resultado incerto para não reenviar ao reiniciar; simular falhas entre gravação e entrega.
- [ ] Reconciliar inicialização e retomada antes de processar timers; comprovar com relógio/armazenamento falsos.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
