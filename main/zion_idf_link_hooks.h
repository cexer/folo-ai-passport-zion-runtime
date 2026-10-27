#pragma once
#include <stdint.h>
/* Exact-host, single-module-per-boot interposition. Staging does not affect
 * callers until the ELF and its executable Flash cache are both committed. */
void zion_idf_stage_link_hook(const char *name,uintptr_t address);
void zion_idf_begin_link_hooks(void);
void zion_idf_activate_link_hooks(void);
uintptr_t zion_idf_link_real(const char *name);
