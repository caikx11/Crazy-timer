# Tarefa 12 — Roteiro de validação (Windows 11)

Ambiente: Windows 11 (registrar build em `winver`), Windhawk 1.7.x, mod `power-timer-simulation` gerado por `scripts/build-mod.ps1`. **Windows 10 não verificado** (sem máquina disponível). Tudo é simulado: nenhum teste liga/desliga o PC.

Marque `[x]` e anote observações. Se algo falhar, anote a mensagem exata/print.

## Preparação
- [ ] `git pull`, `scripts/test.ps1` e `scripts/test-executor.ps1` passam.
- [ ] `scripts/build-mod.ps1` sem erro; versão do mod: ____ .
- [ ] Colar o `.wh.cpp` no Windhawk, compilar, ativar. Ícone aparece na bandeja.

## 1. Avisos de contagem (problema aberto)
- [ ] Timer de 2 min: notificação de aviso aparece em ~1 min restante? Sim/Não: ____
- [ ] Repetir com Foco/Não perturbe desligado; depois ligado. Anotar diferença.
- [ ] Opção de som ligada e desligada: som segue a opção.
- [ ] Timer de 5 min: avisos de 5 e 1 min, uma vez cada.

## 2. Ações e capacidades (simulado)
- [ ] Timer de 1 min para Desligar/Suspender/Hibernar: notificação "intenção registrada, nenhuma ação real".
- [ ] Ocorrência fica "Concluída (simulada)" e não repete.
- [ ] Caixa "Forçar fechamento": aviso de perda de dados; "Não" desmarca; desabilitada em Suspender/Hibernar; lista mostra "(forçado)".
- [ ] Forçado persiste após fechar/reabrir o painel e reativar o mod.

## 3. Agendas
- [ ] Avulso (data/hora) e semanal (dias) criam, editam, pausam, cancelam, adiam 10 min.
- [ ] Dois horários iguais: conflito é recusado com número da outra programação.
- [ ] Mudar fuso/relógio com programação ativa: lista recalcula, nada dispara em duplicidade.

## 4. Ciclo de vida
- [ ] Desativar o mod: bandeja some, sem processo/erro. Reativar: estado restaurado; vencidas aparecem como expiradas.
- [ ] Reiniciar o Explorer (`taskkill /f /im explorer.exe` e abrir `explorer`): ícone volta, sem duplicar.
- [ ] Segundo login/sessão do mesmo usuário (se possível): apenas uma instância ativa.

## 5. Interface
- [ ] PT e EN: todas as telas legíveis, sem texto cortado (principalmente a caixa de forçar e botões da lista).
- [ ] Tema claro, escuro e alto contraste.
- [ ] DPI 100%, 150%, 200%; segundo monitor se houver.
- [ ] Navegação só por teclado (Tab/Enter/Espaço).

## Resultado
Data: ____ Build do Windows: ____ Falhas: ____
