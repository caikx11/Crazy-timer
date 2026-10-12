# Objetivo do projeto
Power Timer: agendador minimalista de energia para Windows 10/11 via Windhawk, com futura publicação no GitHub e catálogo. Nome provisório.

# Arquitetura
src/scheduler.hpp contém parser e núcleo original. calendar.hpp define Calendar substituível e helpers civis; windows/local_calendar.hpp converte fuso Windows. schedule_types.hpp contém registros/estado; schedule_validation.hpp centraliza invariantes; schedule_book.hpp fornece comandos, previsões, recuperação e entrega; schedule_codec.hpp lê V1/V2 e grava V2; windows/schedule_file.hpp grava atomicamente. windows/tray_host.hpp integra UI, bandeja e timer; preview.cpp reutiliza esse host. mod_entry.cpp usa launcher oficial preservado. scripts/build-mod.ps1 empacota headers em ordem. Spec e 13 tickets em .scratch/power-timer-v1.

# Tecnologias
C++23, Win32, PowerShell 7, Windhawk 1.7.3/Clang 20. Na raiz, scripts/test.ps1 executa quatro suítes; scripts/build-preview.ps1 e scripts/build-mod.ps1 compilam. Último resultado: 241 scheduler + 1230 book + 32442 storage + 23 calendar; inclui loops de stress. Preview com Werror e DLL x86 0.3.0 compilados após correções; warning único lpReserved do launcher oficial. Git main local sem remoto. Build e contexto ignorados.

# Regras obrigatórias
Ler AGENTS.md. Português. Main Flow grill-with-docs → to-spec → to-tickets → implement → matt-code-review → retro. Antes de etapa/subagente anunciar modelo e esforço. Recomendação principal Astra/high; seletor não verificável. Ao recomendar TROCAR modelo principal, PARAR até o usuário trocar e responder explicitamente ok. Usuário pede Computer Use em vez de operações manuais quando possível; ler skill e respeitar limites de acesso. Nenhuma energia real nesta versão. Não commitar contextos sem pedido explícito.

# Decisões tomadas
Tracker local, rótulos padrão e 13 tickets aprovados. Duração monotônica na sessão, UTC salvo na recuperação. Avulso usa data/hora local explícitas convertidas para UTC fixo. Weekly segue dias/hora locais; bit0 domingo, bit6 sábado. Gap: avulso rejeita, weekly pula; fold usa primeiro UTC. Despacho normal tolera até 5000 ms; init/resume/mudança explícita de relógio expiram vencidas sem entregar. Calendar tem local e resolve (nullopt falha, vetor vazio gap, candidatos ordenados fold). Busca limitada a 366 dias antes de erro Calendar. Datas 1970–9999.
Weekly conserva próxima ocorrência e last_occurrence_id/last_state da anterior. civil_high_water obrigatório e crescente impede reenvio após rollback/fuso/restart; edição explícita renova a regra. Pausa não congela calendário; cancel consome somente a ocorrência. Tick grava intenção Uncertain antes de entregar; complete registra resultado. Falha de processamento bloqueia módulo e efeitos do host. UI invalida colunas locais em WM_TIMECHANGE, TimeZoneInformation e retomada. Arquivo POWER_TIMER 2 migra leitura V1 e grava atomicamente só em alteração bem-sucedida. Nome físico mantido schedules-v1.dat (preview-v1.dat separado) em %LOCALAPPDATA%/PowerTimer. Máximo 1000 registros/1 MiB.

# Não alterar
sources/ e AGENTS.md sincronizado superior. Launcher oficial. Proteções/ACL/antivírus/outros mods. Nenhum executor real nos testes. Mod instalado ainda 0.1.0; 0.3.0 somente compilado, NÃO instalado. Preview usa dados próprios. Não publicar remotamente nem instalar dependências sem escopo.

# Tarefas concluídas
01 harness; 02 integração Windhawk local (bandeja, contagem, cancelamento, disable/re-enable, recuperação Explorer, saída), encerrada em 34a4da4. 03 lista persistente, revisão/retro, commits 2c4e3f4 e 0291297. 04 datas/semanais, migração e UI, revisão/retro, commits d01f44d e 0e02bfa4bdc5dea12ad0a45d3cb9f71463e5a620; working tree limpo no último check.
05 conflitos, resolvidos com detecção e tratamento de programações conflitantes, incluindo mensagens de erro e suspensão de ocorrências conflitantes.
06 avisos múltiplos e configuração de som, revisão/retro; codec atualizado para versão 4 com campo sound_enabled; testes de armazenamento corrigidos.

# Pendências
1. Atualizar e validar 0.3.0 no Windhawk. Editor elevado continua sem aceitar cliques/teclas do Computer Use; requer interação do usuário. Fonte pronta build/mod/crazy-time.wh.cpp. Não confundir preview com engine.
2. Próxima fatia sugerida: tarefa 12 (validação Windhawk/Windows10/11).
3. Matriz Windows10/11, multissessão, DPI/acessibilidade e mudança real de fuso na UI permanecem tarefa12. Publicação/CI/portfólio tarefa13; CI/hook ainda ausentes.
4. Futura tarefa08: verificar consumidor real com efeito substituível e falha no lote; não repetir o laço no teste. Limites de calendário e recuperação estão documentados nos ADRs.

# Onde parei
READMEs: README.md (inglês, página do GitHub) e README.pt-BR.md (português). Release: scripts/package-release.ps1 + docs/releases/ (publicação manual pelo usuário; falta merge na main e tag v0.6.0).

2026-10-12 (tarde): a pedido do usuário, o modo simulação foi REMOVIDO do mod: o funcionamento real é o padrão e não há setting realActions. Textos base agora descrevem ação real; só preview.cpp usa simulation_mode (start(..., simulate=true)) com SimulatedBackend e tabela simulation_texts. Mutex/classe de janela mantêm 'Simulation' no nome (compat). NÃO compilado no Windows: rodar build-mod.ps1 e build-preview.ps1. Preview nunca deve agir de verdade.

2026-10-12 (fim): usuário testou o modo real no Windows 11 (Windhawk, mod crazy-time v0.6.0, opção 'Executar ações reais' ligada) e relatou que tudo funcionou. Sem evidências por ação. A causa do 'só simula' inicial era o mod antigo ainda ativo (mutex de instância única mantido). Falta: code-review do WindowsPowerBackend, screenshots, declaração de IA, decisão sobre publicar.

2026-10-12 (depois): usuário pediu o mod com funcionamento real. Criado src/windows/windows_power_backend.hpp (ExitWindowsEx com EWX_POWEROFF[+EWX_FORCE só com opt-in]; SetSuspendState(hibernate, force=FALSE, disableWake=FALSE); SeShutdownPrivilege com checagem de ERROR_NOT_ALL_ASSIGNED e restauração; capacidades via GetPwrCapabilities carregada dinamicamente, Modern Standby tolerado). Escolha em tempo de execução: setting do mod `realActions` (padrão false; vale após desativar/reativar). Em modo real os textos de simulação são trocados (real_texts em tray_host.hpp). Mod renomeado para id `crazy-time` v0.6.0 (build/mod/crazy-time.wh.cpp); desativar o mod antigo. NÃO testado no Windows (nem compilado): rodar build-mod.ps1, depois ensaio manual com Suspender de 1 min antes de qualquer Desligar. Backend real nunca nos testes automáticos.

2026-10-12: tarefas 01-12 concluídas (08 e 09 só simuladas; 12 apenas Windows 11, por relato do usuário; Windows 10 e multissessão não verificados). Tarefa 13 parcial: README, .github/workflows/ci.yml (portáteis), LICENSE MIT, docs/release-checklist.md. Nome definitivo: Crazy Time (id crazy-time, v0.5.0). Visual claro/escuro validado pelo usuário. Code-review 05-09 feito (achado do codec v4 corrigido). Branch de trabalho: claude/peaceful-brahmagupta-e7mwap (sem PR aberto).
Pendente: screenshots atuais (claro/escuro, pt/en) em docs/screenshots e README; declaração de assistência por IA; visibilidade/perfil do repositório; formato do catálogo Windhawk; executor real de energia (só em ensaio manual); desativar o mod antigo 'Power Timer' ao instalar o novo. Nada foi publicado no catálogo.
