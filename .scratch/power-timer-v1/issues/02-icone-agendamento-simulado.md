# 02: Agendar uma duração pelo ícone, em simulação

**What to build:** Abrir um painel pelo ícone, escolher uma ação e duração, acompanhar e cancelar uma ocorrência simulada.

**Blocked by:** 01.

**Status:** ready-for-agent
**Execution:** open

**Histórias da especificação:** 2, 3, 5, 7, 8, 20, 23, 24, 39.

## Critérios de aceite

- [ ] Janela própria acessível por clique e menu direito; fechar esconde a janela e mantém a contagem.
- [ ] Aceitar 1–10080 minutos e atalhos 15/30/60/120, mostrando ação e horário previsto; rejeitar entrada inválida preservando estado.
- [ ] Contagem final e cancelamento atuam até entrega da intenção; intenção simulada ocorre no prazo uma única vez, sem exigir clique.
- [ ] Host inicial integrado como tool mod na versão estável validada do Windhawk, uma instância por usuário; saída/desabilitação remove ícone, timers e processamento. Reinício do Explorer recria só o ícone. Comprovar essa integração cedo, antes de ampliar funcionalidades.
- [ ] UI identifica claramente simulação. Teclado básico, foco e catálogo de textos preparados desde esta fatia; acabamento completo nas tarefas próprias.

## Comments

- 08/10/2026: divisão, critérios e dependências aprovados pelo usuário. Tarefa publicada localmente; execução ainda não iniciada.
