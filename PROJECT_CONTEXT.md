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
1. Atualizar e validar 0.3.0 no Windhawk. Editor elevado continua sem aceitar cliques/teclas do Computer Use; requer interação do usuário. Fonte pronta build/mod/power-timer-simulation.wh.cpp. Não confundir preview com engine.
2. Próxima fatia sugerida: tarefa 12 (validação Windhawk/Windows10/11).
3. Matriz Windows10/11, multissessão, DPI/acessibilidade e mudança real de fuso na UI permanecem tarefa12. Publicação/CI/portfólio tarefa13; CI/hook ainda ausentes.
4. Futura tarefa08: verificar consumidor real com efeito substituível e falha no lote; não repetir o laço no teste. Limites de calendário e recuperação estão documentados nos ADRs.

# Onde parei
2026-10-11: tarefas 01-07, 10 e 11 implementadas e commitadas; 07 (adiar 10 min, formato POWER_TIMER 5) em 73bd684. Repositório privado em github.com/caikx11/power-timer (branch main). Testes: 241 + 1282 + 32505 + 23 + 5 (avisos) passam; build-mod compila (aviso lpReserved).
Último trabalho: lógica de avisos extraída para src/warning_tracker.hpp (testada: timer de 2 min avisa uma vez, em 1 min) e usada pelo tray_host; o som da notificação agora segue a opção de som (antes dependia do tema).
Problema aberto (reportado pelo usuário): num timer de 2 min o aviso de 1 min não apareceu no teste real. A lógica pura está correta; investigar a entrega da notificação (Foco/Não perturbe, NIIF_RESPECT_QUIET_TIME, janela) com a build nova (a confirmar se já resolvido pelo ajuste do som).
Tarefa 08 em andamento (2026-10-12): criado src/power_executor.hpp (capacidades + PowerBackend + execute_power_action, sem retry, sem força) com tests/power_executor_test.cpp e scripts/test-executor.ps1 (passa em g++). Ligado ao tray_host via SimulatedBackend (falha/indisponível deixa Uncertain, sem retry), header no build-mod.ps1, strings pt/en — NÃO compilado (precisa rodar build-mod.ps1 no Windows). Aviso de indisponibilidade ao programar (command schedule_id) e teste de falha+restart (1286 checks no livro) feitos; Validado pelo usuário no Windows em 2026-10-12: testes, build-mod e mod no Windhawk funcionaram. Tarefa 08 simulada concluída; backend real só em ensaio manual deliberado. Nenhum backend real.
Próxima etapa: tarefa 09 (desligamento forçado, simulado); depois 12 (validação Windhawk/Windows 10/11, exige o usuário no editor elevado) e 13.
Pendente: validar visualmente botões da lista (5) em PT/EN e DPI alto; revisão matt-code-review das tarefas 05-07; PROJECT_CONTEXT.md agora versionado para a sessão na nuvem.
