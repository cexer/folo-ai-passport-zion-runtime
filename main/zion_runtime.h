#pragma once
#include <stdbool.h>
#include "zion_sdk.h"
#include "zion_package.h"
void zion_runtime_init(void);
bool zion_runtime_request(int slot);
void zion_runtime_tick(void);
bool zion_runtime_active(void);
bool zion_runtime_manager_done(void);
void zion_runtime_key(unsigned key,unsigned phase);
void zion_runtime_network(bool online);
const zion_api_t *zion_runtime_api(void);
int zion_runtime_default_slot(void);
bool zion_runtime_set_default(int slot);
const char *zion_runtime_name(int slot);
void zion_runtime_refresh(void);
bool zion_runtime_selected_package(zion_package_t *out);
