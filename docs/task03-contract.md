# Contrato de implementação — tarefa 03

Seam já aprovada: módulo de agendamento consumido pela UI, com relógio/armazenamento/efeitos substituíveis (spec, Testing Decisions). Interface comportamental de lista/comandos/recuperação; testes não inspecionam campos privados.

## Contrato compartilhado

`src/schedule_types.hpp` (agente core cria primeiro):
- `ClockSample { uint64_t utc_ms, monotonic_ms; }`.
- `OccurrenceState { Scheduled, Cancelled, Expired, Uncertain, Completed }`.
- `ScheduleRecord { uint64_t id, occurrence_id; Action action; uint64_t minutes, due_utc_ms; bool paused; OccurrenceState state; }`.
- `ScheduleSnapshot { uint64_t next_id=1, next_occurrence_id=1; vector<ScheduleRecord> records; }`.
- `LoadStatus { Missing, Loaded, Invalid, Unavailable }`; `LoadResult { LoadStatus status; ScheduleSnapshot snapshot; }`.
- `ScheduleStorage` interface virtual `LoadResult load()` e `bool save(const ScheduleSnapshot&)`; destrutor virtual.
- `Delivery { uint64_t id, occurrence_id; Action action; }`.

`src/schedule_book.hpp`: `ScheduleBook(ScheduleStorage&)`, `bool initialize(ClockSample)`, `bool reconcile(ClockSample)`, `bool create(Action,wstring_view,ClockSample)`, `bool edit(uint64_t,Action,wstring_view,ClockSample)`, `bool set_paused(uint64_t,bool,ClockSample)`, `bool erase(uint64_t)`, `bool cancel(uint64_t)`, `vector<Delivery> tick(ClockSample)`, `bool complete(Delivery)`, `const vector<ScheduleRecord>& list() const`, `uint64_t remaining_ms(uint64_t,ClockSample) const`, `bool writable() const`, `BookError error() const`. Erros: None, InvalidInput, NotFound, Storage, InvalidData, NotReady. Métodos bool indicam sucesso; erro explícito distingue gravação de entrada. No-op retorna true quando válido.

`src/schedule_codec.hpp`: `string encode_snapshot(const ScheduleSnapshot&)`, `optional<ScheduleSnapshot> decode_snapshot(string_view)`; formato próprio textual estrito, magic+versão, limites de tamanho e validação de enum/IDs/contadores. `src/windows/schedule_file.hpp`: `FileScheduleStorage(wstring path)` implementa interface; leitura Missing separada de inválido/inacessível, gravação temporária+flush+replace atômico Windows; falha preserva original. Diretório preparado pelo host.

## Comportamento

Durante a sessão, duração segue relógio monotônico (inclui suspensão); prazo UTC salvo restaura depois de processo fechado. initialize/reconcile expiram vencidas antes de permitir tick. Pausar mantém prazo correndo; reativar após prazo expira, nunca dispara imediatamente. Editar cria nova identidade de ocorrência e prazo a partir de agora, preservando pausa. Excluir remove só seleção. Cancelar deixa registro terminal. Tick salva Uncertain antes de retornar Delivery; complete persiste Completed; falha entre etapas/reinício nunca reenvia Uncertain. Falha de commit de comando não muda lista. Falha no caminho de processamento bloqueia efeitos até reinicialização explícita. Dados inválidos/unavailable bloqueiam comandos e efeitos sem sobrescrever arquivo.

Não incluir recorrência, conflitos, força, múltiplos avisos ou temas completos nesta fatia. Limite prático1000 registros e arquivo1MiB; erro de limite sem descartar dados. Preview usa armazenamento separado do mod. Nenhuma energia real. Sem nova dependência instalada.
