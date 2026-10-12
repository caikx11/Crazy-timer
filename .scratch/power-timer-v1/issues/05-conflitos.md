# 05: Impedir programações conflitantes

**What to build:** Identificar a programação conflitante e pedir ajuste ao salvar ou reativar horários coincidentes.

**Blocked by:** 04.

**Status:** ready-for-agent
**Execution:** claimed

**Histórias da especificação:** 26, 27.

## Critérios de aceite

- [ ] Detectar mesma ação, ações distintas e normal/forçado no mesmo instante, com dias da semana sobrepostos e exceções.
- [ ] Rejeitar mudança sem perder configuração anterior, exibindo qual programação conflita.
- [ ] Fuso/reconciliação que cria colisão suspende as ocorrências conflitantes e informa o usuário, sem escolher ação automaticamente.
- [ ] Não criar dependência do relógio real nos testes; testar colisões avulsas/recorrentes e ausência de falso conflito em dias distintos.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
