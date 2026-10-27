#pragma once
#include <stdbool.h>
typedef enum { ZION_LIST, ZION_MANAGE, ZION_DELETE, ZION_SETTINGS, ZION_WIFI, ZION_INSTALL, ZION_CANCEL } zion_page_t;
typedef enum { ZION_UP, ZION_DOWN, ZION_OK, ZION_LONG } zion_input_t;
typedef enum { ZION_PRESS_EVENT, ZION_CLICK_EVENT, ZION_HOLD_EVENT } zion_key_phase_t;
typedef enum { ZION_NONE, ZION_BOOT, ZION_DEFAULT, ZION_REMOVE, ZION_WIFI_OPEN, ZION_WIFI_CLOSE, ZION_FORGET, ZION_ABORT } zion_action_t;
typedef struct { zion_page_t page; int selected,target,first; } zion_menu_t;
void zion_menu_init(zion_menu_t *m);
int zion_menu_visible(int apps,int first);
int zion_menu_row_y(int row);
bool zion_menu_install_qr(zion_page_t page,bool busy,unsigned stage,bool dismissed);
bool zion_menu_install_complete(zion_page_t page,bool busy,unsigned stage,bool dismissed);
int zion_menu_installed_target(const int *slots,int count,int installed_slot);
bool zion_key_dispatch(unsigned *swallowed,unsigned key,zion_key_phase_t phase,bool cancelled_startup);
/* Pure navigation shared by device keys and test injection. Destructive actions
 * are returned only after selecting an explicit confirmation, default Cancel. */
zion_action_t zion_menu_input(zion_menu_t *m,zion_input_t key,int apps,bool busy);
