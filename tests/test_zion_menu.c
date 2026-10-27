#include "zion_menu.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(zion_menu_install_complete(ZION_INSTALL,false,100,false));
    assert(!zion_menu_install_complete(ZION_INSTALL,true,100,false));
    assert(!zion_menu_install_complete(ZION_INSTALL,false,255,false));
    assert(!zion_menu_install_complete(ZION_INSTALL,false,100,true));
    int reordered[]={4,1,7};
    assert(zion_menu_installed_target(reordered,3,1)==1);
    assert(zion_menu_installed_target(reordered,3,8)==-1);
    assert(zion_menu_installed_target(NULL,0,-1)==-1);
    int packages[]={-100,3,-101};
    assert(zion_menu_installed_target(packages,3,-101)==2);
    assert(zion_menu_installed_target(packages,3,-1)==-1);
    assert(zion_menu_install_qr(ZION_INSTALL,false,0,true));
    assert(zion_menu_install_qr(ZION_INSTALL,false,255,true));
    assert(!zion_menu_install_qr(ZION_INSTALL,false,255,false));
    assert(!zion_menu_install_qr(ZION_INSTALL,false,100,false));
    assert(!zion_menu_install_qr(ZION_INSTALL,true,10,true));
    assert(!zion_menu_install_qr(ZION_SETTINGS,false,0,true));
    unsigned swallowed=0;
    assert(!zion_key_dispatch(&swallowed,ZION_OK,ZION_PRESS_EVENT,true));
    assert(!zion_key_dispatch(&swallowed,ZION_OK,ZION_CLICK_EVENT,false));
    assert(!zion_key_dispatch(&swallowed,ZION_OK,ZION_PRESS_EVENT,false));
    assert(zion_key_dispatch(&swallowed,ZION_OK,ZION_CLICK_EVENT,false));
    assert(!zion_key_dispatch(&swallowed,ZION_UP,ZION_PRESS_EVENT,true));
    assert(!zion_key_dispatch(&swallowed,ZION_UP,ZION_HOLD_EVENT,false));
    assert(!zion_key_dispatch(&swallowed,ZION_UP,ZION_PRESS_EVENT,false));
    assert(zion_key_dispatch(&swallowed,ZION_UP,ZION_CLICK_EVENT,false));
    for(int apps=0;apps<=8;apps++) for(int first=0;first<=apps;first++) {
        int n=zion_menu_visible(apps,first);assert(n>=0 && n<=4);
        int actions=n?n:1;
        assert(zion_menu_row_y(actions+1)+34<270);
        for(int i=0;i<actions+1;i++) assert(zion_menu_row_y(i)+34<zion_menu_row_y(i+1));
    }
    zion_menu_t m;zion_menu_init(&m);
    assert(zion_menu_input(&m,ZION_OK,0,false)==ZION_NONE && m.page==ZION_INSTALL);
    zion_menu_input(&m,ZION_LONG,0,false);assert(m.page==ZION_LIST);
    zion_menu_input(&m,ZION_DOWN,0,false);zion_menu_input(&m,ZION_OK,0,false);assert(m.page==ZION_SETTINGS);
    assert(zion_menu_input(&m,ZION_OK,0,false)==ZION_WIFI_OPEN && m.page==ZION_WIFI);
    assert(zion_menu_input(&m,ZION_LONG,0,false)==ZION_WIFI_CLOSE);
    zion_menu_init(&m);
    for(int i=0;i<8;i++) {assert(m.selected==i);zion_menu_input(&m,ZION_DOWN,8,false);}
    assert(m.selected==8 && m.first==4);
    zion_menu_init(&m);assert(zion_menu_input(&m,ZION_OK,2,false)==ZION_BOOT);
    zion_menu_input(&m,ZION_LONG,2,false);assert(m.page==ZION_MANAGE);
    zion_menu_input(&m,ZION_DOWN,2,false);assert(zion_menu_input(&m,ZION_OK,2,false)==ZION_DEFAULT);
    zion_menu_input(&m,ZION_DOWN,2,false);assert(zion_menu_input(&m,ZION_OK,2,false)==ZION_NONE && m.page==ZION_DELETE && m.selected==0);
    assert(zion_menu_input(&m,ZION_OK,2,false)==ZION_NONE && m.page==ZION_MANAGE);
    zion_menu_input(&m,ZION_DOWN,2,false);zion_menu_input(&m,ZION_DOWN,2,false);zion_menu_input(&m,ZION_OK,2,false);
    zion_menu_input(&m,ZION_DOWN,2,false);assert(zion_menu_input(&m,ZION_OK,2,false)==ZION_REMOVE);
    assert(zion_menu_input(&m,ZION_OK,2,true)==ZION_NONE);
    assert(zion_menu_input(&m,ZION_OK,0,false)==ZION_NONE);
    m.page=ZION_INSTALL;m.selected=0;
    zion_menu_input(&m,ZION_LONG,2,true);assert(m.page==ZION_CANCEL && m.selected==0);
    assert(zion_menu_input(&m,ZION_OK,2,true)==ZION_NONE && m.page==ZION_INSTALL);
    zion_menu_input(&m,ZION_LONG,2,true);zion_menu_input(&m,ZION_DOWN,2,true);
    assert(zion_menu_input(&m,ZION_OK,2,true)==ZION_ABORT);
    puts("Zion menu: PASS (empty/scrolling list, settings, default, explicit delete/cancel confirmations, busy and stale targets)");
}
