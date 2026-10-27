#pragma once
#include "zion_sdk.h"
typedef enum { ZION_RUNTIME_LIST, ZION_RUNTIME_PENDING, ZION_RUNTIME_RUNNING } zion_runtime_phase_t;
typedef struct { zion_runtime_phase_t phase; const zion_plugin_t *plugin; } zion_runtime_model_t;
int zion_runtime_validate(const zion_api_t *api,const zion_plugin_t *plugin);
int zion_runtime_select(zion_runtime_model_t *model,const zion_api_t *api,const zion_plugin_t *plugin);
int zion_runtime_enter(zion_runtime_model_t *model);
typedef struct { uint32_t begun,suppressed; } zion_runtime_key_gate_t;
int zion_runtime_key_accept(zion_runtime_key_gate_t *gate,unsigned key,unsigned phase,int ready);
