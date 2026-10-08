# ADR 003 — Durações persistentes em simulação

Status: implementado e revisado na tarefa 03, em simulação.

## Decisão

ScheduleBook é a interface de comandos/lista consumida pela UI. Recebe amostras UTC/monotônica e ScheduleStorage substituível. Registros têm identidade estável de agendamento e identidade de ocorrência renovada na edição. Pausa impede entrega, mas o prazo continua correndo. Cancelamento torna só a ocorrência terminal; exclusão remove o registro selecionado.

Durante a sessão, o prazo monotônico governa a duração. Na inicialização, o prazoUTC salvo restaura o restante; vencidas expiram antes de timers. Em retomada, WM_TIMECHANGE e encerramento, reconcile expira o que passou e atualiza oUTC salvo das ocorrências futuras a partir do restante monotônico. Inicialização/retomada não entregam ações.

Antes de retornar uma intenção, tick grava Uncertain. Após entrega simulada, complete grava Completed. Reinício nunca reenvia Uncertain. Erro de gravação de comando preserva lista anterior e aparece na UI; falha durante processamento/reconciliação bloqueia novos efeitos até nova inicialização. Arquivo inválido/inacessível bloqueia edição e processamento, sem sobrescrever os dados.

## Arquivo

Formato ASCII textual estrito, cabeçalho `POWER_TIMER 1`, contadores monotônicos e registros. Limites de 1000 registros e 1 MiB; versão desconhecida e campos inválidos são rejeitados. A validação do snapshot é compartilhada entre Book e codec em schedule_validation.hpp. Windows grava temporário exclusivo no mesmo diretório, faz FlushFileBuffers, fecha e substitui com MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH). Não promove temporários órfãos automaticamente.

Destino por usuário: %LOCALAPPDATA%/PowerTimer/schedules-v1.dat. Preview usa preview-v1.dat separado. Os dados da versão0.1.0 existiam só em memória; não há migração de arquivo antigo nesta entrega.

## Limites

UTC é necessário quando não há relógio monotônico contínuo entre processos/boots. Mudanças manuais do relógio enquanto o processo está fechado, ou crash antes de processar um checkpoint, podem afetar a reconstrução do restante. Não se promete transação atômica entre disco e Windows, nem execução exatamente uma vez; prefere-se resultado incerto sem reenvio.

Flush/replace seguem o contrato do Windows, mas resistência a corte físico depende do dispositivo/sistema de arquivos e não foi ensaiada. Referências: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers e https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw.
