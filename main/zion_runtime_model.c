#include "zion_runtime_model.h"
#include <string.h>
int zion_runtime_validate(const zion_api_t *a,const zion_plugin_t *p) {
    if(!a || !p || !p->id || !p->id[0] || strlen(p->id)>12 || !p->name || !p->entry)
        return ZION_E_ARGUMENT;
    if(p->struct_size!=sizeof(*p) || a->abi_version!=ZION_ABI_VERSION ||
       p->abi_version!=a->abi_version || p->required_api_size>a->struct_size ||
       p->required_api_size<offsetof(zion_api_t,uptime_ms)) return ZION_E_ABI;
    if((p->required_capabilities&a->capabilities)!=p->required_capabilities) return ZION_E_CAPABILITY;
    return ZION_SUCCESS;
}
int zion_runtime_select(zion_runtime_model_t *m,const zion_api_t *a,const zion_plugin_t *p) {
    if(!m) return ZION_E_ARGUMENT;
    if(m->phase!=ZION_RUNTIME_LIST) return ZION_E_STATE;
    int result=zion_runtime_validate(a,p);
    if(result) return result;
    m->plugin=p;m->phase=ZION_RUNTIME_PENDING;return ZION_SUCCESS;
}
int zion_runtime_enter(zion_runtime_model_t *m) {
    if(!m || m->phase!=ZION_RUNTIME_PENDING || !m->plugin) return ZION_E_STATE;
    m->phase=ZION_RUNTIME_RUNNING;return ZION_SUCCESS;
}
int zion_runtime_key_accept(zion_runtime_key_gate_t *g,unsigned key,unsigned phase,int ready) {
    /* Raw BSP release follows a fresh PRESS; it is not exposed through ABI 1. */
    if(!g || !ready || key>ZION_KEY_OK || phase>ZION_KEY_LONG+1) return 0;
    uint32_t bit=1u<<key;
    if(phase==ZION_KEY_PRESS) {g->suppressed&=~bit;g->begun|=bit;}
    else if(!(g->begun&bit)) return 0;
    if(phase==ZION_KEY_LONG) g->suppressed|=bit;
    if(phase==ZION_KEY_CLICK) {
        g->begun&=~bit;
        if(g->suppressed&bit) {g->suppressed&=~bit;return 0;}
    }
    return 1;
}
