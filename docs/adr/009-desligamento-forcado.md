# ADR 009 — Desligamento forçado por programação

Data: 12/10/2026. Status: implementado em simulação (tarefa 09). Nenhuma chamada real ao Windows existe nesta versão.

## Decisão

O fechamento forçado é uma escolha explícita **por programação** (`force_shutdown`), desmarcada em programações novas. Não existe preferência global.

- Só vale para Desligar. Escolher Suspender ou Hibernar limpa a opção; voltar a Desligar não a reativa (o núcleo e a interface aplicam a regra).
- Marcar a opção exige confirmação com aviso de perda de dados (padrão "Não"). A lista mostra "(forçado)" na ação.
- A entrega (`Delivery.force`) leva a escolha ao executor: `PowerBackend::request(action, force)`. `execute_power_action` só repassa `force` para Desligar, mesmo que o valor vaze para outra ação.
- O forçado não participa de conflitos: o conflito depende apenas do instante (ADR 005).
- Persistência: campo `force_shutdown` do formato POWER_TIMER 3 em diante.

## Limites reais do Windows (a validar em ensaio manual)

Estes pontos descrevem o comportamento esperado das APIs e **não foram verificados nesta versão**; um futuro executor real precisa testá-los de propósito.

- Desligamento normal pode ser adiado ou bloqueado por aplicativos (por exemplo, com motivo de bloqueio de desligamento) e por outras sessões de usuário. Forçar fecha aplicativos sem pedir para salvar; o trabalho não salvo se perde.
- Forçar não garante o desligamento: serviços, política do sistema, atualizações pendentes ou falha de privilégio ainda podem impedir. Pedido aceito não prova desligamento concluído (ADR do executor, tarefa 08).
- Suspensão e hibernação não têm modo forçado equivalente; o sistema ou drivers podem recusar. Hibernação depende de estar habilitada; em Modern Standby a suspensão tradicional pode não existir.
- O privilégio de desligamento é habilitado temporariamente e deve ser restaurado em todos os caminhos.

## Consequências

Uma falha ou recusa deixa a ocorrência incerta, sem repetição automática, e não cancela a próxima repetição semanal. Ações reais continuam proibidas fora de teste manual deliberado.
