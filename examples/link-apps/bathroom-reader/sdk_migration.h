#pragma once
#include "esp_idf_version.h"
#include "esp_sleep.h"
#include "esp_netif.h"
#include "dns_server.h"
#include "../../../main/zion_idf_dns.h"
#include <stddef.h>
_Static_assert(sizeof(dns_entry_pair_t)==sizeof(zion_dns_item_t), "DNS item ABI");
_Static_assert(offsetof(dns_entry_pair_t,ip)==offsetof(zion_dns_item_t,ip), "DNS address ABI");
_Static_assert(offsetof(dns_server_config_t,item)==offsetof(zion_dns_config_t,item), "DNS config ABI");
/* ESP-IDF 6 renamed this IDF 5 API; retain the original application source. */
#if ESP_IDF_VERSION_MAJOR >= 6
#define esp_deep_sleep_enable_gpio_wakeup esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown
#endif
