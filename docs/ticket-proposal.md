# Proposta de tarefas — Power Timer v1

Estado: aprovada pelo usuário e publicada como 13 tickets em 08/10/2026. [Mapa vigente](../.scratch/power-timer-v1/map.md). Etapa to-tickets concluída; esta proposta preserva a divisão aprovada.

## Critério de divisão

README e evidências acompanham cada fatia; a tarefa 13 consolida o material. Revisão independente recomendou validar o host Windhawk já em 02, incorporado nesta proposta. As 40 histórias estão cobertas e as dependências não têm ciclos.

Cada tarefa entrega comportamento demonstrável com UI, regras, armazenamento quando aplicável e testes. A tarefa 01 é preparação mínima necessária, pois hoje os testes compilam mas não executam. Não existem refatorações amplas a planejar. Nenhuma tarefa autoriza publicação remota ou testes reais de energia automáticos.

As dependências abaixo são condições reais de início. Relações transitivas não são repetidas. Tarefas 04, 06, 10 e 11 podem avançar após 03; integração de telas/ocorrências é verificada em 07 e 12. Coordenar edição de arquivos compartilhados ao usar subagentes.

## 01. Tornar o ciclo de testes reproduzível

**Blocked by:** Nenhuma.

**Entrega:** Compilar e executar o harness existente com resultado verificável, preservando o isolamento das ações reais de energia.

**Histórias da especificação:** Preparação do harness e prefactoring mínimo.

**Critérios de aceite:**

- [ ] Diagnosticar e corrigir Acesso negado com evidência, sem desabilitar proteções do Windows por conveniência; se o ambiente bloquear, documentar o impedimento e validar em ambiente apropriado autorizado.
- [ ] Uma execução limpa deve compilar e executar testes; uma asserção intencionalmente falha deve tornar o comando malsucedido, sendo depois restaurada.
- [ ] Testes não vinculam executor real de energia; registrar comando, ambiente e resultado reproduzíveis.
- [ ] Fazer apenas o prefactoring necessário para preservar a interface de tempo/intenção existente; não antecipar funcionalidades.

## 02. Agendar uma duração pelo ícone, em simulação

**Blocked by:** 01.

**Entrega:** Abrir um painel pelo ícone, escolher uma ação e duração, acompanhar e cancelar uma ocorrência simulada.

**Histórias da especificação:** 2, 3, 5, 7, 8, 20, 23, 24, 39.

**Critérios de aceite:**

- [ ] Janela própria acessível por clique e menu direito; fechar esconde a janela e mantém a contagem.
- [ ] Aceitar 1–10080 minutos e atalhos 15/30/60/120, mostrando ação e horário previsto; rejeitar entrada inválida preservando estado.
- [ ] Contagem final e cancelamento atuam até entrega da intenção; intenção simulada ocorre no prazo uma única vez, sem exigir clique.
- [ ] Host inicial integrado como tool mod na versão estável validada do Windhawk, uma instância por usuário; saída/desabilitação remove ícone, timers e processamento. Reinício do Explorer recria só o ícone. Comprovar essa integração cedo, antes de ampliar funcionalidades.
- [ ] UI identifica claramente simulação. Teclado básico, foco e catálogo de textos preparados desde esta fatia; acabamento completo nas tarefas próprias.

## 03. Gerenciar e recuperar a lista de agendamentos

**Blocked by:** 02.

**Entrega:** Criar, editar, pausar, reativar e excluir várias durações e reencontrá-las após fechar e reabrir o host.

**Histórias da especificação:** 11, 12, 13, 14, 15, 16, 17.

**Critérios de aceite:**

- [ ] Lista reflete as alterações de ponta a ponta e mostra próxima ocorrência/estado.
- [ ] Persistência atômica/versionada; gravação que falha não aparece como salva; dados inválidos não disparam ações nem são sobrescritos silenciosamente.
- [ ] Suspensão/reinício conta como tempo corrido; recuperar antes do prazo preserva a ocorrência e depois do prazo a expira.
- [ ] Consumir intenção uma única vez e persistir resultado incerto para não reenviar ao reiniciar; simular falhas entre gravação e entrega.
- [ ] Reconciliar inicialização e retomada antes de processar timers; comprovar com relógio/armazenamento falsos.

## 04. Agendar horários e repetições semanais

**Blocked by:** 03.

**Entrega:** Escolher horário local e dias da semana, consultar a próxima ocorrência e continuar a repetição após ocorrências perdidas.

**Histórias da especificação:** 9, 10, 18, 32, 33.

**Critérios de aceite:**

- [ ] Formulário inclui horário avulso futuro com data explícita e repetição diária/dias selecionados; regras e ocorrências persistem.
- [ ] Viradas de semana, mês e ano produzem a próxima ocorrência correta; a regra permanece após expiração.
- [ ] Avanço de relógio/fuso expira horários saltados; retorno não repete uma ocorrência tratada; testar hora inexistente e ambígua.
- [ ] Duração continua usando tempo corrido; não confundir alteração civil da hora com contagem monotônica da sessão.
- [ ] Apresentar estados e erros de calendário no painel e validar recuperação com casos determinísticos.
- [ ] Fixar e documentar tolerância operacional dos timers e resolução de hora ambígua/inexistente, distinguindo atraso de despacho de ocorrência perdida; validar as políticas contra a especificação antes de implementar.

## 05. Impedir programações conflitantes

**Blocked by:** 04.

**Entrega:** Identificar a programação conflitante e pedir ajuste ao salvar ou reativar horários coincidentes.

**Histórias da especificação:** 26, 27.

**Critérios de aceite:**

- [ ] Detectar mesma ação, ações distintas e normal/forçado no mesmo instante, com dias da semana sobrepostos e exceções.
- [ ] Rejeitar mudança sem perder configuração anterior, exibindo qual programação conflita.
- [ ] Fuso/reconciliação que cria colisão suspende as ocorrências conflitantes e informa o usuário, sem escolher ação automaticamente.
- [ ] Não criar dependência do relógio real nos testes; testar colisões avulsas/recorrentes e ausência de falso conflito em dias distintos.

## 06. Receber os cinco avisos e configurar som

**Blocked by:** 03.

**Entrega:** Receber avisos discretos nos marcos previstos e contagem final, mantendo execução independente da interação com avisos.

**Histórias da especificação:** 19, 20, 21, 22, 23.

**Critérios de aceite:**

- [ ] Avisos em 30/15/10/5 minutos e painel nos 60s finais; somente marcos futuros, cada marco no máximo uma vez.
- [ ] Ao criar duração de 10 minutos, primeiros avisos são 5 minutos e 60 segundos; ao criar 1 minuto, abrir contagem final imediatamente.
- [ ] Ticks atrasados/repetidos e reinício não provocam avalanche ou duplicação de avisos.
- [ ] Som opcional inicialmente desligado, preferência persistente; notificação bloqueada/ignorada não altera execução.
- [ ] Tratamento genérico de ocorrências; compatibilidade com repetição/adiamento será verificada na tarefa 07.

## 07. Cancelar e adiar apenas uma ocorrência

**Blocked by:** 05, 06.

**Entrega:** Usar o aviso ou painel para pular ou adiar uma ocorrência sem alterar a repetição dos próximos dias.

**Histórias da especificação:** 24, 25, 26.

**Critérios de aceite:**

- [ ] Cancelar elimina só a ocorrência selecionada; pausar desativa a programação recorrente.
- [ ] Adiar 10 minutos preserva identidade da ocorrência e horário habitual da regra; alteração sobrevive a reinício.
- [ ] Conflito no adiamento exige escolha de horário válido e preserva o original até confirmar; não há prioridade silenciosa.
- [ ] Recalcular somente avisos futuros do novo prazo, sem duplicar avisos antigos; cancelar/adiar no limite do prazo não produz duas intenções.
- [ ] Não prometer cancelamento depois da entrega ao Windows; demonstrar todos os fluxos em simulação.

## 08. Executar energia com capacidades e erros visíveis

**Blocked by:** 07.

**Entrega:** Executar desligamento normal, suspensão ou hibernação pelo mesmo fluxo validado, explicando indisponibilidade e falhas.

**Histórias da especificação:** 5, 6, 28, 30, 31.

**Critérios de aceite:**

- [ ] Consultar capacidades ao programar e antes de executar, incluindo Modern Standby; indisponibilidade aparece com explicação.
- [ ] Privilégios temporários tratados/restaurados; checar falha parcial ao habilitar privilégio e propagar erro real.
- [ ] Desligamento normal não força aplicativos; suspensão/hibernação preservam eventos de despertar e não habilitam recursos do sistema automaticamente.
- [ ] Registrar intenção duravelmente antes da chamada; solicitação aceita não aparece como desligamento comprovadamente concluído.
- [ ] Falha não repete automaticamente a ocorrência e não cancela a próxima repetição; validar funções Windows falsas e crash/restart.
- [ ] Alvo simulado continua fisicamente isolado do executor real. Ações reais só em ensaio manual deliberado.

## 09. Permitir fechamento forçado por agendamento

**Blocked by:** 08.

**Entrega:** Ativar conscientemente o desligamento forçado em uma programação específica, mantendo as demais no modo normal.

**Histórias da especificação:** 29.

**Critérios de aceite:**

- [ ] Opção desmarcada em novas programações, aviso claro de perda de dados e indicador na lista/resumo.
- [ ] Escolher suspensão/hibernação remove a aplicação da opção; retornar a desligamento não a ativa implicitamente.
- [ ] Persistir a escolha individual, sem preferência global que force outras programações.
- [ ] Testar configuração enviada ao executor falso, diferenças normal/forçado e conflito de horários; documentar limites reais do Windows.

## 10. Aplicar temas e acabamento acessível

**Blocked by:** 03.

**Entrega:** Usar o painel compacto com tema Windows ou escolha manual, boa legibilidade e navegação acessível.

**Histórias da especificação:** 4, 34, 35, 36.

**Critérios de aceite:**

- [ ] Tema claro/escuro automático/manual salvo; azul discreto, cantos arredondados e estados textuais consistentes.
- [ ] Preferência de visibilidade abre configurações Windows; não promete pin programático.
- [ ] Teclado, foco visível, rótulos acessíveis, alto contraste, texto ampliado e DPI 100/150/200% sem sobreposição.
- [ ] Posicionar painel em área visível de múltiplos monitores; capturar evidências visuais da implementação.
- [ ] Aplicar padrões aos controles compartilhados; tarefas posteriores usam esses padrões, e tarefa 12 verifica todas as telas finais.

## 11. Usar o produto em português ou inglês

**Blocked by:** 03.

**Entrega:** Selecionar idioma automaticamente pelo Windows ou manualmente, preservando regras, datas e preferências.

**Histórias da especificação:** 37, 38.

**Critérios de aceite:**

- [ ] pt-BR/inglês, fallback inglês, seleção manual persistente e catálogo extensível sem alterar energia.
- [ ] Textos de lista/formulário/erro/avisos passam pelo catálogo; novas mensagens de tarefas posteriores seguem a mesma regra.
- [ ] Formatos de data/hora e resumo de execução compreensíveis, sem mudar o instante ao trocar idioma.
- [ ] Teste de integridade das chaves e visual com textos extensos; tarefa 12 confere traduções das telas finais.

## 12. Validar o candidato completo no Windhawk e Windows 10/11

**Blocked by:** 09, 10, 11.

**Entrega:** Instalar localmente um candidato do mod e reproduzir os fluxos completos em Windows 10 e 11.

**Histórias da especificação:** 1, 2, 3, 6, 15, 36, 39.

**Critérios de aceite:**

- [ ] Empacotar artefato único com launcher oficial compatível; verificar compilação, ativação, desativação e recarga, uma instância por usuário.
- [ ] Desativar encerra processamento e limpa recursos; reativar restaura estado e expira ocorrências vencidas; reiniciar Explorer não duplica processamento.
- [ ] Executar matriz integrada em ambas versões e registrar ambiente real; não inferir compatibilidade pela compilação de uma única versão.
- [ ] Validar recorrência, conflitos, avisos/adiamento, capacidades e persistência combinados; conferir todas as telas em dois idiomas/temas.
- [ ] Ensaios automáticos simulados passam; testes reais de energia têm autorização deliberada e resultados separados.
- [ ] Produzir candidato para code-review posterior; não publicar no catálogo nem declarar release final por concluir este ticket.

## 13. Preparar demonstração e material do portfólio

**Blocked by:** 12.

**Entrega:** Entregar README, demonstração e pacote de contribuição que permitam entender, compilar e avaliar o produto.

**Histórias da especificação:** 40.

**Critérios de aceite:**

- [ ] Documentar instalação, configuração, limitações, simulação, build/testes e resultados Windows10/11 com screenshots reais.
- [ ] Preparar CI reproduzível e validar comandos localmente onde possível; execução remota fica pendente enquanto não houver repositório.
- [ ] Preparar checklist de autoria/licença/nome/perfil, declaração de assistência por IA e formato exigido pelo catálogo.
- [ ] Nome definitivo, perfil, repositório e licença ainda não confirmados devem permanecer pendências explícitas; não inventar identidade nem publicar placeholder.
- [ ] Publicação GitHub/catalogo ocorre após code-review e decisões finais de release; aceitação dos mantenedores não é critério que possamos garantir.

## Revisão concluída

Usuário aprovou a divisão e dependências. Criados 13 arquivos individuais com ready-for-agent e Execution: open. Cobertura das 40 histórias e ausência de ciclos verificadas. Especificação de origem preservada sem alterações.
