# 08: Executar energia com capacidades e erros visíveis

**What to build:** Executar desligamento normal, suspensão ou hibernação pelo mesmo fluxo validado, explicando indisponibilidade e falhas.

**Blocked by:** 07.

**Status:** ready-for-agent
**Execution:** open

**Histórias da especificação:** 5, 6, 28, 30, 31.

## Critérios de aceite

- [ ] Consultar capacidades ao programar e antes de executar, incluindo Modern Standby; indisponibilidade aparece com explicação.
- [ ] Privilégios temporários tratados/restaurados; checar falha parcial ao habilitar privilégio e propagar erro real.
- [ ] Desligamento normal não força aplicativos; suspensão/hibernação preservam eventos de despertar e não habilitam recursos do sistema automaticamente.
- [ ] Registrar intenção duravelmente antes da chamada; solicitação aceita não aparece como desligamento comprovadamente concluído.
- [ ] Falha não repete automaticamente a ocorrência e não cancela a próxima repetição; validar funções Windows falsas e crash/restart.
- [ ] Alvo simulado continua fisicamente isolado do executor real. Ações reais só em ensaio manual deliberado.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
