# Power Timer

Agendador minimalista de desligamento, suspensão e hibernação para Windhawk, em desenvolvimento.

## O que faz

Power Timer programa desligar, suspender ou hibernar o Windows. Cada agendamento pode ser por duração (1 minuto a 7 dias), por data e hora, ou repetido nos dias da semana. Há vários agendamentos ao mesmo tempo, com editar, pausar, cancelar uma ocorrência, adiar 10 minutos e excluir. A lista é recuperada ao reabrir.

- Avisos de contagem (30, 15, 10, 5 e 1 minuto) com som opcional.
- Conflitos de horário são detectados e recusados.
- Opção de forçar o fechamento de aplicativos, **por agendamento**, desmarcada por padrão e com aviso de perda de dados.
- Ações indisponíveis neste computador são explicadas e não são agendadas.
- Português e inglês; tema automático, claro ou escuro; alto contraste.

**Ações reais vêm desligadas.** Por padrão o mod apenas simula (o alvo é um `SimulatedBackend`). Ligando **Executar ações reais de energia** nas configurações do mod e reativando-o, o computador realmente desliga, suspende ou hiberna (`WindowsPowerBackend`). Esse caminho real ainda não foi testado numa máquina: teste primeiro com uma suspensão curta.

![Formulário semanal e lista recuperada na prévia](docs/screenshots/task04-calendar.png)

## Estado

Testado no Windows 11 com Windhawk 1.7.x. **Windows 10 não foi verificado.** O Windhawk instalado precisa ser atualizado manualmente colando a fonte gerada no editor (veja a instalação local). Publicação no catálogo ainda não foi feita; veja o [checklist de publicação](docs/release-checklist.md), que lista as decisões pendentes (nome, licença, autoria).

- [Validação e limitações atuais](docs/validation/task-04.md)
- [Roteiro de validação Windows 11](docs/validation/task-12-roteiro-win11.md)
- [Desligamento forçado e limites do Windows](docs/adr/009-desligamento-forcado.md)
- [Políticas de calendário e recuperação](docs/adr/004-calendario-recorrencias.md)
- [Integração anterior com Windhawk](docs/validation/task-02.md)
- [Preparação e instalação local](docs/installation-local.md)
- [Especificação vigente](.scratch/power-timer-v1/spec.md)
- [Mapa das 13 tarefas aprovadas](.scratch/power-timer-v1/map.md)

No PowerShell 7, na raiz do projeto:

```powershell
./scripts/test.ps1
./scripts/test-executor.ps1
./scripts/build-preview.ps1
./scripts/build-mod.ps1
```

Os scripts usam o compilador instalado com Windhawk. A interface executável fica em `build/preview/`; fonte única para o editor e DLL de conferência ficam em `build/mod/`. Consulte a instalação local antes de ativar o mod.

O CI (`.github/workflows/ci.yml`) roda apenas as suítes portáteis em Linux; armazenamento, calendário e build do mod exigem Windows e o compilador do Windhawk.

Licença: [MIT](LICENSE).

Coautoria: o código foi desenvolvido com assistência de IA (declaração final a confirmar no checklist).
