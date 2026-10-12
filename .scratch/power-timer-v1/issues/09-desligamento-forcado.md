# 09: Permitir fechamento forçado por agendamento

**What to build:** Ativar conscientemente o desligamento forçado em uma programação específica, mantendo as demais no modo normal.

**Blocked by:** 08.

**Status:** ready-for-agent
**Execution:** done (simulado; ADR 009)

**Histórias da especificação:** 29.

## Critérios de aceite

- [ ] Opção desmarcada em novas programações, aviso claro de perda de dados e indicador na lista/resumo.
- [ ] Escolher suspensão/hibernação remove a aplicação da opção; retornar a desligamento não a ativa implicitamente.
- [ ] Persistir a escolha individual, sem preferência global que force outras programações.
- [ ] Testar configuração enviada ao executor falso, diferenças normal/forçado e conflito de horários; documentar limites reais do Windows.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
