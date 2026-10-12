#include "tray_host.hpp"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (!power_timer::windows::start(true, nullptr, false, true /* simulation only */)) return 1;
    WaitForSingleObject(power_timer::windows::ui_thread, INFINITE);
    power_timer::windows::stop();
    return 0;
}
