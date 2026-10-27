#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "bsp_button.h"
uintptr_t zion_idf_resolve(const char *name);
bool zion_idf_key(unsigned key,unsigned phase);
bool zion_idf_polling_buttons(void);
uintptr_t zion_idf_board_resolve(const char *name);
void zion_idf_inject_component_key(unsigned key,unsigned phase);
