# 04: Agendar horários e repetições semanais

**What to build:** Escolher horário local e dias da semana, consultar a próxima ocorrência e continuar a repetição após ocorrências perdidas.

**Blocked by:** 03.

**Status:** ready-for-agent
**Execution:** claimed

**Histórias da especificação:** 9, 10, 18, 32, 33.

## Critérios de aceite

- [ ] Formulário inclui horário avulso futuro com data explícita e repetição diária/dias selecionados; regras e ocorrências persistem.
- [ ] Viradas de semana, mês e ano produzem a próxima ocorrência correta; a regra permanece após expiração.
- [ ] Avanço de relógio/fuso expira horários saltados; retorno não repete uma ocorrência tratada; testar hora inexistente e ambígua.
- [ ] Duração continua usando tempo corrido; não confundir alteração civil da hora com contagem monotônica da sessão.
- [ ] Apresentar estados e erros de calendário no painel e validar recuperação com casos determinísticos.
- [ ] Fixar e documentar tolerância operacional dos timers e resolução de hora ambígua/inexistente, distinguindo atraso de despacho de ocorrência perdida; validar as políticas contra a especificação antes de implementar.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
