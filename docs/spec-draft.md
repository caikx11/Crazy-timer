# Power Timer — especificação da primeira versão

> Rascunho histórico. A versão vigente está em [spec.md](../.scratch/power-timer-v1/spec.md), publicada como ready-for-agent em 08/10/2026. Consulte a versão vigente para planejamento e implementação.

Estado: rascunho de to-spec; produto confirmado, abordagem de testes aguardando confirmação. Após essa confirmação, publicar no tracker local com ready-for-agent. Nome provisório.

## Problem Statement

O usuário precisa programar ações de energia sem recorrer a comandos, lembrar de executá-las ou perder controle sobre horários. Quer acesso discreto pela área de notificações, uma interface minimalista e regras previsíveis para repetições, avisos e retomadas. O resultado deverá funcionar no Windows 10 e 11 e ser adequado para compartilhar no Windhawk e apresentar no GitHub.

## Solution

Um mod Windhawk com processo próprio, ícone na área de notificações e janela de agendamentos. Permite desligar, suspender ou hibernar por duração, horário específico ou repetição semanal. As programações ficam salvas; o usuário recebe avisos, pode cancelar ou adiar uma ocorrência e controla explicitamente se um desligamento pode fechar aplicativos à força.

O Windhawk instala e ativa o mod. Agendamentos e preferências ficam na janela própria. Um atalho encaminha à configuração de visibilidade da barra do Windows; o mod não promete fixar o ícone programaticamente.

## User Stories

1. Como usuário de Windows 10 ou 11, quero executar o mesmo produto, para manter o mesmo fluxo nos dois sistemas.
2. Como usuário, quero acessar o painel pelo ícone, para programar energia rapidamente.
3. Como usuário, quero abrir configurações pelo botão direito, para ajustar preferências sem procurar outra janela.
4. Como usuário, quero acessar a configuração Windows de visibilidade do ícone, para decidir se ele fica visível ou oculto.
5. Como usuário, quero escolher desligar, suspender ou hibernar, para adequar a ação à minha necessidade.
6. Como usuário, quero ver por que uma ação está indisponível, para não programar algo que meu computador não suporta.
7. Como usuário, quero uma duração de 1 minuto a 7 dias, para escolher quanto tempo falta até a ação.
8. Como usuário, quero atalhos de 15/30 minutos e 1/2 horas, para reduzir o preenchimento.
9. Como usuário, quero escolher um horário específico e ver sua data prevista, para saber exatamente quando ocorrerá.
10. Como usuário, quero repetir diariamente ou nos dias selecionados da semana, para automatizar minha rotina.
11. Como usuário, quero manter vários agendamentos, para atender rotinas diferentes.
12. Como usuário, quero editar um agendamento, para corrigir horário ou ação.
13. Como usuário, quero pausar e reativar agendamentos, para interromper uma rotina sem apagá-la.
14. Como usuário, quero excluir agendamentos, para remover rotinas que não uso mais.
15. Como usuário, quero preservar programações após reinício, para não recriá-las.
16. Como usuário, quero que uma duração conte tempo corrido, para manter o prazo mesmo durante suspensão.
17. Como usuário, quero que ocorrências perdidas expirem, para evitar ações inesperadas ao retornar.
18. Como usuário, quero que uma repetição continue após uma ocorrência perdida, para manter os próximos dias programados.
19. Como usuário, quero avisos em 30/15/10/5 minutos, para me preparar para a ação.
20. Como usuário, quero uma contagem nos últimos 60 segundos, para ter uma última oportunidade de cancelar ou adiar.
21. Como usuário, quero receber somente avisos cujos marcos ainda estão no futuro, para não receber uma sequência de lembretes atrasados.
22. Como usuário, quero som opcional e desligado por padrão, para evitar interrupções desnecessárias.
23. Como usuário, quero que a ação ocorra sem confirmar o aviso, para o agendamento funcionar quando estou ausente.
24. Como usuário, quero cancelar só a ocorrência atual, para preservar a repetição.
25. Como usuário, quero adiar uma ocorrência em 10 minutos, para ganhar tempo sem mudar a rotina seguinte.
26. Como usuário, quero resolver conflitos antes de salvar ou adiar, para não ter duas ações no mesmo horário.
27. Como usuário, quero impedir duplicatas, para não receber solicitações repetidas da mesma ação.
28. Como usuário, quero preservar os bloqueios de aplicativos no desligamento normal, para proteger trabalho não salvo.
29. Como usuário, quero habilitar fechamento forçado por agendamento com explicação do risco, para optar conscientemente por esse comportamento.
30. Como usuário, quero ser informado quando uma solicitação falhar, para entender por que não ocorreu.
31. Como usuário, quero que falhas não gerem tentativas automáticas da mesma ocorrência, para evitar uma ação posterior inesperada.
32. Como usuário, quero que repetições acompanhem o horário local, para manter minha rotina quando mudar de fuso.
33. Como usuário, quero impedir execução duplicada quando o relógio voltar, para não repetir uma ocorrência já tratada.
34. Como usuário, quero tema claro/escuro automático ou manual, para combinar o painel com minha preferência.
35. Como usuário, quero visual compacto com destaque azul discreto, para reconhecer informações sem poluição visual.
36. Como usuário, quero navegação por teclado, foco visível e boa legibilidade, para usar o painel com diferentes necessidades de acesso.
37. Como usuário, quero pt-BR ou inglês conforme o sistema e seleção manual, para entender os comandos.
38. Como futuro colaborador, quero adicionar traduções sem alterar a lógica de energia, para ampliar o público do mod.
39. Como usuário, quero fechar o painel sem encerrar os agendamentos, para deixar a ferramenta discreta.
40. Como avaliador do portfólio, quero instruções reproduzíveis e evidências de testes, para compreender o funcionamento e suas limitações.

## Implementation Decisions

### Decisões aprovadas e responsabilidades

- O módulo de agendamento centraliza criação, edição, pausa, exclusão, conflitos, cálculo das ocorrências, avisos e resultados. Sua interface recebe comandos/eventos e fornece a lista e os efeitos observáveis. A UI não recalcula regras de horário.
- Aproveitar a separação existente entre tempo fornecido pelo chamador e intenção de energia. O núcleo experimental atual suporta apenas uma duração e deve evoluir para o contrato aprovado; sua tolerância de atraso não é requisito confirmado do produto.
- Tempo, armazenamento e efeitos Windows são dependências substituíveis desse módulo. Testes usam tempo controlado, armazenamento temporário e ações falsas; a aplicação usa as implementações reais.
- A janela apresenta lista, formulário, preferências, contagem e resultado. Fechá-la mantém o host ativo. Desabilitar o mod interrompe processamento; ao reativar, restaurar estado e expirar ocorrências vencidas.
- O host Windhawk gerencia uma instância por usuário, loop de mensagens e ciclo de vida do ícone. Usar mecanismo oficial de tool mod compatível com a versão estável validada na implementação. Evitar depender de funções exclusivas de alpha.
- Dados persistidos incluem identidade do agendamento, ação, modalidade, prazo/regra semanal, estado ativo/pausado, fechamento forçado e exceções de ocorrências. Preferências incluem idioma, tema e som. Identidades e resultados de ocorrências evitam duplicação após reinício.
- Salvamento deve ser atômico e versionado. Erro de gravação não pode aparecer como alteração salva. Dados inválidos não podem disparar ações nem ser sobrescritos silenciosamente.

### Regras observáveis

- Duração aceita 1–10080 minutos. A UI mostra o instante previsto antes de confirmar. Um horário avulso deve corresponder a um instante futuro explicitamente apresentado; não executar um horário passado imediatamente.
- Repetição usa dias selecionados e hora local. Uma ocorrência tem identidade própria, preservada ao adiar, separada do instante atualmente previsto.
- Tempo corrido inclui indisponibilidade. Retomada/inicialização reconcilia prazos antes de permitir efeitos. Prazos vencidos nesse intervalo expiram; não acordar o computador para recuperar agendamentos.
- Mudança de fuso/relógio recalcula repetições futuras, expira horários saltados e não repete ocorrências tratadas. Testar também as duas ocorrências de uma hora ambígua e horas inexistentes em transições sazonais.
- Conflitos são verificados ao criar, editar, reativar e adiar, considerando sobreposição de dias e exceções. Rejeição preserva o estado anterior e identifica a programação conflitante. Nunca executar duas ações concorrentes por uma colisão revelada na reconciliação; apresentar conflito para ajuste.
- Cada marco de aviso só pode ser emitido uma vez para a previsão atual. Ao criar/adiar, considerar apenas marcos futuros. Para 1 minuto, exibir a contagem final imediatamente; não disparar avisos antigos. Notificação bloqueada pelo sistema não modifica o prazo nem exige interação.
- Cancelar afeta somente a ocorrência; pausar afeta as futuras do agendamento. Adiar desloca a ocorrência em 10 minutos e mantém a regra original. Se houver conflito, rejeitar o adiamento até o usuário escolher um horário válido.
- Cancelamento/adiamento atua até a entrega da solicitação de energia ao Windows; depois disso, a UI não promete revogação. Serializar comandos e execução para impedir disputa no limite do prazo.
- Fechamento forçado é desativado inicialmente e exclusivo de desligamento. A UI explica perda de dados ao ativar e distingue visualmente agendamentos com essa opção. Não herdar essa opção ao escolher suspensão/hibernação.
- Consultar capacidades antes de programar e novamente antes da execução. Erros de privilégios ou chamadas são apresentados; não habilitar hibernação ou elevar configurações do sistema automaticamente.
- Solicitação aceita pelo Windows não equivale a desligamento concluído. Uma ocorrência falha não é repetida automaticamente; a próxima ocorrência recorrente continua válida.
- Escrita de resultado e envio de energia não formam transação atômica. Registrar a intenção antes da chamada e não reenviar automaticamente intenção de resultado incerto após falha/reinício. Preferir não repetir a ação a alegar garantia de execução exata.
- Recriar o ícone após reinício do Explorer sem recriar ocorrências ou duplicar avisos. Respeitar DPI, teclado, alto contraste e monitores. Arredondamento é acabamento visual; sua implementação não deve comprometer acessibilidade no Windows 10.
- Texto da interface usa catálogo de traduções com pt-BR/inglês e fallback em inglês para idioma não suportado. Novos idiomas não exigem mudanças nas regras de agendamento.

### Propostas técnicas a validar na implementação

- Resolver mudanças do relógio civil sem alterar indevidamente durações usando relógio monotônico durante a sessão e prazo persistido para recuperação; documentar os limites em mudanças manuais durante desligamento, quando não há relógio monotônico contínuo.
- Definir tolerância operacional de timers por evidência e testes; não confundir atraso normal de despacho com recuperação de ocorrência perdida durante suspensão/reinício.
- Se reconciliação por fuso criar colisão antes inexistente, suspender as ocorrências conflitantes e explicar o motivo; nunca escolher silenciosamente uma ação.

## Testing Decisions

### Abordagem proposta para confirmação

Usar uma interface principal de testes: comandos e resultados do módulo de agendamento, no mesmo nível consumido pela UI. Tempo, armazenamento e ações falsas permitem atravessar o fluxo completo sem desligar o computador. Complementar com testes específicos do adaptador Windows e validação visual/manual. Testar comportamento observável; não espelhar métodos privados nem exigir determinada organização interna.

Prior art: há um pequeno núcleo puro e testes determinísticos de prazo, cancelamento e overflow. Compilação passou; execução do binário retornou Acesso negado. Não considerar esses testes aprovados até resolver a execução. Ampliar esse ponto de teste em vez de criar um framework paralelo.

### Matriz de aceite

| Grupo | Evidência necessária |
|---|---|
| Duração | Limites 1 e 10080, entradas inválidas, overflow, atalhos e resultado único por ocorrência |
| Avisos | 30/15/10/5/1 min, criação de 10 min sem aviso inicial, contagem imediata de 1 min, eventos repetidos sem duplicação |
| Repetição | Dias úteis, todos os dias, virada de semana/mês/ano e próxima ocorrência após cancelamento/falha |
| Cancelar/adiar | Último instante antes de execução, preservação da regra, validação do novo horário e ausência de efeito após cancelamento |
| Conflitos | Duplicatas, ações distintas, normal/forçado, dias sobrepostos, adiamento, reativação e colisão após mudança de fuso |
| Recuperação | Reinício/suspensão antes e depois do prazo; mod desabilitado; ocorrências perdidas sem execução; estado incerto sem reenvio |
| Relógio | Avanço/retorno, mudança de fuso, hora inexistente/ambígua, duração sem salto enquanto há relógio monotônico |
| Persistência | Reinício do host, falha antes/depois da gravação, armazenamento corrompido, preservação de configurações e ocorrência adiada |
| Windows | Capacidade ausente, perda de capacidade, privilégio não atribuído, erro de API, solicitação aceita sem alegar conclusão |
| UI | Temas, pt-BR/inglês, texto ampliado, teclado, alto contraste, DPI 100/150/200%, múltiplos monitores, Explorer reiniciado |
| Compatibilidade | Compilar e validar em Windows 10 e 11; simulação primeiro, ações reais somente em teste manual deliberado |
| Distribuição | Compilação do mod único, ativar/desativar/recarregar, ausência de processamento após unload e documentação reproduzível |

Testes automáticos nunca vinculam o executor real de energia. No adaptador, substituir as chamadas de sistema por funções controladas. A demonstração deve mostrar claramente que é simulação. O ciclo de implementação segue teste que falha → implementação mínima → teste aprovado → refatoração. Não executar ações reais no computador do usuário como verificação automática.

## Out of Scope

- Reiniciar, fazer logoff, bloquear tela ou ligar/acordar o computador.
- Repetição a cada X horas, automações condicionadas a uso da CPU/download ou controle remoto.
- Sincronização em nuvem, conta online e telemetria.
- Fixação programática do ícone e sincronização bidirecional de configurações com funções não suportadas do Windhawk estável.
- Traduções além de pt-BR/inglês nesta entrega; estrutura para ampliação está incluída.
- Garantir aceitação no catálogo ou publicar sem definir repositório/autoria/licença.

## Further Notes

Entendimento do produto confirmado em 08/10/2026 após 22 respostas. Arquitetura/testes acima são a proposta técnica da etapa to-spec, não alegação de implementação existente. Após confirmar a abordagem de testes, publicar a especificação no tracker local como ready-for-agent e avançar para to-tickets.

Nome final, perfil GitHub, licença e imagens reais serão fechados antes da publicação. Referências normativas: [tool mods](https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process), [criação de mods](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod), [área de notificações](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area), [desligamento](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-exitwindowsex), [suspensão/hibernação](https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-setsuspendstate).
