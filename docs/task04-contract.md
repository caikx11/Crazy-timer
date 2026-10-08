# Contrato — horários e repetições

Escopo: tarefa 04 e histórias 9, 10, 18, 32, 33. Seam de testes permanece a API pública do agendador com tempo, armazenamento e efeitos substituíveis, conforme spec aprovada.

## Política operacional validada contra a spec

- Avulso: data e hora local explícitas, convertidas em instante UTC futuro na criação. Instante fica fixo depois de salvo; UI exibe a data local atual. Repetição: dias da semana e hora local, recalculada ao mudar fuso/relógio.
- Hora inexistente: avulso rejeitado com erro; repetição pula esse dia. Hora ambígua: usar somente o primeiro instante UTC, jamais a segunda repetição da mesma hora civil.
- Despacho normal aceita até 5 segundos de atraso (inclusive). Atraso maior expira a ocorrência, preservando a regra recorrente. É uma tolerância operacional conservadora, não um prazo garantido pelo Windows; o timer do painel consulta a cada 250 ms. Inicialização, retomada e mudança explícita do relógio expiram prazos já alcançados, sem tolerância nem entrega.
- Durações permanecem monotônicas durante a sessão; tempo indisponível conta. A política de atraso se aplica também à entrega de duração, distinguindo despacho de recuperação.
- Identidade civil consumida é persistida antes da entrega. Retornar relógio/fuso não recria datas civis já tratadas. Recorrência conserva um limite civil persistente, inclusive para horários saltados; edição explícita inicia uma nova regra/identidade.
- Pausa não congela calendário; ocorrências passadas expiram e a regra continua. Cancelamento preserva regra e consome a ocorrência atual. Incerta não é reenviada; próxima data permanece válida. Último resultado deve continuar observável na lista/registro.
- Formato versionado deve ler durações da versão anterior. Dados antigos só são substituídos após gravação atômica bem-sucedida. Conflitos, adiamento, cinco avisos e executor real pertencem a tickets seguintes.

Essas escolhas concretizam as propostas técnicas e regras de recuperação/relógio da especificação; não alteram os comportamentos aprovados. Tolerância será testada nos limites de 5000/5001 ms, sem prometer precisão de tempo real.

## Contrato do calendário

`src/calendar.hpp`: `CivilTime { int year; unsigned month, day, hour, minute; }`, com igualdade; datas aceitas 1970–9999. `Calendar` tem destrutor virtual, `std::optional<CivilTime> local(uint64_t utc_ms) const` e `std::optional<std::vector<uint64_t>> resolve(CivilTime) const`. nullopt indica falha do adaptador; vetor vazio indica hora inexistente; candidatos UTC ordenados/únicos indicam hora válida ou ambígua. Helpers públicos: `valid_civil`, `civil_key` (YYYYMMDDHHMM), `next_day`, `weekday` (0 domingo), `civil_from_key` (optional).

`src/windows/local_calendar.hpp`: `WindowsCalendar` implementa Calendar e lê o fuso Windows atual por chamada; construtor padrão e construtor com DYNAMIC_TIME_ZONE_INFORMATION explícito para testes sem alterar Windows. Resolver candidatos pelos deslocamentos padrão/verão do ano e validar cada candidato com conversão UTC→local. Núcleo depende apenas da interface Calendar.

Propriedade dos arquivos: agente calendário — calendar.hpp, windows/local_calendar.hpp, testes e runner próprios; agente core — schedule_types.hpp, schedule_validation.hpp, schedule_book.hpp, testes book; parent — UI, strings, codec/armazenamento e integração dos scripts. Coordenar antes de editar fora da propriedade.
