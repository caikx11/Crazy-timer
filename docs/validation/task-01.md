# Validação da tarefa 01 — ciclo de testes

Data: 08/10/2026. Executada após o usuário informar que reiniciou o Windows.

## Resultado

O comando `./scripts/test.ps1` voltou a compilar e executar no caminho fixo `build/tests/scheduler_test.exe`, dentro do sandbox, sem mudanças de código, flags, ACL ou proteção nesta tentativa. O bloqueio anterior deixou de reproduzir após a reinicialização; o componente responsável pela negativa de sobrescrita continua não identificado. Não foi verificado o estado atual da proteção; não presumir que o sucesso prova algo sobre sua configuração.

1. Compilação integral e execução: `PASS: 130 deterministic checks; no power APIs linked.`, exit 0.
2. Alterada temporariamente apenas a primeira expectativa de `State::Idle` para `State::Requested`: compilação passou; execução retornou `FAIL line 15: timer.state() == State::Requested`; runner retornou `Testes falharam: 1`, exit 1.
3. Fonte restaurada em `finally`, conferida byte a byte; nova compilação integral e execução: 130 verificações aprovadas, exit 0.

Não houve exclusão do executável antes de compilar: isso confirma que a sobrescrita no mesmo destino voltou a funcionar. O runner compila a fonte inteira a cada execução, sem cache de objetos.

## Isolamento de energia

Inspeção completa de `src/scheduler.hpp`, `tests/scheduler_test.cpp` e `scripts/test.ps1`: alvo contém apenas núcleo C++ puro e teste; não há executor Windows, chamadas de energia, carregamento dinâmico ou comandos de energia. O agendador emite valores de enumeração, sem executar a ação.

Inspeção da tabela de importação PE do executável final: formato PE32, 143 imports nomeados, apenas `KERNEL32.dll` e `msvcrt.dll`. Nenhum import direto de SetSuspendState, ExitWindowsEx, InitiateShutdown, InitiateSystemShutdown ou NtShutdownSystem. A tabela de imports sozinha não exclui chamadas dinâmicas; a inspeção da fonte completa confirma o isolamento deste alvo.

A primeira tentativa do leitor de PE assumiu PE32+ e abortou antes de inspecionar os imports. O leitor foi ajustado para reconhecer ambos os formatos e a inspeção acima concluiu com exit 0. Isso foi ajuste do diagnóstico, sem alteração do produto.

## Identificação do estado validado

SHA-256:

- `src/scheduler.hpp`: `25174DA88B8DAC016A287B43F0F03BAD0B515B6A1D32A7219DDCF1EFA00228BA`
- `tests/scheduler_test.cpp`: `91BF0C113DA0F7807E78D4FCACED61B0D8D36FD514CB9674D5DA5DDA22AE9B53`
- `scripts/test.ps1`: `30A5869270F8BDA2779191A540516FFD472D81453B249472CC3589508CCA4CBE`
- `build/tests/scheduler_test.exe`: `7160F3F3FA4BDDE9D7D41CEB0D846C98DA644A50CEEE1047BCC33251C0E54F8A`

Compilador usado pelo runner: `C:/Program Files/Windhawk/Compiler/bin/clang++.exe`; flags C++23, Wall, Wextra, Werror, pedantic, static. Não inferir que o teste PE32 valida o futuro mod de outra arquitetura nem a matriz Windows 10/11.

## Limites e pendências

Os 130 checks validam o núcleo experimental existente, não todas as funcionalidades do produto. A tolerância experimental de cinco segundos não é requisito aprovado do produto.

Critérios técnicos da tarefa 01 atendidos; revisão formal em andamento. Usuário aprovou base vazia e autoria noreply; Git local inicializado para fixar o escopo da revisão. Tarefa 02 ainda não iniciada. Não criar commits com autoria inventada, nem incluir PROJECT_CONTEXT.md sem pedido explícito.

Histórico: [diagnóstico de acesso](../test-harness-diagnosis.md).
