/* DeviceTest only. Connect Four's 60-second menu sleep removed USB while
 * unattended diagnostics were being inspected. Keep test devices reachable by
 * resetting at the final deep-sleep boundary. Applications still prepare their
 * peripherals normally; production builds retain genuine SDK deep sleep.
 * This deliberately excludes sleep current/wakeup acceptance from these tests.
 * See docs/runtime-optimization-log.md, test-infrastructure section. */
#include "esp_system.h"
#include "esp_log.h"
void __wrap_esp_deep_sleep_start(void) {
    ESP_LOGW("awake-test", "TEST_DEEP_SLEEP_TO_RESET physical sleep excluded from unattended tests");
    esp_restart();
    __builtin_unreachable();
}
