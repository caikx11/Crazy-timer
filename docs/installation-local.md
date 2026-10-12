# Instalação local

**O mod instalado executa ações reais**: ao chegar a hora, o computador desliga, suspende ou hiberna. Salve seu trabalho e teste primeiro com uma suspensão de 1 minuto. Só a prévia (`scripts/build-preview.ps1`, `build/preview/`) é simulada e nunca age sobre o computador. Testado no Windows 11; Windows 10 não verificado.

## Preparar

Na raiz do projeto, PowerShell 7:

```powershell
./scripts/test.ps1
./scripts/build-mod.ps1
```

O segundo script gera `build/mod/crazy-time.wh.cpp` e uma DLL de conferência. A instalação pelo editor recompila a fonte; não é necessário copiar manualmente a DLL nem editar o registro.

## No Windhawk 1.7.3

1. Abrir o Windhawk e escolher criar um novo mod.
2. Copiar todo o conteúdo de `build/mod/crazy-time.wh.cpp` para substituir o template do novo mod.
3. Conferir o id `crazy-time` e o nome Crazy Time. Não substituir nenhum outro mod.
4. Compilar pelo botão do editor ou Ctrl+B. O id local usado pelo engine será `local@crazy-time`.
5. Ativar somente esse mod e observar painel/ícone. A opção Abrir painel ao iniciar está ligada por padrão.

Nesta máquina a janela Windhawk executa como administrador. A automação atual consegue observá-la, mas não controlá-la. A interação administrativa normal precisa ser feita com o usuário presente. Não foi preparado instalador alternativo nem feita alteração de permissões.

## Critérios para fechar a tarefa 02

- Abrir pelo ícone, abrir menu direito, esconder no X e recuperar a mesma contagem.
- Agendar 1 minuto; confirmar que a ação acontece uma única vez (na prévia, só a intenção simulada). Testar cancelar antes do prazo.
- Desabilitar somente Power Timer: ícone, processo dedicado e timers encerram. Reativar: uma instância e um ícone.
- Sair pelo menu: confirmar encerramento do processo dedicado; depois desabilitar/reativar para testar nova abertura.
- Em teste manual deliberado de reinício do Explorer, o ícone reaparece sem reiniciar agendamento nem processo.
- Registrar versão Windows, resultado e logs em `docs/validation/task-02.md`.

Para experimentar apenas a interface, `./scripts/build-preview.ps1` gera `build/preview/power-timer-preview.exe`. Preview e mod compartilham a UI e a exclusão de instância: feche o preview por Opções → Sair antes de testar o mod. Essa execução isolada não comprova integração com o Windhawk.
