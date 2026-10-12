# Checklist de publicação (pendências explícitas)

Nada aqui foi decidido ou publicado. Itens abertos permanecem abertos até o usuário confirmar.

## Decisões que dependem do usuário
- [ ] **Nome definitivo** do produto ("Power Timer" é provisório).
- [ ] **Licença** do repositório e do mod (nenhum arquivo LICENSE existe; não escolher sem confirmação).
- [ ] **Autoria e perfil** a constar no cabeçalho do mod e no catálogo.
- [ ] **Declaração de assistência por IA**: o código foi escrito com apoio de assistentes de IA; texto final a aprovar.
- [ ] Visibilidade do repositório (hoje privado).

## Qualidade antes de publicar
- [ ] Roteiro Windows 11 completo (`docs/validation/task-12-roteiro-win11.md`).
- [ ] Windows 10: **não verificado** (sem máquina). Declarar assim ou testar em VM.
- [ ] Revisão de código das tarefas 05–09.
- [ ] Screenshots reais atualizados no README (tema claro/escuro, pt/en).
- [ ] Remover o aviso `lpReserved`? Vem do launcher oficial; não alterar.
- [ ] CI remoto verde (`.github/workflows/ci.yml`, só suítes portáteis).

## Antes de qualquer ação real de energia
- [ ] Executor real implementado e revisado (hoje só `SimulatedBackend`).
- [ ] Ensaio manual deliberado, resultados separados dos testes simulados (ADR 008/009).

## Catálogo Windhawk
- [ ] Conferir o formato exigido pelo catálogo (cabeçalho `@id`, `@name`, `@version`, `@author`, `@license`) contra a documentação atual.
- [ ] Publicar somente após as decisões acima. Aceitação dos mantenedores não é garantida.
