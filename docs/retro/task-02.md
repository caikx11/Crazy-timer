# Retrospectiva — tarefa 02

Data: 08/10/2026. Skills retro e writing-for-agents. Fontes: pesquisa Windhawk, código e scripts da fatia, evidências do preview, revisão em dois eixos. Integração local concluída e evidências aceitas em revisão incremental.

## Melhorias propostas, por impacto

1. **Conferir acesso à integração antes de ampliar a UI.** A versão, arquitetura e contrato foram pesquisados cedo, mas a UI administrativa só se tornou bloqueio após desenvolver o painel. Em próximas fatias dependentes de aplicativo externo, verificar a capacidade de operar o destino e o modo normal de instalação primeiro. Guardar roteiro manual curto para a presença do usuário. Distinguir elevação de sandbox de elevação Windows; uma não concede a outra.
2. **Automatizar os comandos de validação existentes.** Há testes e compilação com warnings como erros no preview, mas nenhum CI/hook executa esses comandos automaticamente. Lacuna já registrada na tarefa 01 e prevista no ticket 13; não instalar ferramenta nova nem antecipar publicação nesta fatia. O pipeline deverá compilar também a fonte monolítica, além do núcleo puro, e manter testes isolados de energia.
3. **Centralizar dados que mudam juntos.** A revisão detectou rótulos e valores dos presets separados. Tabela única aplicada e reavaliada; não é necessária nova regra global para esse caso. Evidência em docs/reviews/task-02.md.
4. **Reduzir tentativas de UI quando privilégios explicam o bloqueio.** Uma observação, uma ação, uma reobservação e diagnóstico de integridade foram suficientes para distinguir falta de controle de erro do mod. Usar captura só quando necessária; árvore instantânea pode preceder atualização visual. Preservar resultado real em documentação, sem confundir captura com teste de engine.

## O que funcionou

Núcleo puro permitiu TDD sem energia real. Preview reutiliza o mesmo host Win32 e permitiu testar UI enquanto a instalação estava bloqueada. Revisão de ciclo de vida corrigiu o filtro específico de TaskbarCreated e a saída do processo dedicado antes da prova no engine. Revisão formal em dois eixos manteve explícita a lacuna de integração. Modelo/esforço e estado de retomada têm documentos próprios; não duplicar regras.

## Próximo passo

Roteiro integrado executado; tarefa02resolvida e03liberada. A consulta de processos deve ocorrer fora do sandbox, pois a enumeração isolada não mostrava o host real. Usar evidência de processo/janela para distinguir preview e mod. Recomenda-se Astra/high para integração; seletor atual não verificável.
