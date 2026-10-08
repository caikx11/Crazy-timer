# Retrospectiva — tarefa 04

Data: 08/10/2026. Skills retro e writing-for-agents. Fontes: contrato, ADR 004, código, logs TDD, validação visual e revisões.

## Melhorias por impacto

1. **Incluir contexto de apresentação na atualização da UI.** O UTC persistido estava correto, mas a lista podia conservar a hora formatada no fuso anterior. Os eventos de relógio/fuso/retomada agora invalidam a apresentação. Na tarefa 12, incluir a comprovação visual com mudança deliberada de fuso e retorno, mantendo ações simuladas.
2. **Executar o conjunto de verificações na entrega.** O calendário foi conectado ao runner existente; migração é testada em arquivo real e a fonte monolítica inclui os novos headers. Falta CI/hook, já previsto na tarefa 13. Evitar criar um segundo conjunto de scripts para o mesmo pipeline.
3. **Contratos pequenos antes de delegar.** A interface Calendar e os campos persistidos foram acordados cedo, permitindo separar calendário, núcleo e codec. Os ciclos de compilação expuseram transitoriamente métodos e validação ainda em implementação; executar o build final só após os responsáveis declararem estabilidade.
4. **Limitar entrada civil antes de converter tipos.** std::chrono pode estreitar month/day; testes de entradas grandes capturaram esse caso. O decoder limita representações e a validação compartilhada limita os valores do domínio. Manter esse teste é mais eficaz que acrescentar uma regra genérica ao AGENTS.md.

## Próxima etapa

Tarefa 05: impedir programações conflitantes. Astra/high recomendado pela combinação de recorrência, mudanças de fuso e gravação transacional. Regra de troca de modelo permanece no AGENTS.md; se recomendar outra seleção, aguardar o usuário alterar e responder ok.
