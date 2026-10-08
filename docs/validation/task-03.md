# Validação — tarefa 03

Data: 08/10/2026. Escopo: lista de durações em simulação, persistência, recuperação e prevenção de reenvio. Ainda sem execução real de energia.

## Testes automáticos

- Núcleo original: 241 verificações, prazo/entrada/cancelamento.
- ScheduleBook: 1118 verificações. CRUD, rollback de gravação, duração monotônica, recuperação antes/depois do prazo, pausa sem congelar prazo, estado incerto e não reenvio, corrupção, limites e overflow. Checkpoint de relógio civil testado com avanço/recuo/UTC zero e falha de gravação.
- Armazenamento: 32318 verificações, incluindo 32 ciclos de substituição com 1000 registros. Roundtrip, schema/enum/IDs/contadores, booleano inválido, truncamento/tail/overflow, bloqueio real de substituição preservando original, temporário abandonado e integração com ScheduleBook.
- Ciclos TDD observados pelos agentes: API ausente/criação; CRUD rollback; recuperação; corrupção; identidade/overflow; estado Uncertain; representação booleana não canônica; prazo civil após mudança de relógio. Falhas reproduzidas antes das correções.

O número de verificações inclui loops de stress; não representa a mesma quantidade de cenários independentes. scripts/test.ps1 chama as três suítes. Nenhum alvo de teste vincula executor de energia.

## Interface real

Via Computer Use, no preview compartilhando o mesmo host da nova versão:

1. Criado agendamento de30min; pausa persistida.
2. Criado segundo de15min; edição para20min preservou duas linhas, sem criar terceira.
3. Saída por menu, recompilação com core finalizado e reabertura: duas linhas, estados e previsões recuperados.
4. Reativação do primeiro mostrou25:44 restantes, mantendo o prazo original em vez de reiniciar30min.
5. Seleção entre linhas e cancelamento individual conferidos; ambas ocorrências terminaram canceladas. Saída pelo menu e consulta CIM fora do sandbox confirmou ausência do preview.

[Captura real da lista recuperada](../screenshots/task03-list.png). Exclusão testada pelo módulo/API pública com persistência e recuperação; o botão Excluir não foi acionado via Computer Use. Recuperação vencida e falhas entre intenção/gravação/entrega foram simuladas com relógio e armazenamento controlados, sem suspender a máquina.

## Limites e próximos passos

A nova DLL/fonte0.2.0 compila; a versão instalada no Windhawk ainda é0.1.0 da tarefa02. Não confundir a prova do preview com atualização da instalação. Verificação da0.2.0 no editor será tentada via Computer Use após revisão.

Formato e relógio documentados em docs/adr/003-persistencia-duracoes.md. Não foi testado corte físico de energia. Falha entre alteração do relógio civil e checkpoint pode restaurar o últimoUTC persistido. Sessões simultâneas/Windows10e11 permanecem na matriz12.
