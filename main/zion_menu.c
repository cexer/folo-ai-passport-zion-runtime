#include "zion_menu.h"
#include <string.h>
void zion_menu_init(zion_menu_t *m) {memset(m,0,sizeof(*m));}
int zion_menu_visible(int apps,int first) {int n=apps-first;return n<0?0:n>4?4:n;}
int zion_menu_row_y(int row) {return 40+row*38;}
bool zion_menu_install_qr(zion_page_t page,bool busy,unsigned stage,bool dismissed) {
    return page==ZION_INSTALL && !busy && (dismissed || (stage!=100 && stage!=255));
}
bool zion_menu_install_complete(zion_page_t page,bool busy,unsigned stage,bool dismissed) {
    return page==ZION_INSTALL && !busy && stage==100 && !dismissed;
}
int zion_menu_installed_target(const int *slots,int count,int installed_slot) {
    /* Runtime packages use negative slot IDs; only -1 means no result. */
    if(!slots || installed_slot==-1) return -1;
    for(int i=0;i<count;i++) if(slots[i]==installed_slot) return i;
    return -1;
}
bool zion_key_dispatch(unsigned *swallowed,unsigned key,zion_key_phase_t phase,bool cancelled_startup) {
    if(key>ZION_OK) return false;
    unsigned bit=1u<<key;
    if(phase==ZION_PRESS_EVENT) {*swallowed&=~bit;if(cancelled_startup) *swallowed|=bit;return false;}
    if(*swallowed&bit) {if(phase==ZION_CLICK_EVENT) *swallowed&=~bit;return false;}
    return phase==ZION_CLICK_EVENT || phase==ZION_HOLD_EVENT;
}
static void page(zion_menu_t *m,zion_page_t p) {m->page=p;m->selected=0;}
zion_action_t zion_menu_input(zion_menu_t *m,zion_input_t key,int apps,bool busy) {
    if(apps<0) apps=0;
    if(busy && m->page!=ZION_INSTALL && m->page!=ZION_CANCEL) return ZION_NONE;
    if(key==ZION_LONG) {
        if(m->page==ZION_LIST && m->selected<apps) {m->target=m->selected;page(m,ZION_MANAGE);}
        else if(m->page==ZION_INSTALL && busy) page(m,ZION_CANCEL);
        else if(m->page==ZION_CANCEL) page(m,ZION_INSTALL);
        else if(m->page==ZION_DELETE) page(m,ZION_MANAGE);
        else {bool wifi=m->page==ZION_WIFI;page(m,ZION_LIST);if(wifi) return ZION_WIFI_CLOSE;}
        return ZION_NONE;
    }
    int count=m->page==ZION_LIST?apps+2:m->page==ZION_MANAGE?4:m->page==ZION_SETTINGS?3:2;
    if(key==ZION_UP || key==ZION_DOWN) {
        if(m->page==ZION_WIFI || m->page==ZION_INSTALL) return ZION_NONE;
        m->selected=(m->selected+count+(key==ZION_UP?-1:1))%count;
        if(m->page==ZION_LIST && m->selected<apps) {
            if(m->selected<m->first) m->first=m->selected;
            if(m->selected>=m->first+4) m->first=m->selected-3;
        }
        return ZION_NONE;
    }
    if(key!=ZION_OK) return ZION_NONE;
    switch(m->page) {
    case ZION_LIST:
        if(m->selected<apps) {m->target=m->selected;return ZION_BOOT;}
        page(m,m->selected==apps?ZION_INSTALL:ZION_SETTINGS);break;
    case ZION_MANAGE:
        if(m->target>=apps) {page(m,ZION_LIST);break;}
        if(m->selected==0) return ZION_BOOT;
        if(m->selected==1) return ZION_DEFAULT;
        if(m->selected==2) page(m,ZION_DELETE);else page(m,ZION_LIST);
        break;
    case ZION_DELETE:
        if(m->selected==1 && m->target<apps) return ZION_REMOVE;
        page(m,ZION_MANAGE);break;
    case ZION_SETTINGS:
        if(m->selected<2) {bool forget=m->selected==1;page(m,ZION_WIFI);return forget?ZION_FORGET:ZION_WIFI_OPEN;}
        page(m,ZION_LIST);break;
    case ZION_WIFI:page(m,ZION_SETTINGS);return ZION_WIFI_CLOSE;
    case ZION_INSTALL:if(!busy) page(m,ZION_LIST);break;
    case ZION_CANCEL:
        if(m->selected==1) {page(m,ZION_INSTALL);return ZION_ABORT;}
        page(m,ZION_INSTALL);break;
    }
    return ZION_NONE;
}
