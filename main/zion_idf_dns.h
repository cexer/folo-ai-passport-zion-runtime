#pragma once
/* ABI of Espressif's example DNS server (IPv4 wildcard configuration).
 * Deliberately not a promise of arbitrary multi-rule DNS service support. */
#include <stdint.h>
typedef struct {
    const char *name;
    const char *if_key;
    struct { uint32_t addr; } ip;
} zion_dns_item_t;
typedef struct { int num_of_entries; zion_dns_item_t item[1]; } zion_dns_config_t;
void *zion_idf_dns_start(zion_dns_config_t *config);
void zion_idf_dns_stop(void *handle);
