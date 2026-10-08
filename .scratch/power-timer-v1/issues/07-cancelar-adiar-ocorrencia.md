# 07: Cancelar e adiar apenas uma ocorrência

**What to build:** Usar o aviso ou painel para pular ou adiar uma ocorrência sem alterar a repetição dos próximos dias.

**Blocked by:** 05, 06.

**Status:** ready-for-agent
**Execution:** open

**Histórias da especificação:** 24, 25, 26.

## Critérios de aceite

- [ ] Cancelar elimina só a ocorrência selecionada; pausar desativa a programação recorrente.
- [ ] Adiar 10 minutos preserva identidade da ocorrência e horário habitual da regra; alteração sobrevive a reinício.
- [ ] Conflito no adiamento exige escolha de horário válido e preserva o original até confirmar; não há prioridade silenciosa.
- [ ] Recalcular somente avisos futuros do novo prazo, sem duplicar avisos antigos; cancelar/adiar no limite do prazo não produz duas intenções.
- [ ] Não prometer cancelamento depois da entrega ao Windows; demonstrar todos os fluxos em simulação.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
