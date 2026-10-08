# 04: Agendar horários e repetições semanais

**What to build:** Escolher horário local e dias da semana, consultar a próxima ocorrência e continuar a repetição após ocorrências perdidas.

**Blocked by:** 03.

**Status:** ready-for-agent
**Execution:** resolved

**Histórias da especificação:** 9, 10, 18, 32, 33.

## Critérios de aceite

- [x] Formulário inclui horário avulso futuro com data explícita e repetição diária/dias selecionados; regras e ocorrências persistem.
- [x] Viradas de semana, mês e ano produzem a próxima ocorrência correta; a regra permanece após expiração.
- [x] Avanço de relógio/fuso expira horários saltados; retorno não repete uma ocorrência tratada; testar hora inexistente e ambígua.
- [x] Duração continua usando tempo corrido; não confundir alteração civil da hora com contagem monotônica da sessão.
- [x] Apresentar estados e erros de calendário no painel e validar recuperação com casos determinísticos.
- [x] Fixar e documentar tolerância operacional dos timers e resolução de hora ambígua/inexistente, distinguindo atraso de despacho de ocorrência perdida; validar as políticas contra a especificação antes de implementar.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
- 08/10/2026: implementada em simulação, com TDD, migração V1→V2, prévia real e revisão em dois eixos. Políticas em docs/task04-contract.md e ADR 004; evidências em docs/validation/task-04.md. Instalação 0.3.0 no Windhawk pendente; a versão instalada permanece 0.1.0. Tarefa 05 disponível.
