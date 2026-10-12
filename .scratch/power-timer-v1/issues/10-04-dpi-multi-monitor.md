# 10.04: DPI múltiplo e posicionamento multi-monitor

**What to build:** Painel se adapta corretamente a DPI 100/150/200% sem sobreposição e posiciona-se em área visível em múltiplos monitores.

**Blocked by:** 10.01: Tema automático e manual com persistência

**Status:** ready-for-agent

- [ ] Testar `px()` scaling em DPI 100/150/200%
- [ ] Verificar sobreposição de controles em escalas diferentes
- [ ] Posicionar janela em área visível considerando múltiplos monitores
- [ ] `WM_DPICHANGED` atualizando corretamente fonts e layout
- [ ] `AdjustWindowRectExForDpi` aplicado consistentemente
