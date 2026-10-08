# ADR 004 — Horários e repetições locais

Data: 08/10/2026. Status: implementação sob revisão na tarefa 04.

## Política

As decisões de tolerância e resolução de calendário estão em [task04-contract.md](../task04-contract.md), derivadas das regras observáveis da spec. Avulso exige data explícita e instante futuro; uma vez salvo, conserva o instante UTC. Repetição segue dias e hora locais. Horas inexistentes são rejeitadas no avulso e puladas na repetição; horas ambíguas usam exclusivamente o primeiro instante.

O despacho normal aceita atraso de até 5000 ms; depois disso expira sem entregar. Inicialização, retomada e mudança explícita de relógio expiram vencidas sem tolerância. Durações continuam monotônicas. A tolerância limita comportamento; não garante precisão de execução do Windows.

## Estado e armazenamento

Uma regra semanal mantém ocorrência futura e o resultado da ocorrência anterior. Um limite civil crescente impede recriar datas já tratadas ou saltadas, inclusive após voltar relógio/fuso e reiniciar. Editar explicitamente cria nova identidade e regra. Pausa mantém o calendário avançando; cancelamento consome a ocorrência atual e mantém a repetição.

POWER_TIMER 2 acrescenta modalidade, calendário, dias selecionados, limite civil e último resultado. Leitura aceita POWER_TIMER 1 como durações; gravação posterior usa V2 e preserva o arquivo anterior caso falhe. O nome do arquivo permanece schedules-v1.dat (preview-v1.dat na prévia) para encontrar os dados existentes; a versão válida é a do cabeçalho. Versões antigas do programa não interpretam V2 e o rejeitam em vez de sobrescrever silenciosamente.

## Adaptador Windows

Calendar separa o núcleo das APIs de fuso. O adaptador obtém o fuso atual e os deslocamentos padrão/verão para o ano; converte candidatos UTC de volta para hora local, descartando candidatos inconsistentes. Isso distingue hora inexistente de hora ambígua. UTC e conversões têm limite 1970–9999. Falha do adaptador é diferente de um horário inexistente.

As APIs e seus limites foram conferidos na documentação Microsoft: [fuso dinâmico](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-getdynamictimezoneinformation), [regras anuais](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-gettimezoneinformationforyear), [UTC para local](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltimeex). Para fixtures com fuso explícito sem chave do registro, usa-se a conversão com regras fixas, com ressalva documentada para certas viradas de ano com mudanças históricas de deslocamento. O fuso ativo com chave usa a API dinâmica. Testes não alteram o fuso do computador.

## Limites desta fatia

Conflitos ainda serão tratados na tarefa 05; adiamento, avisos completos, executor real e acabamento/idiomas têm seus próprios tickets. Toda entrega é simulada. A fonte única 0.3.0 ainda exige validação da instalação no Windhawk; o preview compartilha o host, mas não substitui essa prova.
