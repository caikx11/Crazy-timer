# Configuração para revisão

Nome provisório: Power Timer. Proposta aprovada pelo usuário e aplicada em 08/10/2026.
Escolhas confirmadas: tarefas locais e cinco rótulos padrão.
Proposta: criar `AGENTS.md` na raiz de `power-timer`, pois não há AGENTS.md ou CLAUDE.md próprios nesta subpasta. Preservar o arquivo sincronizado da pasta superior.

## Conteúdo proposto: AGENTS.md

```markdown
# Power Timer

Responda em pt-BR. Ao retomar, leia PROJECT_CONTEXT.md.
Execute scripts/test.ps1 após mudanças no agendador.
Os testes usam ações simuladas; ações reais de energia exigem teste manual deliberado.

## Agent skills

### Issue tracker
Tarefas locais em .scratch/<feature>/. Leia docs/agents/issue-tracker.md ao criar ou consultar tarefas.

### Triage labels
Cinco rótulos padrão. Leia docs/agents/triage-labels.md ao fazer triagem.

### Domain docs
Contexto único. Leia docs/agents/domain.md antes de explorar o domínio.
```

## Conteúdo proposto: docs/agents/issue-tracker.md

```markdown
# Tarefas locais
Uma funcionalidade por pasta .scratch/<feature>/.
Especificação em spec.md; cada tarefa em issues/NN-descricao.md.
Status: registra o rótulo de triagem; Execution: registra open, claimed ou resolved.
Comentários ficam em ## Comments.
Publicar no tracker significa criar arquivo local; consultar tarefa significa ler o arquivo.
Para mapas de trabalho, use map.md com decisões e links aos tickets.
Dependências ficam em Blocked by: NN, NN; iniciar apenas quando resolvidas.
Marque Execution: claimed antes de trabalhar; ao concluir, registre evidência e resolved.
```

## Conteúdo proposto: docs/agents/triage-labels.md

```markdown
# Rótulos
needs-triage: precisa de avaliação.
needs-info: falta informação.
ready-for-agent: especificada e pronta para implementação pelo agente.
ready-for-human: exige atuação humana.
wontfix: não será implementada.
```

## Conteúdo proposto: docs/agents/domain.md

```markdown
# Documentação de domínio
Contexto único: GLOSSARY.md e docs/adr/ na raiz.
Antes de explorar o código, leia o glossário e decisões relevantes, quando existirem.
Use os termos do glossário e sinalize conflitos com decisões registradas.
Ausência desses arquivos não bloqueia trabalho; crie-os quando houver conceitos ou decisões consolidados.
```
