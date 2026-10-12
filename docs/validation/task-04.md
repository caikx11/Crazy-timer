# Validação — tarefa 04

Data: 08/10/2026. Escopo: horário avulso com data e repetição por dias da semana; somente simulação.

## Verificação automática

scripts/test.ps1 passou após implementação e novamente após correções de revisão:

- Agendador original: 241 verificações.
- ScheduleBook: 1230 verificações, incluindo criação/edição entre modalidades, próxima ocorrência nas viradas de semana/mês/ano e ano bissexto, continuidade após expiração/cancelamento/resultado incerto, pausa, retorno de relógio/fuso com recuperação, gap/fold e atraso de 5000/5001 ms. Falhas de gravação/calendário e overflow bloqueiam efeitos sem confirmar alterações.
- Armazenamento: 32442 verificações (inclui loops de stress), com migração V1→V2 em arquivo real, bloqueio de substituição preservando bytes antigos, recuperação de campos civis e histórico, corrupção e truncamento.
- Calendário: 23 verificações. Regras Gregorianas, UTC 1970–9999, fuso explícito Eastern para hora inexistente e ambos instantes da hora ambígua, UTC fixo, fuso atual e DST desabilitado. Nenhuma configuração do Windows foi alterada.

O número de verificações não equivale a cenários independentes. Ciclos red→green foram observados antes de implementar APIs avulsas/semanais, migração, limites, highwater obrigatório e bloqueio de adaptador que só retorna gaps.

Preview e DLL x86/fonte única 0.3.0 compilados. Preview usa warnings como erros; a fonte monolítica mantém somente o warning preexistente lpReserved do launcher oficial.

## Interface real por Computer Use

1. Recuperadas as duas durações canceladas da etapa anterior, usando dados de preview separados do mod.
2. Criada repetição com todos os dias. Cancelamento moveu próxima data de 09/10 para 10/10 e exibiu "Anterior: Cancelada". Pausa preservou a regra e a data.
3. Criado horário avulso em 09/10 com data explicitamente apresentada antes de salvar.
4. Saída pelo menu, recompilação e reabertura: avulso, repetição pausada, datas e último resultado recuperados.
5. Edição da repetição para segunda a sexta manteve quatro registros, preservou pausa e renovou a regra explicitamente. [Captura real](../screenshots/task04-calendar.png).
6. Avulso de teste cancelado; repetição de teste deixada pausada. Saída pelo menu e consulta de janelas confirmou ausência da prévia.

## Limites da evidência

Mudanças de fuso e recuperação de prazos vencidos foram ensaiadas com dependências controladas; não houve alteração do fuso real, suspensão real ou energia real. A correção da invalidação visual da lista nos eventos Windows foi revisada por leitura, não por troca real de fuso. Matriz Windows 10/11, DPI/teclado completa e integração do candidato final continuam na tarefa 12.

O mod instalado no Windhawk continua 0.1.0. A fonte 0.3.0 está pronta em build/mod/crazy-time-simulation.wh.cpp, mas o editor elevado não aceita os inputs de Computer Use disponíveis; não houve atualização da instalação. Esta evidência do preview não equivale a comprovação da nova versão no engine.
