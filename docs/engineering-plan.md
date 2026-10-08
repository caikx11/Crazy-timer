# Power Timer — plano de engenharia

> Proposta exploratória, ainda sujeita à entrevista grill-with-docs solicitada pelo usuário. As escolhas de comportamento abaixo foram usadas apenas para um núcleo experimental; não constituem requisitos aprovados do produto. Em 08/10/2026 o usuário determinou iniciar o Main Flow pela entrevista antes de avançar na implementação.

## Produto
Mod Windhawk com ícone na área de notificações e painel nativo minimalista para programar Desligar, Suspender ou Hibernar. Nome provisório. O Windows controla se o ícone aparece diretamente ou na área de ícones ocultos.

Primeira entrega: duração relativa de 1 minuto a 7 dias; atalhos de 15 min, 30 min, 1 h e 2 h; um agendamento por vez; editar e cancelar. Horário civil e recorrência ficam para uma etapa posterior.

Interface proposta: painel compacto de uma coluna, Segoe UI, fundo neutro, uma cor de destaque, resumo do horário e contagem regressiva. Aviso 60 segundos antes. Foco visível, teclado, alto contraste e escala de DPI fazem parte dos critérios de aceite.

## Arquitetura e harness
- Scheduler: C++ puro recebe tempo monotônico e devolve intenção; não chama Windows.
- TrayHost: janela e loop de mensagens, controles nativos, ícone e aviso; serializa alterações.
- WindowsPowerActions: capacidades, privilégio e chamadas de energia; erros explícitos.
- Preview: mesma interação com ações simuladas.
- Distribuição: gerar arquivo único .wh.cpp a partir das fontes e launcher oficial.

Harness significa aqui um ciclo reproduzível de implementação e validação: tarefa com critério de aceite → implementação isolada → testes determinísticos → revisão → integração → atualização do contexto. Agentes receberam escopos separados: Windhawk/publicação, UX e núcleo/testes. O agente principal revisa e integra.

## Semântica definida para o núcleo inicial
Tempo em milissegundos monotônicos. Aviso nos 60 segundos finais. Tick no prazo solicita uma ação uma única vez; atraso de até 5 segundos é tolerado, acima disso expira. Retomada de suspensão no prazo ou após ele expira. A interface deve processar eventos de retomada antes de ticks pendentes. Cancelar após a intenção ser entregue não revoga a chamada externa. Agendamento em memória; saída do processo cancela. Entrada inválida preserva o agendamento anterior.

O host deverá consultar capacidades imediatamente antes da execução e nunca fechar aplicativos à força. Solicitação aceita pela API não significa desligamento concluído.

## Etapas e critérios
1. Configuração Matt Pocock: aprovar rascunho e criar instruções próprias.
2. Núcleo: prazos, limites, cancelamento, reprogramação, retomada e emissão única verificados sem energia real.
3. UI em simulação: tray, painel, aviso, teclado, DPI, múltiplos monitores e recriação do ícone após reinício do Explorer.
4. Adaptador Windows: capacidades e privilégios; falhas visíveis; testes com funções falsas.
5. Windhawk: launcher oficial sem alterações, compilação e ciclo instalar/desabilitar/desinstalar.
6. Portfólio: README com screenshots reais, licença/autoria, CI e instruções reproduzíveis.
7. Publicação: repositório GitHub escolhido pelo usuário e PR para catálogo após validação.

## Referências verificadas em 08/10/2026
- [Mods em processo dedicado](https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process): para compatibilidade estável, usar snippet oficial integral e include windhawk.exe; não depender inicialmente do header da versão alpha.
- [Criação de mod](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod).
- [Catálogo e contribuição](https://github.com/ramensoftware/windhawk-mods) e [revisão](https://github.com/ramensoftware/windhawk/wiki/Mod-submission-review-process). Aceitação depende dos mantenedores.
- [Shell notification area](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area).
- [ExitWindowsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-exitwindowsex).
- [SetSuspendState](https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-setsuspendstate).
- [AdjustTokenPrivileges](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-adjusttokenprivileges).

## Skills
find-skills e setup-matt-pocock-skills foram lidas e usadas. codebase-design e writing-for-agents apoiam esta estrutura. tdd, diagnosing-bugs, domain-modeling, prototype e matt-code-review estão instaladas para etapas pertinentes; sua presença não significa que já foram executadas.

Busca consultou skills.sh e fontes dos autores. [Matt Pocock](https://github.com/mattpocock/skills) já está disponível localmente. [Microsoft win-dev-skills](https://github.com/microsoft/win-dev-skills) foi inspecionado: fonte oficial, 463 estrelas observadas; catálogo skills.sh listava cerca de 1,6 mil instalações do conjunto, não de cada skill. Voltado a WinUI/empacotamento/depuração; não instalar para esta base Win32. Não foi encontrada uma skill Windhawk específica validada. Busca via CLI não retornou e foi encerrada; pesquisa web foi usada como alternativa. Nenhum pacote foi instalado.
