/* Declaration-only bridge for the original SDK5 public name; a provider must
 * still be supplied and validated at host link time before any package runs. */
#include "esp_err.h"
#include <stdint.h>
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t mask,int mode);
#ifndef CONFIG_APP_PROJECT_VER
#define CONFIG_APP_PROJECT_VER "original-soundlab"
#endif
