# Revisão da tarefa 02 — entrega local concluída

Data: 08/10/2026. Skill: matt-code-review. Base escolhida sob autorização de autonomia: última entrega concluída, `ef6a38aa0a59c53b147310e8286d5e83ee31c03e`. Snapshot: `75b368b3e1232ac2e10bd2556bc9e6739f43e711`.

Comando: `git diff ef6a38aa0a59c53b147310e8286d5e83ee31c03e...75b368b3e1232ac2e10bd2556bc9e6739f43e711`. Commit: `75b368b feat: add simulated tray scheduling preview for task 02`. Diff não vazio: 15 arquivos, 1179 inserções, 11 exclusões.

Dois revisores independentes Sol/high, anunciados antes da criação, somente leitura. Escopo: ticket 02 e histórias relacionadas da especificação. Evidência em [validação](../validation/task-02.md). Não reexecutaram testes nem instalaram o mod.

## Standards

Nenhuma violação comprovada dos padrões documentados e nenhum defeito funcional concreto identificado no diff fixo.

**Julgamento heurístico — [P3] possível Shotgun Surgery nos atalhos.** Em `src/strings.hpp:15`, os rótulos aparecem como `{L"15 min", L"30 min", L"1 hora", L"2 horas"}`; em `src/windows/tray_host.hpp:243`, os valores correspondentes aparecem separadamente como `{L"15", L"30", L"60", L"120"}`. A criação e o tratamento também repetem a quantidade literal `4` nas linhas 162 e 242. Alterar um atalho exige manter rótulo, valor, ordem e quantidade sincronizados entre esses pontos. Considerar uma única tabela de atalhos com valor e referência ao texto do catálogo, derivando dela o tamanho. É uma melhoria de manutenção, sem comportamento incorreto demonstrado; não viola regra explícita do repositório.

O agendador mantém ações simuladas, a documentação distingue preview de integração real, e o ticket permanece aberto bloqueando a tarefa 03. A ausência de validação no Windhawk constitui uma pendência registrada, não um bug comprovado. O launcher oficial foi excluído das sugestões de refatoração conforme orientação.

Resultado inicial: **0 violações documentais, 1 julgamento P3, 0 defeitos concretos**.

### Correção e revisão incremental

Aplicada tabela `DurationPreset` com rótulo e minutos; criação/tratamento derivam quantidade e valor dessa tabela. Preview e DLL recompilaram. A interface confirmou que “2 horas” preenche 120 minutos e atualiza a previsão. Núcleo/testes não mudaram.

Retorno do mesmo revisor: “O P3 foi resolvido: rótulos e minutos agora ficam na mesma tabela DurationPreset, e tanto a criação dos botões quanto o tratamento dos comandos derivam dela a quantidade e o valor. Não encontrei novos achados no diff dos dois arquivos.”

Resultado incremental: **0 violações documentais, 0 julgamentos pendentes, 0 defeitos concretos**.

## Spec

A implementação local está aderente à fatia 02; não encontrei defeitos concretos nem ampliação indevida de escopo no diff `ef6a38a...75b368b`.

- **Requisito parcial — integração ainda sem evidência:** o ticket exige “Janela própria acessível por clique e menu direito” (linha 14) e “Host inicial integrado como tool mod […] saída/desabilitação remove ícone, timers e processamento” (linha 17). Os caminhos existem em `src/windows/tray_host.hpp:388` (Explorer), `:406` (ícone), `:416` (limpeza) e `:508` (parada), ligados aos callbacks em `src/windows/mod_entry.cpp:41` e `:53`. Porém, carga real, clique/menu na bandeja, desabilitação/reativação e unicidade entre sessões continuam sem comprovação, conforme `docs/validation/task-02.md:19–21`. Trata-se de **lacuna de validação**, não de defeito demonstrado. Manter o ticket `ready-for-human/open` e bloquear 03 está correto.
- **Implementação incorreta:** nenhum achado. A rejeição de entrada preserva a ocorrência; cancelamento e entrega da intenção são serializados na thread da interface; fechar esconde o painel; saída destrói janela, timer e ícone; reconstrução da bandeja preserva o agendador. A intenção simulada fica terminal e não é repetida.
- **Scope creep:** nenhum achado relevante. Não cobrei persistência, repetição, executor real, temas ou idiomas previstos nas tarefas posteriores.

**Resultado: zero defeitos concretos e uma lacuna de integração já documentada; aceite integral da tarefa 02 permanece pendente.**

Total final: Standards 0 pendentes (P3 resolvido), pior problema nenhum; Spec 1 lacuna de validação, integração no Windhawk pendente.

## Encerramento após prova integrada

O mesmo revisor Spec (Sol/high) verificou o relatório atualizado em 08/10/2026. Aceite local recomendado: clique/menu e retorno do ícone confirmados pelo usuário; processo, contagem, cancelamento, desabilitação/reativação, continuidade após Explorer e Sair conferidos pelo agente. Não há novos defeitos ou bloqueios. Código/scripts/testes permaneceram iguais desde316de71, sem necessidade de repetir testes.

A lacuna original acima está resolvida para a fatia local. Unicidade entre sessões simultâneas ainda não foi exercitada; cenário explicitamente mantido na matriz12. Isso não impede iniciar03.

Resultado vigente: Standards0 pendentes; Spec0 pendentes nesta fatia local. Tarefa02resolvida. Nenhuma publicação ou teste real de energia realizado.
