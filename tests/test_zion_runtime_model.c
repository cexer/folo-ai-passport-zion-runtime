#include "zion_runtime_model.h"
#include <assert.h>
#include <stdio.h>
static void entry(const zion_api_t *api) {(void)api;}
int main(void) {
    zion_api_t api={.abi_version=1,.struct_size=sizeof(api),.capabilities=ZION_CAP_TEXT|ZION_CAP_KEYS};
    zion_plugin_t p={1,sizeof(p),sizeof(api),ZION_CAP_TEXT,"test","Test",entry};
    zion_runtime_model_t m={0};
    assert(zion_runtime_enter(&m)==ZION_E_STATE);
    assert(zion_runtime_validate(&api,&p)==0);
    p.abi_version=2;assert(zion_runtime_validate(&api,&p)==ZION_E_ABI);p.abi_version=1;
    p.required_api_size=sizeof(api)+1;assert(zion_runtime_validate(&api,&p)==ZION_E_ABI);p.required_api_size=sizeof(api);
    p.required_capabilities=ZION_CAP_PCM;assert(zion_runtime_select(&m,&api,&p)==ZION_E_CAPABILITY);
    assert(m.phase==ZION_RUNTIME_LIST && !m.plugin);p.required_capabilities=ZION_CAP_TEXT;
    p.id="id-longer-than-12";assert(zion_runtime_validate(&api,&p)==ZION_E_ARGUMENT);p.id="test";
    assert(zion_runtime_select(&m,&api,&p)==0 && m.phase==ZION_RUNTIME_PENDING);
    assert(zion_runtime_select(&m,&api,&p)==ZION_E_STATE);
    assert(zion_runtime_enter(&m)==0 && m.phase==ZION_RUNTIME_RUNNING);
    assert(zion_runtime_enter(&m)==ZION_E_STATE);
    assert(zion_runtime_select(&m,&api,&p)==ZION_E_STATE);
    assert(m.plugin==&p); /* No unloading or second application in a boot. */
    zion_runtime_key_gate_t gate={0};
    assert(!zion_runtime_key_accept(&gate,2,ZION_KEY_PRESS,0));
    assert(!zion_runtime_key_accept(&gate,2,ZION_KEY_CLICK,1)); /* Held across reset. */
    assert(!zion_runtime_key_accept(&gate,2,ZION_KEY_LONG,1));
    assert(zion_runtime_key_accept(&gate,2,ZION_KEY_PRESS,1));
    assert(zion_runtime_key_accept(&gate,2,ZION_KEY_LONG,1));
    assert(!zion_runtime_key_accept(&gate,2,ZION_KEY_CLICK,1)); /* Long release must not launch. */
    assert(zion_runtime_key_accept(&gate,1,ZION_KEY_PRESS,1));
    assert(zion_runtime_key_accept(&gate,1,ZION_KEY_CLICK,1));
    assert(!zion_runtime_key_accept(&gate,1,ZION_KEY_CLICK,1));
    assert(!zion_runtime_key_accept(&gate,0,ZION_KEY_LONG+1,1));
    assert(zion_runtime_key_accept(&gate,0,ZION_KEY_PRESS,1));
    assert(zion_runtime_key_accept(&gate,0,ZION_KEY_LONG+1,1));
    assert(zion_runtime_key_accept(&gate,0,ZION_KEY_CLICK,1));
    assert(!zion_runtime_key_accept(&gate,0,ZION_KEY_LONG+1,1));
    puts("runtime model: PASS (ABI, capabilities, rejection, one app per boot)");
}
