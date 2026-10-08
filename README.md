# Power Timer

Projeto em desenvolvimento: agendador minimalista de desligamento, suspensão e hibernação para Windhawk.

Estado: ciclo de testes da tarefa 01 validado após reinicialização: compilação e 130 verificações aprovadas, falha intencional detectada e fonte restaurada. Revisão formal concluída sem achados; tarefa 01 resolvida. Próxima: ícone e painel em simulação. Veja a [validação](docs/validation/task-01.md), a [revisão](docs/reviews/task-01.md) e a [retrospectiva](docs/retro/task-01.md). Ainda não é um mod instalável; interface e integração Windows estão pendentes.

- [Especificação vigente — ready-for-agent](.scratch/power-timer-v1/spec.md)
- [Mapa das 13 tarefas aprovadas](.scratch/power-timer-v1/map.md)

- [Plano de engenharia](docs/engineering-plan.md)
- [Configuração das skills para revisão](docs/setup-proposal.md)

Para testar o núcleo no PowerShell: `./scripts/test.ps1`. O script usa o compilador do Windhawk instalado ou o caminho explicitamente informado. Os testes não chamam APIs de energia.

Publicação no GitHub e no catálogo Windhawk é uma etapa futura, após validação funcional e visual.
