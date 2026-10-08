#pragma once

namespace power_timer::text {
// UI catalog boundary. Locale selection and English arrive in ticket 11.
inline constexpr auto title = L"Power Timer · Simulação";
inline constexpr auto brand = L"Power Timer";
inline constexpr auto subtitle = L"Programe uma pausa.";
inline constexpr auto simulation = L"SIMULAÇÃO";
inline constexpr auto simulation_note = L"Seu computador não será desligado nesta versão.";
inline constexpr auto action_label = L"O que você quer fazer?";
inline constexpr const wchar_t* actions[] = {L"Desligar", L"Suspender", L"Hibernar"};
inline constexpr auto duration_label = L"Daqui a quantos minutos?";
inline constexpr auto duration_hint = L"De 1 minuto a 7 dias (10080 minutos)";
inline constexpr const wchar_t* presets[] = {L"15 min", L"30 min", L"1 hora", L"2 horas"};
inline constexpr auto schedule = L"Iniciar simulação";
inline constexpr auto replace = L"Substituir agendamento";
inline constexpr auto cancel = L"Cancelar ocorrência";
inline constexpr auto idle = L"Tudo no seu tempo";
inline constexpr auto idle_detail = L"Escolha uma ação e a duração acima.";
inline constexpr auto active = L"Simulação agendada";
inline constexpr auto warning = L"Últimos 60 segundos";
inline constexpr auto completed = L"Simulação concluída";
inline constexpr auto completed_detail = L"Intenção registrada. Nenhuma ação real executada.";
inline constexpr auto cancelled = L"Ocorrência cancelada";
inline constexpr auto cancelled_detail = L"Você pode programar uma nova simulação.";
inline constexpr auto expired = L"Ocorrência expirada";
inline constexpr auto expired_detail = L"O prazo passou. Nenhuma ação foi executada.";
inline constexpr auto invalid_duration = L"Informe um número inteiro de 1 a 10080 minutos.";
inline constexpr auto preview_prefix = L"Previsão: ";
inline constexpr auto scheduled_prefix = L"Agendado: ";
inline constexpr auto separator = L" · ";
inline constexpr auto invalid_preview = L"Preencha uma duração válida para ver o horário.";
inline constexpr auto open = L"Abrir painel";
inline constexpr auto settings = L"Configurações";
inline constexpr auto options = L"Opções";
inline constexpr auto notification_settings = L"Visibilidade do ícone no Windows";
inline constexpr auto exit = L"Sair do Power Timer";
inline constexpr auto settings_note = L"SIMULAÇÃO · Duração e ação são configuradas neste painel.\nTemas e idiomas chegam nas próximas etapas.";
inline constexpr auto settings_error = L"Não foi possível abrir as configurações do Windows.";
inline constexpr auto tray_error = L"Ícone indisponível. Tentando reconectar à barra…";
inline constexpr auto clock_empty = L"— — : — —";
inline constexpr auto countdown_notification = L"Falta 1 minuto. Abra o painel para acompanhar ou cancelar.";
inline constexpr auto unavailable_time = L"Horário indisponível";
}
