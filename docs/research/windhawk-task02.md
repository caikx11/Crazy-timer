# Windhawk: integração da tarefa 02

Pesquisa em 08/10/2026, somente leitura da instalação e dos fontes oficiais. Nenhum mod instalado, ativado, desativado ou compilado nesta pesquisa. Nenhuma configuração/registro alterado, nem ação de energia executada.

## Versão e arquitetura verificadas

- `C:/Program Files/Windhawk/windhawk.exe`: FileVersion/ProductVersion **1.7.3**; PE Machine **0x014C (x86)**. O sistema operacional é de 64 bits, confirmado por `Environment.Is64BitOperatingSystem`.
- Extensão instalada: `UI/resources/app/extensions/windhawk/package.json`, versão **1.7.3**. EnginePath no `windhawk.ini`: `Engine/1.7.3`; AppDataPath: `%ProgramData%/Windhawk`; Portable=0.
- Bibliotecas disponíveis: `Engine/1.7.3/{32,64,arm64}/windhawk.lib`. Headers locais `windhawk_api.h` e `windhawk_api_internal.h` presentes; **windhawk_tool_mod.h ausente**.
- O endpoint oficial [latest release](https://github.com/ramensoftware/windhawk/releases/latest) redirecionou para **v1.7.3**. A documentação de 2.0 é alpha e não define o contrato desta entrega.

## Contrato do tool mod estável

Para 1.7.3, usar `@include windhawk.exe` e o launcher completo da [wiki oficial](https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process). Define-se a lógica antes dele em `BOOL WhTool_ModInit()`, `void WhTool_ModSettingsChanged()` e `void WhTool_ModUninit()`; o snippet referencia os três, mesmo quando settings não faz nada.

O launcher diferencia instância lançadora/host por `-tool-mod WH_MOD_ID`, exclui sessão 0 e processos de serviço, cria mutex e conecta o entry point a `EntryPoint_Hook`, que faz `ExitThread(0)`. Logo `WhTool_ModInit` precisa iniciar uma thread própria com loop de mensagens antes de retornar. A thread principal não deve ser usada como loop de UI. `WhTool_ModUninit` deve solicitar término, aguardar a thread e liberar recursos; depois o snippet chama `ExitProcess(0)`.

O mutex sem prefixo `Global` é local à sessão. Ele evita duas instâncias do mesmo id nessa sessão; inferência: não garante uma única instância para o mesmo usuário em várias sessões simultâneas. Se a especificação exigir identidade por usuário além disso, será preciso contrato próprio.

Launcher preservado integralmente em `build/task02-research/launcher.txt`, extraído do primeiro bloco cpp do [Markdown oficial](https://raw.githubusercontent.com/wiki/ramensoftware/windhawk/Mods-as-tools:-Running-mods-in-a-dedicated-process.md), com uma quebra de linha final. SHA256: `D8FC18E29ECC63D3225C3612A524078B87907250A200AEC57058B1C3AA46B9A2`.

## Compilar fora da UI

O mecanismo está em [compilerUtils.ts de v1.7.3](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/utils/compilerUtils.ts#L218). Conferido também no `dist/extension.js` instalado, SHA256 `46A52A698DB47D04CB134C7DCA57A42A294BED614BB1C8323862219532C29563`.

Para este host, target **i686-w64-mingw32** e import library **32/windhawk.lib** são necessários. `@architecture x86` basta localmente; omitir arquitetura produz x86 + x86-64. Uma DLL apenas amd64 não carrega no executável x86 verificado.

Argumentos correspondentes ao compilador da extensão, com entrada por arquivo absoluto em lugar de stdin e saída exclusiva no workspace:

```text
clang++.exe
-std=c++23 -O2 -shared
-DUNICODE -D_UNICODE
-DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 -DNTDDI_VERSION=0x0A000008
-D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD
-DWH_MOD_ID=L"local@power-timer" -DWH_MOD_VERSION=L"0.1"
"C:/Program Files/Windhawk/Engine/1.7.3/32/windhawk.lib"
-x c++ "<caminho absoluto da fonte monolítica .wh.cpp>"
-include windhawk_api.h -target i686-w64-mingw32
-Wl,--export-all-symbols -o "<DLL no build do workspace>"
-lshell32 -lcomctl32 -lgdi32 -luser32
```

Executável: `C:/Program Files/Windhawk/Compiler/bin/clang++.exe`. Diretório corrente do compilador oficial: `C:/Program Files/Windhawk/Compiler`; pode-se usar include absoluto em execução a partir do workspace. Os valores de id/versão acima são exemplos e devem corresponder ao mod real. Passar argumentos como array, preservando as aspas da macro literal larga; não concatenar uma linha de shell.

O compilador oficial não adiciona `-static`. A extensão provisiona `libc++.whl`, `libunwind.whl` e `windhawk-mod-shim.dll` no diretório Engine/Mods da arquitetura. Esses arquivos existem localmente em `C:/ProgramData/Windhawk/Engine/Mods/32`. Gerar uma DLL no workspace verifica compilação, mas não registra nem prova carregamento.

## Fluxo recomendado de instalação e ativação local

O [template instalado/oficial](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/files/mod_template.wh.cpp) orienta compilar pelo botão ou **Ctrl+B**. O [código da extensão](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/extension.ts) confirma fluxo Novo mod → editor, compilação com id interno `local@<id>`, registro da DLL e fonte, e controle de ativação do mod editado. Esta é a rota apropriada para a prova no Windhawk instalado:

1. Preparar uma única fonte `.wh.cpp` com metadados, UI/core necessários e launcher no fim; includes de arquivos do repo não estarão disponíveis se a extensão compilar a fonte por stdin em seu diretório próprio.
2. Abrir Windhawk → criar novo mod → substituir o template pela fonte preparada. Usar id exclusivo do Power Timer; não abrir nem editar outros mods.
3. Compilar pelo botão/Ctrl+B. O editor pode compilar e ativar conforme o estado do controle; definir o estado desejado antes da primeira execução. Compilar pela UI escreve a fonte local, DLL e configuração desse mod.
4. Controlar o interruptor desse mod no editor ou na página instalada. Desabilitação específica altera apenas seu campo `Disabled`; o [backend oficial](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/utils/modConfigUtils.ts) possui enableMod por id. Preferir esse controle à edição manual do registro.

`command-line.txt` instalado documenta `-tray-only`, `-exit [-wait]`, `-restart [-tray-only]` e `-safe-mode`. Não foi encontrada CLI oficial de compilação/instalação por arquivo para 1.7.3. O [app.cpp da versão](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/windhawk/app/app.cpp) concentra opções da aplicação. `-tool-mod` é parte do launcher: não substitui a instalação da DLL. Evitar saída/reinício/safe-mode globais para testar somente este mod, pois afetam outros mods.

## Prova de integração a executar pelo agente principal

- Compilar no workspace e pelo editor, com processo dedicado `windhawk.exe -tool-mod "local@<id>"` e só uma instância na sessão atual.
- Confirmar ícone; abrir por clique/menu, fechar escondendo, contagem sem interação e intenção exclusivamente simulada.
- Desabilitar apenas este mod: conferir callback de cleanup, remoção do ícone e término da thread/processo/timers. Reativar: uma nova instância e um ícone.
- Tentar segunda invocação do host somente após instalação e ativação deliberada: mutex deve rejeitar duplicata. Não considerar mera compilação como evidência de runtime.
- Para Explorer, usar reconstrução do ícone via mensagem `TaskbarCreated` quando for possível teste manual deliberado. O host e a ocorrência devem sobreviver; reiniciar Explorer interfere na sessão desktop e em outros mods, portanto não foi executado nesta pesquisa.

`WhTool_ModUninit` pode aguardar sem deadlock se a thread tiver um canal de término pronto e o loop não depender da thread do engine. Recomendação de implementação: evento/handshake de readiness e mensagem de término; limpeza de ícone, timers e janelas na thread proprietária; aguardar antes de retornar. Isso é orientação derivada do contrato, não código prescrito pela wiki.

## Complemento: registro local sem depender da UI

Pesquisa adicional solicitada após verificar que a UI está elevada e não responde ao controle disponível. O mecanismo abaixo é uma reprodução do armazenamento interno verificado da extensão, **não uma CLI pública de instalação**. Nenhuma instalação executada nesta pesquisa.

### Destinos exatos para `local@crazy-time-simulation`

Na instalação não portátil verificada, os destinos são:

| Item | Destino |
| --- | --- |
| Fonte UTF-8 | `C:/ProgramData/Windhawk/ModsSource/local@crazy-time-simulation.wh.cpp` |
| DLL x86 | `C:/ProgramData/Windhawk/Engine/Mods/32/<LibraryFileName>` |
| Configuração | `HKLM/SOFTWARE/Windhawk/Engine/Mods/local@crazy-time-simulation`, **Registry64** |
| Settings | subchave `Settings` dentro da configuração acima |

A extensão usa `KEY_WOW64_64KEY` explicitamente. Não usar `WOW6432Node`. `LibraryFileName` é apenas o basename, não caminho absoluto. O compilador oficial gera `<id-interno>_<version>_<aleatório de seis dígitos>.dll`, portanto `local@crazy-time-simulation_0.1_<seis dígitos>.dll` é o padrão da extensão. O engine combina esse nome com Mods/32, conforme a arquitetura do processo. A fonte mantém `@id crazy-time-simulation` sem o prefixo `local@`, enquanto a macro compilada `WH_MOD_ID` contém `local@crazy-time-simulation`.

Fontes: [modSourceUtils.ts](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/utils/modSourceUtils.ts#L50), [compilerUtils.ts](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/utils/compilerUtils.ts#L422), [modConfigUtils.ts](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/utils/modConfigUtils.ts#L353), [engine storage_manager.cpp](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/windhawk/engine/storage_manager.cpp#L269).

### Campos que o editor grava nesta compilação

| Nome no registro | Tipo | Valor nesta fatia |
| --- | --- | --- |
| `LibraryFileName` | REG_SZ | basename da DLL com id, versão e sufixo exclusivo |
| `Disabled` | REG_DWORD | `1` durante preparação; `0` para ativar |
| `LoggingEnabled` | REG_DWORD | `0` ou `1` conforme escolha de logging |
| `Include` | REG_SZ | `windhawk.exe` |
| `Exclude` | REG_SZ | string vazia se nenhum exclude nos metadados |
| `Architecture` | REG_SZ | `x86` se esse for o metadado adotado; múltiplos separados por `|`; sem metadado, string vazia |
| `Version` | REG_SZ | versão exata dos metadados |
| `SettingsChangeTime` | REG_DWORD | `floor(unixSeconds) & 0x7fffffff`, gravado ao inicializar/escrever settings |

O editor não grava `DebugLoggingEnabled`, `IncludeCustom`, `ExcludeCustom`, `IncludeExcludeCustomOnly` nem `PatternsMatchCriticalSystemProcesses` nesta operação; os defaults ausentes são falsos/vazios. Não preencher esses valores por suposição. Para um mod novo sem settings YAML, `setModConfig(...initialSettings: {})` ainda cria a subchave `Settings` vazia e grava `SettingsChangeTime`. Para mod existente, preserva settings e só escreve quando a mesclagem adiciona algo. O editor não atualiza `userprofile.json` para a compilação/ativação de um id local.

Fontes: [extension.ts, compileEditedMod](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/extension.ts#L1518), [serializer e tipos](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/utils/modConfigUtils.ts#L22), [settings](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/vscode-windhawk/src/utils/modConfigUtils.ts#L457).

### Refresh do loader

O engine cria `RegNotifyChangeKeyValue` na raiz `HKLM/SOFTWARE/Windhawk/Engine/Mods`, visão 64 bits, **watchSubtree=TRUE**, flags `REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET` e, em Windows 8+, `REG_NOTIFY_THREAD_AGNOSTIC`. Mudança de nome/subchave/valor dispara o observador; não há necessidade de mensagem para o tray ou chamada manual de API. O loop espera 200 ms para agrupar mudanças, rearma o observador e executa `ReloadModsAndSettings`.

`Disabled=1` faz `ShouldLoadInRunningProcess` retornar false; `Disabled=0` permite avaliação de Include/Architecture e carga. Mudar `LibraryFileName` força recarga desse mod. `SettingsChangeTime` controla o callback de settings, não serve como sinal obrigatório de enable/disable. O reload enumera todos os mods mas mantém aqueles sem mudanças carregados; não reinicia o engine nem exige mudar configurações de outros mods. Essa notificação comum não é uma garantia de ausência absoluta de atividade em outros mods: todos são reavaliados, como também ocorre pela UI.

Fontes: [observador](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/windhawk/engine/storage_manager.cpp#L432), [loop, espera de 200 ms](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/windhawk/engine/customization_session.cpp#L331), [recarga seletiva](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/windhawk/engine/mods_manager.cpp#L86), [decisão de carga/configuração](https://github.com/ramensoftware/windhawk/blob/v1.7.3/src/windhawk/engine/mod.cpp#L2544).

### Privilégios e roteiro proposto

ACL de `C:/ProgramData/Windhawk/Engine/Mods/32` lida: Administradores e SYSTEM têm FullControl; Usuários/Todos têm ReadAndExecute. A UI elevada observada pelo agente principal é consistente com essa instalação. É preciso helper/script com **elevação Windows normal**, sujeito a UAC e à política da sessão, para escrever nesses destinos e no HKLM. `sandbox_permissions=require_escalated` por si não comprova integridade alta do processo. Não alterar ACLs, proteções, identidade ou tokens para contornar a necessidade de elevação.

Viabilidade: sim, por helper local estreito que replica esses campos e usa Registry64. Não encontrei comando/API público oficial para instalar o arquivo na 1.7.3. Proposta para o agente principal, ainda não executada:

1. Finalizar/revisar DLL e fonte no workspace; validar PE x86 e macro de id. Verificar hashes antes da cópia.
2. Ler/exigir destinos resolvidos exatos e id literal; capturar backup da chave do próprio id, fonte e DLL referenciada se já existirem, preservando tipos dos valores. Em 08/10/2026 a fonte e chave desse id **não existiam** na leitura desta pesquisa. Não exportar/alterar a raiz inteira.
3. Se houver instância anterior, desabilitar somente esse id e verificar término. Para registro novo, gravar `Disabled=1` antes dos outros valores. Preparar fonte e DLL com nome exclusivo; não substituir DLL carregada.
4. Gravar campos equivalentes aos metadados e settings segundo as regras acima. Deixar `Disabled=1` até validar arquivos/configuração. Não tocar `userprofile.json`, EditorWorkspace ou configurações de outros ids.
5. Alterar somente `Disabled` para `0`; o observador atualiza o loader. Esperar/verificar processo `-tool-mod` e ícone em vez de presumir sucesso pelo retorno da gravação. Ao terminar a prova, mudar somente `Disabled` para `1` e verificar cleanup.
6. Para rollback, primeiro desabilitar e aguardar saída; restaurar apenas backups desse id ou remover apenas os artefatos novos desse id se originalmente ausentes. Conservar binário anterior até confirmar término/rollback. Não executar limpeza ampla nem reinício global.

Mesmo nome de DLL/id/flags corretos não prova runtime; continuar exigindo logs/processo e testes de UI/ciclo de vida. O host tool pode herdar a integridade do processo launcher; esta investigação não mede a integridade efetiva da futura instância do Power Timer.
