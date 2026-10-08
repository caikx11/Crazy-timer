# Entrevista do produto

Status: Q1–Q22 respondidas; entendimento compartilhado confirmado pelo usuário em 08/10/2026 ("vamos seguir"). Etapa to-spec iniciada.

## Rodada 1 — respostas confirmadas em 08/10/2026

1. Ícone na área de notificações, podendo ficar oculto ou permanentemente visível. Usuário deseja configurar isso pelo Windhawk e também pelo menu direito do ícone > Configurações. Viabilidade técnica do controle de visibilidade pelo mod está em pesquisa; não prometer controle que o Windows não oferece.
2. Duração personalizável, atalhos como 15/30 minutos e repetição são requisitos da primeira versão. Horário civil e forma exata da repetição ainda precisam ser definidos.
3. Se o horário for perdido durante suspensão/desligamento, expirar e informar; não executar inesperadamente na retomada.
4. Compatibilidade com Windows 10 e Windows 11 desde a primeira versão.

## Rodada 2 — respostas confirmadas em 08/10/2026

5. Janela própria de agendamentos/preferências pelo menu direito. Visibilidade encaminha às configurações do Windows; Windhawk cuida da instalação/ativação.
6. Duração personalizada, horário específico e repetição diária ou por dias da semana. Repetição a cada X horas fica fora inicialmente.
7. Lista com vários agendamentos, cada um ativável, pausável, editável e excluível.
8. Preservar agendamentos futuros e recorrentes após reinício, com Windhawk ativo; ocorrências perdidas expiram e a repetição segue na próxima ocorrência.
9. Cinco avisos prévios: 30, 15, 10 e 5 minutos, mais 60 segundos. Forma de apresentação, avisos em agendamentos curtos e comportamento após adiar ainda abertos.
10. Respeitar aplicativos por padrão no desligamento; oferecer opção de forçar caso o usuário queira. Escopo da opção (por agendamento/global) a confirmar. Forçar pode descartar trabalho não salvo. Não confundir essa opção com suspensão/hibernação: bForce em SetSuspendState não tem efeito, conforme documentação Microsoft.

## Rodada 3 — respostas confirmadas em 08/10/2026

11. Notificações discretas em 30/15/10/5 minutos e painel de contagem nos últimos 60 segundos. Emitir somente marcos futuros; em duração inicial de 10 minutos, avisos aos 5 minutos e 60 segundos, sem aviso imediato de 10 minutos nem avisos retroativos.
12. Fechamento forçado configurado por agendamento, inicialmente desmarcado, com aviso de perda de dados ao ativar.
13. Detectar conflito de ações diferentes no mesmo horário e exigir ajuste antes de salvar; não escolher ação silenciosamente.
14. Cancelar pula apenas a ocorrência atual; adiar 10 minutos altera apenas a ocorrência atual. Horários futuros da repetição permanecem iguais. Pausar agendamento interrompe a repetição.
15. Duração conta tempo corrido, inclusive suspensão e reinício; prazo não é deslocado. Ocorrência vencida enquanto indisponível expira.
16. Tema acompanha Windows com seleção manual claro/escuro, painel compacto, cantos arredondados, azul discreto e legibilidade; direção visual consistente no Windows 10 e 11.

## Rodada 4 — respostas confirmadas em 08/10/2026

17. Executar no horário sem exigir interação com avisos; avisos sem som por padrão, som opcional.
18. Informar falha e não tentar executar novamente a mesma ocorrência automaticamente. Exibir ações indisponíveis com explicação.
19. Repetições acompanham o horário local. Se ajuste de relógio/fuso pular um horário, expirar a ocorrência; se voltar, não executar a mesma ocorrência duas vezes.
20. Adiamento que conflita exige escolher outro horário. Impedir duplicatas da mesma ação no mesmo horário; desligamento normal/forçado também conflitam.
21. Primeira versão pt-BR e inglês, seleção inicial conforme Windows e troca manual. Preparar expansão futura para outros idiomas muito utilizados; idiomas adicionais ainda não escolhidos.
22. Duração de 1 minuto a 7 dias; atalhos 15 min, 30 min, 1 h e 2 h. Repetições por dias da semana independem desse limite.

## Fontes técnicas

[Microsoft](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area) reserva ao usuário promover ícone para a área visível; não há API pública para impor essa preferência. [Windhawk](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod) separa leitura de settings do armazenamento local; não oferece setter documentado de settings no estável. Rota para página do mod/CLI de settings aparece no [2.0 alpha](https://github.com/ramensoftware/windhawk/releases). Janela própria aceita em Q5.

[ExitWindowsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-exitwindowsex) documenta desligamento forçado e risco de perda de dados; [SetSuspendState](https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-setsuspendstate) informa que bForce não tem efeito. Retorno aceito de desligamento não comprova conclusão.

## Próxima fronteira

Síntese confirmada e to-spec concluída em 08/10/2026. Especificação vigente: [.scratch/power-timer-v1/spec.md](../.scratch/power-timer-v1/spec.md). Próxima etapa: to-tickets. Se implementação revelar nova escolha de produto, reabrir a pergunta pertinente, sem pressupor resposta.

Detalhes restantes serão resolvidos antes da síntese para confirmação de entendimento. Nome definitivo, autoria e destino GitHub podem ser definidos na preparação da publicação. O núcleo atual é experimental: não possui repetição nem persistência e não satisfaz o escopo confirmado.
