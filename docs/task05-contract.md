# Contrato — conflitos

Escopo: tarefa 05, histórias 26/27. Testes na interface pública do ScheduleBook e no adaptador de armazenamento, conforme seam aprovada na spec. Apenas simulação.

## Comportamento

- Conflito significa mesmo instante UTC previsto entre agendamentos ativos, independentemente da ação e da opção de força. Para duas regras semanais, comparar interseção de dias e mesma hora local, mesmo quando próximas ocorrências iniciais forem diferentes. Não inventar janela de exclusão em torno dos horários: tolerância de despacho não transforma horários próximos em horários iguais.
- Criar, editar e reativar validam antes de gravar. Falha preserva registros, identidades e arquivo anterior; erro Conflict identifica o outro agendamento. Pausados não reservam horários; reativação deve validar. Edição explícita de uma programação pausada preserva a pausa.
- Avulso/duração contra repetição verifica a ocorrência naquela data, respeitando o limite civil já consumido, dias selecionados e primeiro instante de hora ambígua. Cancelamentos existentes contam como exceções; adiamento de 10 minutos é tarefa 07 e reutilizará essa validação quando existir.
- Inicialização, retomada, mudança de relógio/fuso e processamento de timers verificam colisões antes de entregar efeitos. Suspender os envolvidos, persistir a identificação do conflito e informar a UI; não escolher vencedor. Demais programações sem conflito podem continuar. Falha na gravação da suspensão bloqueia efeitos do processamento.
- Estado de conflito é separado de pausa voluntária. Ao editar/excluir/pausar uma das regras, recalcular os impedimentos de forma transacional. Reconciliação e reabertura devem preservar a segurança. Identificação exibida por número estável da programação, ação e próxima data quando disponível.
- A propriedade de domínio `force_shutdown` (padrão false, válida somente para Shutdown) pode ser persistida/testada para cobrir a invariância normal/forçado desta tarefa. Não há comando nem opção visual para ativá-la; interface, explicação e executor continuam na tarefa 09. A comparação de conflitos ignora esse atributo.

## Integração

Core publica cedo API e campos persistentes (proposta: `conflict_with` e `force_shutdown` anexados com defaults, BookError::Conflict e accessor para ID conflitante). O formato deve migrar V1/V2 sem perda; escritor novo versionado. Não renumerar estados existentes. Preview continua separado do mod.

Propriedade: agente core — tipos, validação, book e testes book; agente storage — codec e testes storage; parent — host/strings/scripts/docs. Coordenar alterações de contrato antes de tocar arquivo de outro responsável. Baseline de revisão: 0e02bfa4bdc5dea12ad0a45d3cb9f71463e5a620.
