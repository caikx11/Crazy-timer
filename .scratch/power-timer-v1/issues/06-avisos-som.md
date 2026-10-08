# 06: Receber os cinco avisos e configurar som

**What to build:** Receber avisos discretos nos marcos previstos e contagem final, mantendo execução independente da interação com avisos.

**Blocked by:** 03.

**Status:** ready-for-agent
**Execution:** open

**Histórias da especificação:** 19, 20, 21, 22, 23.

## Critérios de aceite

- [ ] Avisos em 30/15/10/5 minutos e painel nos 60s finais; somente marcos futuros, cada marco no máximo uma vez.
- [ ] Ao criar duração de 10 minutos, primeiros avisos são 5 minutos e 60 segundos; ao criar 1 minuto, abrir contagem final imediatamente.
- [ ] Ticks atrasados/repetidos e reinício não provocam avalanche ou duplicação de avisos.
- [ ] Som opcional inicialmente desligado, preferência persistente; notificação bloqueada/ignorada não altera execução.
- [ ] Tratamento genérico de ocorrências; compatibilidade com repetição/adiamento será verificada na tarefa 07.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
