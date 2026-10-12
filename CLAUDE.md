# Power Timer

Responda em pt-BR. Ao retomar, leia PROJECT_CONTEXT.md (estado do trabalho e "Onde parei").

## Regras
- Ações reais de energia existem (WindowsPowerBackend), mas vêm DESLIGADAS: só rodam com a configuração do mod "Executar ações reais" ligada pelo usuário. Nunca ligar nem executar ações reais em testes automáticos; testes reais só manuais e deliberados. Decisão do usuário em 2026-10-12.
- Não alterar o launcher oficial `src/windows/windhawk_tool_launcher.inc`; a identidade do mod vem do header gerado por `scripts/build-mod.ps1`.
- Fluxo: grill-with-docs → to-spec → to-tickets → implement → code-review → retro. Tickets locais em `.scratch/power-timer-v1/issues/`.
- Não commitar sem pedido do usuário; não publicar no catálogo do Windhawk.

## Testes e build (Windows)
- `powershell -File scripts/test.ps1`: scheduler, livro, armazenamento, calendário, avisos.
- `powershell -File scripts/build-mod.ps1`: gera `build/mod/crazy-time.wh.cpp` (colar no editor do Windhawk) e a DLL x86. Requer o compilador do Windhawk em `C:\Program Files\Windhawk`.
- Na nuvem (Linux) só dá para editar e rodar os testes portáteis; o build do mod e a bandeja ficam na máquina do usuário.

## Arquitetura
`src/` núcleo portátil (scheduler, calendar, schedule_book, codec v5, warning_tracker, strings pt/en); `src/windows/` host Win32 (tray_host, mod_entry, arquivo, calendário local).
