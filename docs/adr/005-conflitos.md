# ADR 005 — Programações conflitantes

Data: 08/10/2026. Status: implementação sob revisão.

## Definição

Conflito representa o mesmo instante previsto entre programações ativas. Não depende da ação ou de fechamento forçado. Regras semanais também conflitam quando compartilham dia e hora em uma ocorrência futura, mesmo que suas primeiras datas sejam diferentes. Pausados não reservam horários; terminais não participam.

Avulso e duração são comparados à ocorrência efetiva da repetição naquela data, respeitando dias selecionados, limite civil consumido e a escolha do primeiro instante em hora ambígua. Cancelar uma ocorrência permite usar seu horário sem apagar a regra seguinte. A tolerância de despacho de cinco segundos não constitui uma janela de conflito.

## Comandos e processamento

Criar, editar e reativar produzem um candidato, validam conflitos e só então persistem. Rejeição retorna o número do outro agendamento e mantém os dados anteriores. A interface exibe números estáveis para relacionar mensagem e lista.

Reconciliação e timers detectam colisões antes de entregar intenções. Todos os envolvidos recebem uma referência de conflito persistida; nenhuma ação é escolhida como vencedora. A marca é separada de pausa voluntária. Resolver o impedimento recalcula as referências de forma transacional. Se não for possível salvar o bloqueio, não há entrega de efeitos.

## Persistência e escopo

Formato POWER_TIMER 3 acrescenta conflict_with e force_shutdown. Leitura de V1/V2 atribui defaults, mantendo migração atômica em uma gravação posterior. A referência de conflito pode apontar a um ID anterior ausente (mas nunca a si própria ou além do contador), permitindo recuperar e recalcular vínculos antigos.

force_shutdown é uma propriedade de domínio padrão false, válida somente em desligamento. Sua presença permite testar que normal e forçado não contornam conflitos. Nenhuma opção visual, comando de ativação ou executor de energia foi adicionado; isso continua na tarefa 09. Adiamento continua na tarefa 07 e deverá passar pela mesma validação quando implementado.

Toda validação é simulada. Dados, regras de relógio e tratamento de resultados incertos das etapas anteriores permanecem aplicáveis.
