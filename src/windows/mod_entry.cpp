// ==WindhawkMod==
// @id              power-timer-simulation
// @name            Power Timer (Simulation)
// @name:pt-BR      Power Timer (Simulação)
// @description     Schedule simulated power actions from a tray icon. No real power action is performed.
// @description:pt-BR Agende ações simuladas pelo ícone da barra. Nenhuma ação real de energia é executada.
// @version         0.4.0
// @author          caikx11
// @include         windhawk.exe
// @architecture    x86
// @compilerOptions -lshell32 -lcomctl32 -lgdi32 -ladvapi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Power Timer — simulation preview

This local development version creates a tray icon and a scheduling panel.
It never shuts down, suspends or hibernates the computer.

Click the icon to open the panel. Right-click for settings or to exit.
Closing the panel keeps the simulated countdown running; disabling the mod
stops the host and removes its icon. Schedules are stored locally per user; overdue occurrences expire on restart.

Windhawk 1.7.3 uses its official tool-mod launcher, preserved below.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- showOnStart: true
  $name: Open panel on startup
  $name:pt-BR: Abrir painel ao iniciar
  $description: The tray icon remains available when the panel is hidden.
  $description:pt-BR: O ícone continua disponível quando o painel está oculto.
- soundEnabled: false
  $name: Play notification sounds
  $name:pt-BR: Reproduzir sons de notificação
  $description: Play a sound when warnings and countdown notifications appear.
  $description:pt-BR: Reproduzir um som quando os avisos e notificações de contagem regressiva aparecerem.
*/
// ==/WindhawkModSettings==

#include "tray_host.hpp"

BOOL WhTool_ModInit() {
    return power_timer::windows::start(Wh_GetIntSetting(L"showOnStart") != 0,
        [](const wchar_t* message) { Wh_Log(L"%s", message); }, true);
}

void WhTool_ModSettingsChanged() {
    if (Wh_GetIntSetting(L"showOnStart")) {
        if (HWND window = power_timer::windows::host_window.load())
            PostMessageW(window, power_timer::windows::show_message, 0, 0);
    }

    // Update sound_enabled setting in the schedule book
    if (HWND window = power_timer::windows::host_window.load()) {
        // Access the tray host to update the schedule book's sound_enabled setting
        auto& tray_host = *reinterpret_cast<power_timer::windows::TrayHost*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (tray_host.book_.writable()) {
            auto candidate = tray_host.book_.snapshot_;
            bool sound_enabled = Wh_GetIntSetting(L"soundEnabled") != 0;
            if (candidate.sound_enabled != sound_enabled) {
                candidate.sound_enabled = sound_enabled;
                tray_host.book_.commit(candidate);
            }
        }
    }
}

void WhTool_ModUninit() {
    power_timer::windows::stop();
}

#include "windhawk_tool_launcher.inc"
