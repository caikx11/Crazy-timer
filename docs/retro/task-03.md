# Retrospectiva — tarefa 03

Data: 08/10/2026. Skills retro e writing-for-agents. Fontes: contrato, implementação, suítes, evidências do preview e revisão em dois eixos.

## Melhorias por impacto

1. **Tratar falhas também no consumidor do núcleo.** O Book bloqueava corretamente, mas o host continuava um lote já recebido. Correção aplicada. Na futura integração do executor real (tarefa 08), testar o limite de entrega com efeito substituível e falha entre intenção e resultado; o cenário precisa observar o consumidor verdadeiro, sem copiar seu laço no teste.
2. **Conectar a validação existente à automação de entrega.** scripts/test.ps1 agora executa as três suítes. CI/hook continuam ausentes e previstos na tarefa 13; o futuro pipeline deve compilar também a fonte única para detectar omissão de headers no empacotamento. Nenhuma instalação adicional foi feita.
3. **Manter invariantes em um lugar.** A revisão detectou validação duplicada entre recuperação e formato. schedule_validation.hpp passou a servir ambos, sem criar uma nova regra global ou dependência.
4. **Preservar evidências sem confundir ambientes.** O preview possibilitou validar a lista e sua recuperação. O editor elevado continuou sem responder à automação. Manter instalação 0.2.0 explicitamente pendente e usar o roteiro local quando o usuário puder operar o editor.

## Preferências de colaboração

A regra de aguardar o usuário trocar o modelo e responder "ok" está em AGENTS.md. A recomendação principal permaneceu Astra/high; nenhuma troca de seletor foi feita ou presumida. Contextos são atualizados localmente e ficam fora dos commits.
