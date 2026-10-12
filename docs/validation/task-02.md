# Validação — tarefa 02 (integração local verificada)

Data: 08/10/2026. Ambiente local: Windows da sessão, Windhawk 1.7.3 x86, Clang 20, PowerShell 7. Nenhuma operação real de energia foi executada.

## Núcleo e compilação verificados

- `scripts/test.ps1`: **241 verificações aprovadas**. TDD com falhas observadas para API ausente, entrada vazia e espaços, seguidas de correção. Cobertos limites 1/10080, sinais, decimais, lixo, dígitos Unicode, overflow, cancelamento e preservação do agendamento anterior.
- `scripts/build-preview.ps1`: C++23 com warnings como erros aprovado.
- `scripts/build-mod.ps1`: fonte monolítica e DLL x86 para engine 1.7.3 aprovadas. Um warning de campo não inicializado no launcher oficial preservado. SHA256 do launcher: D8FC18E29ECC63D3225C3612A524078B87907250A200AEC57058B1C3AA46B9A2.
- Revisão Standards/Spec em [task-02.md](../reviews/task-02.md). P3 dos presets corrigido com tabela única e reavaliado sem novos achados. Ambos os alvos recompilaram; UI confirmou 2 horas → 120 minutos. Núcleo/testes permaneceram idênticos à execução de 241 verificações.

## Preview independente

Interface real confirmou início em 1 minuto, aviso final, entrada 0 rejeitada preservando contagem, X esconde mantendo processamento, reabertura da mesma janela, conclusão simulada sem confirmação, preset 15 minutos, cancelamento e menu Opções/Configurações/Sair. Tab percorreu campo e presets, com foco visível. [Captura real](../screenshots/task02-simulation.png).

**Limite das primeiras verificações de saída:** Get-Process dentro do sandbox não enxergava processos externos; ausência nessa consulta não comprovava encerramento. Posteriormente a janela do preview foi identificada pelo app proprietário, fechada pelo menu Sair e sua ausência confirmada por inventário de janelas e CIM fora do sandbox. A consulta CIM dentro do sandbox retornou indisponibilidade. Não usar ausência em consulta isolada como prova de runtime.

## Integração Windhawk comprovada até aqui

1. Usuário instalou/ativou pelo editor oficial. Fonte `ModsSource/local@power-timer-simulation.wh.cpp` corresponde à fonte gerada após normalizar CRLF/LF e espaço final. Registro lido: Disabled=0, Version=0.1.0, Include=windhawk.exe, Architecture=x86, LibraryFileName=local@power-timer-simulation_0.1.0_530652.dll.
2. Primeiro painel ainda era do preview independente, que compartilha exclusão de instância com o mod. Preview encerrado pelo menu. Usuário desativou/reativou somente Power Timer.
3. Inventário de janelas identificou painel com proprietário `C:/Program Files/Windhawk/windhawk.exe`. CIM fora do sandbox confirmou exatamente um processo com `-tool-mod "local@power-timer-simulation"` (PID 1924 nesta execução), nenhum preview. Outros mods não foram alterados.
4. Automação operou o painel integrado: duração 1, Iniciar simulação, contagem 00:01:00 e aviso Últimos 60 segundos. X escondeu a janela. [Captura real da contagem](../screenshots/task02-windhawk.png).
5. Após interrupção do controle pelo usuário com Escape e autorização para retomar, nova observação do mesmo painel confirmou **Simulação concluída** / **Intenção registrada. Nenhuma ação real executada.** O estado final não exigiu clique de confirmação.

## Bandeja e desabilitação

Usuário confirmou que clique simples abre o painel e botão direito mostra o menu. Após desativar somente Power Timer, relatou desaparecimento do ícone. Consulta CIM fora do sandbox confirmou **PowerTimerProcesses=0** e registro **Disabled=1**. Antes da desativação o host PID1924 permanecia ativo com painel oculto. A ausência do processo confirma encerramento de suas threads e timers. Solicitada reativação e autorização específica para reiniciar Explorer durante uma simulação.

## Reativação e cancelamento integrados

Após nova reativação pelo usuário, consulta confirmou Disabled=0 e exatamente um host novo, PID27708. Janela pertence a windhawk.exe. Início de 30 minutos confirmado, seguido de cancelamento: painel mostrou Ocorrência cancelada, relógio vazio e botão Cancelar desabilitado. Uma nova contagem simulada foi preparada para testar continuidade após Explorer. Usuário autorizou explicitamente o reinício.

## Reinício do Explorer

Com autorização explícita, revalidado processo Explorer11972, caminho C:/WINDOWS/Explorer.EXE, sessão1 (mesma da execução). Reiniciado somente esse processo; novo Explorer3436. Host Power Timer manteve PID27708. Painel preservou horário previsto18:23 e continuou de00:27:55 antes para00:27:24 depois; não recriou a ocorrência. [Captura real após reinício](../screenshots/task02-after-explorer.png). Usuário confirmou: o ícone voltou e responde ao clique. A contagem e o processo preservados foram observados pelo agente.

## Saída pelo menu

Após o teste Explorer, a simulação foi cancelada: estado Ocorrência cancelada observado. Ação Opções → Sair executada no host dedicado. Consulta CIM fora do sandbox confirmou **PowerTimerProcesses=0**, sem preview. A enumeração instantânea de janelas ainda mostrou a janela anterior durante o encerramento; a consulta de processos subsequente é a evidência de término. Nenhuma ocorrência fica ativa após término do processo.

## Limites da validação

- Unicidade do host na sessão atual confirmada; o mutex global por SID foi revisado, mas sessões simultâneas do mesmo usuário não foram exercitadas. Esse cenário deve entrar na matriz ampliada da tarefa 12.
- Testes desta fatia foram locais na máquina atual. Windows 10/11 em máquinas distintas, DPI, temas, acessibilidade completa e idiomas pertencem à matriz e às tarefas seguintes.
- Relatos do usuário comprovam clique/menu/retorno visual do ícone; observações do agente comprovam processo, janela, contagem, cancelamento e término. Nenhuma alegação de teste automatizado da bandeja.

A UI administrativa do editor Windhawk não respondeu aos inputs disponíveis na investigação inicial; token confirmou integridade alta e acesso de gravação HKLM negado. O usuário realizou instalação e toggles normalmente. O painel dedicado aceitou automação. Nenhuma ACL/proteção foi alterada. Nenhuma nova quarentena observada.

A prova local de carga, duração, bandeja, cancelamento, desabilitação, reativação, recuperação do Explorer e Sair foi obtida. A revisão incremental de evidências aceitou o encerramento local da tarefa 02, com os limites acima preservados. Roteiro em [instalação local](../installation-local.md).
