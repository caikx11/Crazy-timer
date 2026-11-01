// ==WindhawkMod==
// @id              crazy-time
// @name            Crazy Time
// @name:pt-BR      Crazy Time
// @description     Schedule shutdown, sleep or hibernate from a tray icon, with warnings and weekly repeats.
// @description:pt-BR Agende desligar, suspender ou hibernar pelo ícone da barra, com avisos e repetição semanal.
// @version         0.7.0
// @author          caikx11
// @github          https://github.com/caikx11
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -lshell32 -lcomctl32 -lgdi32 -ladvapi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Crazy Time

A minimalist scheduler for shutdown, sleep and hibernate, living in the
notification area. Português abaixo.

![Crazy Time panel](https://raw.githubusercontent.com/caikx11/Crazy-timer/c92d0c0f575f877a535377ca7ccf28734ca42306/docs/images/crazy-time-panel.png)

> **This mod really shuts down, sleeps or hibernates your computer** when a
> schedule fires. Save your work first. A warning notification appears before
> the action, and you can cancel or postpone an occurrence from the panel.

> **Windows 10 is untested.** It was developed and tested on Windows 11 only.
> It is expected to work on Windows 10 (version 1809 or newer for the dark
> theme of the controls), but this has not been verified. Please report issues.

## What it does

- Schedule an action **by duration** (1 minute to 7 days), at a **date and time**,
  or **repeating on chosen weekdays**.
- Keep several schedules at once: edit, pause, cancel one occurrence,
  postpone it by 10 minutes, or delete.
- Countdown warnings at 30, 15, 10, 5 and 1 minute, with optional sound.
- Schedules at the same instant are detected and refused.
- Optional per-schedule "force apps to close" (shutdown only), off by default,
  with a data-loss warning.
- Actions the computer cannot perform are explained instead of scheduled.
- Portuguese and English; automatic, light or dark theme; high contrast.

## How to use

1. Enable the mod. A tray icon appears; click it to open the panel.
2. Pick the action and how it is scheduled, then press **Add schedule**.
3. Click the tray icon to open the panel again. Closing the panel keeps the
   countdown running; right-click the icon or use **Options** to exit.
   **Exit** stops scheduling until Windhawk restarts or the mod is toggled off
   and on again; Windhawk still lists the mod as enabled.
4. Disabling the mod stops everything and removes the icon. Schedules are saved
   per user; occurrences that expired while the mod was off are not run.

Schedules are stored in Windhawk's mod storage, in a file named with your user
SID. On first use, an existing legacy LocalAppData schedule file is copied there
without overwriting an existing destination. The legacy file is kept as a backup.
Removing the mod deletes Windhawk's managed storage; the legacy backup remains.

## Português

Agendador minimalista de desligar, suspender e hibernar, na área de notificação.

**Este mod realmente desliga, suspende ou hiberna o computador** quando um
agendamento dispara. Salve seu trabalho antes. Uma notificação de aviso aparece
antes da ação e você pode cancelar ou adiar a ocorrência pelo painel.

**Não testado no Windows 10.** Desenvolvido e testado apenas no Windows 11. Deve
funcionar no Windows 10 (versão 1809 ou mais nova para o tema escuro dos
controles), mas isso não foi verificado. Relate problemas.

**Como usar:** ative o mod, escolha a ação e o tipo de agendamento (por
duração, data e hora, ou repetição semanal) e clique em **Adicionar
agendamento**. Clique no ícone da barra para reabrir o painel; fechar o
painel não interrompe a contagem. Desativar o mod encerra tudo.
**Sair** interrompe os agendamentos até reiniciar o Windhawk ou desativar e
reativar o mod; ele ainda aparece como ativado no Windhawk.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- soundEnabled: false
  $name: Play notification sounds
  $name:pt-BR: Reproduzir sons de notificação
  $description: Play a sound when warnings and countdown notifications appear.
  $description:pt-BR: Reproduzir um som quando os avisos e notificações de contagem regressiva aparecerem.
*/
// ==/WindhawkModSettings==

#include "tray_host.hpp"

BOOL WhTool_ModInit() {
    power_timer::windows::mod_storage_path = [](wchar_t* buffer, size_t count) {
        return Wh_GetModStoragePath(buffer, count);
    };
    power_timer::windows::notification_sound_setting = [] {
        return Wh_GetIntSetting(L"soundEnabled") != 0;
    };
    return power_timer::windows::start(false,
        [](const wchar_t* message) { Wh_Log(L"%s", message); }, true);
}

void WhTool_ModSettingsChanged() {
    // Notifications read the current Windhawk setting when they are emitted.
}

void WhTool_ModUninit() {
    power_timer::windows::stop();
}

#include "windhawk_tool_launcher.inc"
