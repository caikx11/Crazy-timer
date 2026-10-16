# 01: Preservar início oculto na recuperação do host

**What to build:** manter o painel oculto ao iniciar uma segunda instância automaticamente e durante falha/recuperação do registro do ícone da bandeja; preservar abertura explícita pelo usuário.

**Blocked by:** None (can start immediately after scope/test confirmation).

**Status:** ready-for-agent

**Execution:** resolved

- [x] Confirmar escopo e limite dos testes com o usuário.
- [x] Inicialização oculta duplicada não solicita exibição do painel existente.
- [x] Inicialização visível duplicada continua solicitando exibição.
- [x] Falha/recuperação da bandeja não abre o painel automaticamente.
- [x] Falha da bandeja mantém diagnóstico e novas tentativas.
- [x] Testes isolados sem energia real nem dados do usuário passam.
- [x] Mod e preview compilam; revisão em padrões/especificação realizada.

## Comments
Ensaio diagnóstico da duplicidade reproduziu uma ordem indevida de abertura. Causa do reboot específico ainda não confirmada.

Validação: teste do host real passou para início oculto/visível duplicado, falha e recuperação da bandeja, tentativa periódica após 5000 ms simulados, diagnóstico e ativação explícita. Suite completa: 241 scheduler, 1291 book, 32507 storage, 23 calendar, 5 warning tracker. Mod x86 e preview compilados. Revisão Standards e Spec: zero achados pendentes. Reboot real e instalação não executados.
