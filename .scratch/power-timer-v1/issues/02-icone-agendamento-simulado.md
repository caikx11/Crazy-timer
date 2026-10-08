# 02: Agendar uma duração pelo ícone, em simulação

**What to build:** Abrir um painel pelo ícone, escolher uma ação e duração, acompanhar e cancelar uma ocorrência simulada.

**Blocked by:** 01.

**Status:** ready-for-human
**Execution:** open

**Histórias da especificação:** 2, 3, 5, 7, 8, 20, 23, 24, 39.

## Critérios de aceite

- [ ] Janela própria acessível por clique e menu direito; fechar esconde a janela e mantém a contagem.
- [x] Aceitar 1–10080 minutos e atalhos 15/30/60/120, mostrando ação e horário previsto; rejeitar entrada inválida preservando estado.
- [x] Contagem final e cancelamento atuam até entrega da intenção; intenção simulada ocorre no prazo uma única vez, sem exigir clique.
- [ ] Host inicial integrado como tool mod na versão estável validada do Windhawk, uma instância por usuário; saída/desabilitação remove ícone, timers e processamento. Reinício do Explorer recria só o ícone. Comprovar essa integração cedo, antes de ampliar funcionalidades.
- [x] UI identifica claramente simulação. Teclado básico, foco e catálogo de textos preparados desde esta fatia; acabamento completo nas tarefas próprias.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.

- 08/10/2026: implementação local e preview verificados, 241 verificações aprovadas. Critérios de duração/contagem/UI atendidos nesta fatia; clique/menu no ícone e integração real ainda pendentes. Detalhes em docs/validation/task-02.md. Windhawk elevado exige interação humana; nenhuma configuração foi alterada. Tarefa permanece aberta e bloqueia 03.
