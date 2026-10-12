# 10.02: Suporte completo a alto contraste

**What to build:** Interface aplica cores do sistema em modo alto contraste Windows com todos controles visíveis e legíveis.

**Blocked by:** 10.01: Tema automático e manual com persistência

**Status:** ready-for-agent

- [ ] Detectar `SPI_GETHIGHCONTRAST` e aplicar cores do sistema
- [ ] Botões owner-draw usarem `GetSysColor` em alto contraste
- [ ] Textos e fundos com contraste mínimo 4.5:1
- [ ] Listview e controles customizados respeitarem cores do sistema
- [ ] Testar com tema alto contraste ativo no Windows
