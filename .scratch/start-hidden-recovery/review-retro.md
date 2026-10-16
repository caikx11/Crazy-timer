# Revisão e retrospectiva — início oculto

## Standards
Revisor independente: nenhum achado acionável. Isolamento de dados, mutex por PID e sincronização do ensaio considerados adequados.

## Spec
Revisor independente: nenhuma falha de implementação ou ampliação de escopo. Lacuna inicial de cobertura do diagnóstico e do timer corrigida; revisão incremental confirmou zero achados pendentes.

## Evidência
Antes: início oculto duplicado enviou uma ordem indevida de abertura; falha/recuperação da bandeja produziu uma exibição indevida.
Depois: zero ordens no início oculto duplicado, uma no visível; zero exibições na falha/recuperação; uma exibição na ativação explícita; tentativa periódica somente após 5000 ms simulados e um diagnóstico registrado.
Suíte completa aprovada; mod e preview compilados. Comparação contra HEAD inicial 5ec0b5cb5b731a9aa1aa0c94946210f84e50fe3a, incluindo arquivos novos do ensaio.

## Retro
A correção anterior limitava somente a primeira criação do painel. O ensaio do host real agora inclui duplicidade, recuperação e abertura explícita, integrado ao comando local existente. A CI atual continua portátil; testes de host Windows exigem o ambiente local com compilador Windhawk. Não criar nova regra textual: a regressão está coberta por verificação determinística.

## Limites
APIs externas de visibilidade, bandeja, mensagens, relógio, diretório de dados e filtro são simuladas no ensaio. Isso comprova as decisões do host, sem comprovar entrega real da bandeja ou resultado do reboot do usuário. Instalação e reboot real permanecem pendentes.
