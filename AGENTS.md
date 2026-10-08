# Power Timer

Responda em pt-BR. Ao retomar, leia PROJECT_CONTEXT.md.
Execute scripts/test.ps1 após mudanças no agendador.
Os testes usam ações simuladas; ações reais de energia exigem teste manual deliberado.

## Escolha de modelo antes de cada etapa
Ao concluir uma etapa e anunciar a próxima, incluir na própria resposta final o modelo recomendado para a próxima etapa, o esforço de raciocínio e se o usuário deve manter ou trocar sua seleção, quando verificável. Repetir essa orientação antes de iniciar a etapa. Não apenas mencionar o nome da próxima skill e deixar a orientação para depois. Se a seleção atual não for verificável, usar uma orientação condicional clara, como: "Se está em Astra/high, pode manter; não consigo verificar o seletor atual."
Informar também o esforço de raciocínio recomendado (por exemplo, high), justificar brevemente e comparar com o esforço atual somente quando ele puder ser verificado. Nunca afirmar conhecer modelo ou esforço selecionados sem evidência. Para to-spec, recomendação registrada em 08/10/2026: GPT-6 Astra, esforço high; reavaliar nas próximas etapas. Não alterar o seletor automaticamente.
Antes de iniciar cada etapa do fluxo, informar o modelo recomendado, a razão breve e se o modelo atual é adequado ou se há alternativa melhor para o trabalho. Considerar qualidade, complexidade e custo/limites quando conhecidos. Verificar informações atuais pelas fontes disponíveis; se o modelo selecionado não puder ser confirmado, declarar essa limitação sem inferi-lo pelo nome usado na conversa. Avisar antes de iniciar a etapa, sem trocar modelo por conta própria. Esta regra se aplica também ao retomar o projeto em outra sessão.

## Agent skills

### Modelo e esforço dos subagentes
Antes de criar um subagente, avaliar modelo e esforço adequados à subtarefa e informar a escolha e uma razão breve. Verificar opções e restrições disponíveis na ferramenta; distinguir configuração explicitamente selecionada de configuração herdada. Considerar qualidade, complexidade, custo e limites conhecidos. Não afirmar que um subagente está no melhor modelo sem evidência. Quando só for possível herdar, ou não for possível verificar a configuração, declarar a limitação. Aplicar também ao reatribuir trabalho diferente a um subagente existente. Respeitar as regras do ambiente para autorização, herança e overrides; não usar provedores externos por essa regra.

### Fluxo obrigatório do projeto
Seguir o Main Flow de https://www.aihero.dev/skills, por determinação do usuário:
grill-with-docs → to-spec → to-tickets → implement → code-review → retro.
Ler a skill correspondente ao entrar em cada etapa; respeitar seus critérios de conclusão.
Concluir entrevista e entendimento compartilhado antes de especificar e implementar.
A etapa de revisão usa a skill local matt-code-review, que revisa padrões e aderência à especificação.
Skills auxiliares apoiam a etapa atual sem substituir a sequência. Código experimental anterior não conta como implementação aprovada.

### Issue tracker
Tarefas locais em .scratch/<feature>/. Leia docs/agents/issue-tracker.md ao criar ou consultar tarefas.

### Triage labels
Cinco rótulos padrão. Leia docs/agents/triage-labels.md ao fazer triagem.

### Domain docs
Contexto único. Leia docs/agents/domain.md antes de explorar o domínio.
