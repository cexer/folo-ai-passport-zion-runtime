#include "zion_service.h"
#include "zion_builtin.h"
#include "zion_runtime.h"
#include "zion_package.h"
#include "zion_manage.h"
#include "meta_carve_flash.h"
#include "ui_pixel.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "esp_app_desc.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS 6
#if CONFIG_META_E2E_TEST_CONTROL
bool zion_font_selftest(void);
#endif
LV_IMAGE_DECLARE(zion_official_mini_code);
static zion_menu_t menu;
static zion_service_state_t service;
static meta_carve_t catalog;
static int slots[META_CARVE_MAX_SLOTS+META_DATA_MAX+1],apps;
static char versions[META_CARVE_MAX_SLOTS][17];
static lv_obj_t *screen,*title,*content,*footer,*footer_right,*stats[3],*battery,*fill,*power;
static lv_obj_t *qr,*install_hint;
static lv_obj_t *rows[ROWS],*names[ROWS],*details[ROWS],*marks[ROWS];
/* Stable text storage avoids reallocating labels into TLS receive-buffer holes
 * while a download is active. All updates still run under the LVGL lock. */
static char title_text[64],content_text[640],footer_text[96],power_text[12],install_hint_text[320];
static char footer_right_text[48];
static char stats_text[3][56],names_text[ROWS][96],details_text[ROWS][64];
static void label_update(lv_obj_t *obj,char *buffer,size_t capacity,const char *value) {
    if(!strcmp(buffer,value) && lv_label_get_text(obj)==buffer) return;
    snprintf(buffer,capacity,"%s",value);lv_label_set_text_static(obj,buffer);
}
#define LABEL(obj,value) label_update(obj,obj##_text,sizeof(obj##_text),value)
static char notice[96]="短按OK启动  长按OK管理";
static bool dirty=true,auto_boot,skip_once,pending;
static bool startup_escape;
static zion_action_t pending_action;
static int64_t deadline,last_refresh;
static unsigned generation;
static unsigned install_id;
static bool install_result_dismissed=true;

static void catalog_refresh(void) {
    if(service.busy) return;
    if(!zion_service_catalog(&catalog)) return;
    zion_runtime_refresh();apps=1;slots[0]=ZION_BUILTIN_SLOT;
    for(unsigned i=0;i<zion_package_count();i++) slots[apps++]=zion_package_slot(i);
    for(unsigned i=0;i<catalog.count;i++) {
        memset(versions[i],0,sizeof(versions[i]));
        if(catalog.slot[i].state==META_SLOT_VALID) (void)esp_flash_read(NULL,versions[i],catalog.slot[i].offset+48,16);
    }
    for(unsigned i=0;i<catalog.count;i++) if(catalog.slot[i].kind==META_CARVE_KIND_APP) slots[apps++]=(int)i;
    if(menu.selected>=apps+2) menu.selected=apps+1;
    if(menu.first>=apps) menu.first=apps>4?apps-4:0;
}
static int target(void) {return menu.target>=0 && menu.target<apps?slots[menu.target]:-1;}
static int default_slot(void) {return zion_runtime_default_slot()!=-1?zion_runtime_default_slot():zion_default_find(&catalog,&service.prefs);}
static bool target_ready(void) {return zion_runtime_name(target())!=NULL || (target()>=0 && catalog.slot[target()].state==META_SLOT_VALID);}
static void submit(zion_action_t a) {
    if(a==ZION_NONE) return;
    if((zion_runtime_name(target())!=NULL && a==ZION_BOOT) || (target()==ZION_BUILTIN_SLOT && a==ZION_REMOVE)) {
        if(a==ZION_BOOT) {if(!zion_runtime_request(target())) snprintf(notice,sizeof(notice),"启动失败：请关闭配网或稍后重试");}
        else if(a==ZION_REMOVE) {
            snprintf(notice,sizeof(notice),"内置玩法不可删除");
        }
        dirty=true;return;
    }
    if((a==ZION_BOOT || a==ZION_DEFAULT) && !target_ready()) {
        snprintf(notice,sizeof(notice),"玩法未安装完整，请先删除");dirty=true;return;
    }
    if(zion_service_request(a,target())) {
        pending_action=a;pending=a!=ZION_ABORT;
        snprintf(notice,sizeof(notice),"正在处理，请稍候");
    } else snprintf(notice,sizeof(notice),"设备忙，请稍后重试");
    dirty=true;
}
bool zion_launcher_cancel_auto(void) {
    bool cancelled=auto_boot || startup_escape;
    if(cancelled) {snprintf(notice,sizeof(notice),auto_boot?"已取消默认启动":"已留在Zion");auto_boot=false;startup_escape=false;dirty=true;}
    return cancelled;
}
void zion_launcher_key(zion_input_t key) {
    auto_boot=false;
    zion_service_get(&service);
    if(menu.page==ZION_MANAGE && target()==ZION_BUILTIN_SLOT && menu.selected==2 && key==ZION_OK) return;
    zion_page_t previous=menu.page;
    zion_action_t a=zion_menu_input(&menu,key,apps,service.busy);
    /* A deliberate new visit opens the community, even after a failed install.
     * Automatic terminal results remain visible until the user leaves them. */
    if(!service.busy && previous==ZION_LIST && menu.page==ZION_INSTALL) install_result_dismissed=true;
    if(!service.busy) submit(a);
    else if(a==ZION_ABORT) submit(a);
    dirty=true;
}
static void row(unsigned i,int y,const char *name,const char *detail,bool selected) {
    lv_obj_clear_flag(rows[i],LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(rows[i],10,y);
    label_update(names[i],names_text[i],sizeof(names_text[i]),name);label_update(details[i],details_text[i],sizeof(details_text[i]),detail?detail:"");
    lv_obj_set_pos(names[i],3,detail?-2:4);
    ui_pixel_set_selected(rows[i],selected,true);
}
static void render(void) {
    if(!bsp_lvgl_lock(200)) return;
    for(unsigned i=0;i<ROWS;i++) {lv_obj_add_flag(rows[i],LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(marks[i],LV_OBJ_FLAG_HIDDEN);}
    bool show_qr=zion_menu_install_qr(menu.page,service.busy,service.stage,install_result_dismissed);
    if(!show_qr) LABEL(install_hint,"");
    if(!show_qr && qr) {lv_obj_delete(qr);qr=NULL;}
    if(menu.page==ZION_LIST) {
        lv_obj_set_width(footer,110);
        LABEL(footer,"短按OK启动");LABEL(footer_right,"长按OK管理");
        lv_obj_clear_flag(footer_right,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_width(footer,220);LABEL(footer_right,"");
        lv_obj_add_flag(footer_right,LV_OBJ_FLAG_HIDDEN);
    }
    if(menu.page!=ZION_LIST) for(unsigned i=0;i<3;i++) label_update(stats[i],stats_text[i],sizeof(stats_text[i]),"");
    char line[640],meta[64];
    int def=default_slot();
    const char *heading=menu.page==ZION_LIST?"ZION":menu.page==ZION_MANAGE?"ZION / 管理":
        menu.page==ZION_DELETE?"ZION / 删除":menu.page==ZION_SETTINGS?"ZION / 设置":
        menu.page==ZION_WIFI?"ZION / Wi-Fi":"ZION / 安装";
    LABEL(title,heading);
    if(menu.page==ZION_LIST) {
        if(apps) LABEL(content,"");
        int shown=zion_menu_visible(apps,menu.first);
        for(int i=0;i<shown;i++) {
            int s=slots[menu.first+i];
            if(zion_runtime_name(s)) {
                snprintf(line,sizeof(line),"%s%s",zion_runtime_name(s),s==def?" *":"");
                row((unsigned)i,zion_menu_row_y(i),line,s==ZION_BUILTIN_SLOT?"NATIVE  BUILT-IN":"ELF  RV32IM",menu.selected==menu.first+i);
                continue;
            }
            const meta_carve_slot_t *item=&catalog.slot[s];
            snprintf(line,sizeof(line),"%s",item->name[0]?item->name:"未完成玩法");
            snprintf(meta,sizeof(meta),"#%d  %luK  %s",s+1,(unsigned long)(item->size/1024),item->state==META_SLOT_VALID?versions[s]:"-");
            row((unsigned)i,zion_menu_row_y(i),line,meta,menu.selected==menu.first+i);
            lv_obj_set_width(names[i],s==def?188:208);
            if(s==def) {
                lv_point_t extent;lv_text_get_size(&extent,line,ZION_FONT,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
                lv_obj_set_pos(marks[i],7+(extent.x<188?extent.x:188),-2);lv_obj_clear_flag(marks[i],LV_OBJ_FLAG_HIDDEN);
            }
        }
        if(!apps) {LABEL(content,"尚未安装玩法");lv_obj_set_pos(content,12,44);}
        int y=zion_menu_row_y(shown?shown:1);
        row(4,y,"+ 安装玩法",NULL,menu.selected==apps);
        row(5,y+38,"> 设置",NULL,menu.selected==apps+1);
        int installed=1+(int)zion_package_count();for(int i=installed;i<apps;i++) if(catalog.slot[slots[i]].state==META_SLOT_VALID) installed++;
        snprintf(line,sizeof(line),"已安装%d款",installed);label_update(stats[0],stats_text[0],sizeof(stats_text[0]),line);
        snprintf(line,sizeof(line),"空闲%luK",(unsigned long)(meta_carve_free(&catalog)/1024));label_update(stats[1],stats_text[1],sizeof(stats_text[1]),line);
        snprintf(line,sizeof(line),"连续%luK",(unsigned long)(meta_carve_largest_gap(&catalog)/1024));label_update(stats[2],stats_text[2],sizeof(stats_text[2]),line);
    } else if(menu.page==ZION_MANAGE) {
        const char *items[]={"运行一次",target()==def?"取消默认":"设为默认",target()==ZION_BUILTIN_SLOT?"内置玩法不可删除":"删除玩法","返回"};
        for(unsigned i=0;i<4;i++) {
            row(i,44+(int)i*44,items[i],NULL,menu.selected==(int)i);
            if(i<2 && !target_ready()) ui_pixel_set_selected(rows[i],menu.selected==(int)i,false);
        }
        lv_obj_set_pos(content,10,230);LABEL(content,"* 表示开机默认玩法");
        LABEL(footer,"短按OK选择  长按OK返回");
    } else if(menu.page==ZION_DELETE || menu.page==ZION_CANCEL) {
        lv_obj_set_pos(content,10,48);
        if(menu.page==ZION_DELETE) {
            const char *name=zion_runtime_name(target());
            if(!name) name=target()>=0?catalog.slot[target()].name:"玩法";
            snprintf(line,sizeof(line),"删除：%.40s\n玩法及关联资源将被清除\n此操作无法撤销",name);LABEL(content,line);
        } else LABEL(content,"取消当前安装？\n未完成的玩法不会启动");
        row(0,174,"返回",NULL,menu.selected==0);
        row(1,218,menu.page==ZION_DELETE?"确认删除":"确认取消",NULL,menu.selected==1);
        LABEL(footer,"短按OK确认  长按OK返回");
    } else if(menu.page==ZION_SETTINGS) {
        const char *items[]={"Wi-Fi 状态与配网","更换 Wi-Fi","返回"};
        for(unsigned i=0;i<3;i++) row(i,44+(int)i*44,items[i],NULL,menu.selected==(int)i);
        snprintf(line,sizeof(line),"版本 %s\nWi-Fi：%s",esp_app_get_description()->version,service.online?"已连接":"未连接");
        lv_obj_set_pos(content,10,192);LABEL(content,line);
        LABEL(footer,"短按OK选择  长按OK返回");
    } else if(menu.page==ZION_WIFI) {
        lv_obj_set_pos(content,10,44);
        if(!service.ap && service.online) snprintf(line,sizeof(line),"Wi-Fi 已连接\n\n配网完成\n返回列表即可安装玩法");
        else if(!service.ap && service.result!=ESP_OK) snprintf(line,sizeof(line),"配网热点启动失败\n\n%s\n\n也可从官方小程序配网",service.detail);
        else snprintf(line,sizeof(line),"Wi-Fi：%s\n\n手机连接：\n%s\n无需密码\n\n浏览器打开\n192.168.4.1\n也可从官方小程序配网",
            service.online?"已连接":"未连接",service.ap?service.ap_name:"正在开启配网");
        LABEL(content,line);LABEL(footer,"短按OK返回  长按OK返回");
    } else {
        lv_obj_set_pos(content,10,48);
        if(show_qr) {
            if(!qr) {
                qr=lv_image_create(screen);lv_image_set_src(qr,&zion_official_mini_code);
                lv_obj_set_pos(qr,16,60);
                printf("ZION_COMMUNITY_QR kind=official-mini-code size=208\n");fflush(stdout);
            }
            lv_obj_set_pos(content,10,40);
            LABEL(content,"微信扫码打开小程序");
            if(qr) lv_obj_move_foreground(qr);
            lv_obj_set_pos(install_hint,10,270);
            snprintf(line,sizeof(line),"Wi-Fi：%s  蓝牙：%s",service.online?"已连接":"未连接",service.connected?"已连接":"待连接");
            LABEL(install_hint,line);
        } else if(service.busy || service.stage==255 || service.stage==100) {
            zion_install_format(line,sizeof(line),&service.install,service.stage,service.percent,service.detail);
            LABEL(content,line);
        } else {
            snprintf(line,sizeof(line),"在官方玩法社区选择玩法\n点击小程序安装\n选择已配对的设备\n\nWi-Fi：%s\n蓝牙：%s",service.online?"已连接":"请先在设置中配网",service.connected?"已连接":"等待手机连接");
            LABEL(content,line);
        }
        LABEL(footer,service.busy?"长按OK取消安装":"短按OK返回  长按OK返回");
    }
    if(service.busy && service.stage==200) {
        for(unsigned i=0;i<ROWS;i++) lv_obj_add_flag(rows[i],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(content,10,70);LABEL(content,"正在处理，请稍候");
        LABEL(footer,"请等待操作完成");
    }
    int soc=bsp_battery_soc();if(soc>100) soc=100;
    snprintf(line,sizeof(line),soc<0?"?":"%d%%",soc);LABEL(power,line);
    lv_obj_set_width(fill,soc>0?34*soc/100:0);
    bsp_lvgl_unlock();dirty=false;
}
void zion_launcher_init(void) {
    zion_service_get(&service);catalog_refresh();zion_menu_init(&menu);
    if(!bsp_lvgl_lock(1000)) return;
    screen=ui_pixel_screen_create("ZION");
    title=lv_obj_get_child(screen,0);
    for(unsigned i=0;i<ROWS;i++) {
        rows[i]=ui_pixel_panel_create_plain(screen,10,40,220,34,UI_PAPER);
        names[i]=ui_pixel_label(rows[i],"",ZION_FONT,UI_INK);
        lv_obj_set_width(names[i],208);lv_label_set_long_mode(names[i],LV_LABEL_LONG_DOT);
        marks[i]=ui_pixel_label(rows[i],"*",ZION_FONT,UI_INK);lv_obj_add_flag(marks[i],LV_OBJ_FLAG_HIDDEN);
        details[i]=ui_pixel_label(rows[i],"",ZION_FONT,UI_INK2);
        lv_obj_set_style_text_font(details[i],&lv_font_unscii_8,0);lv_obj_set_pos(details[i],3,18);
    }
    content=ui_pixel_label(screen,"",ZION_FONT,UI_INK);lv_obj_set_width(content,220);
    install_hint=ui_pixel_label(screen,"",ZION_FONT,UI_INK2);lv_obj_set_pos(install_hint,10,230);lv_obj_set_width(install_hint,220);
    footer=ui_pixel_label(screen,"",ZION_FONT,UI_INK);lv_obj_set_pos(footer,10,296);
    lv_obj_set_width(footer,220);lv_label_set_long_mode(footer,LV_LABEL_LONG_DOT);
    footer_right=ui_pixel_label(screen,"",ZION_FONT,UI_INK);lv_obj_set_pos(footer_right,120,296);
    lv_obj_set_width(footer_right,110);lv_label_set_long_mode(footer_right,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(footer_right,LV_TEXT_ALIGN_RIGHT,0);
    for(unsigned i=0;i<3;i++) {stats[i]=ui_pixel_label(screen,"",ZION_FONT,UI_INK2);lv_obj_set_pos(stats[i],10+(int)i*77,270);}
    battery=ui_pixel_panel_create_plain(screen,194,9,38,16,UI_SKY);
    lv_obj_set_style_pad_all(battery,0,0);
    fill=lv_obj_create(battery);lv_obj_remove_style_all(fill);lv_obj_set_pos(fill,1,1);lv_obj_set_size(fill,0,12);
    lv_obj_set_style_bg_opa(fill,LV_OPA_COVER,0);lv_obj_set_style_bg_color(fill,lv_color_hex(0x425637),0);
    power=ui_pixel_label(battery,"?",ZION_FONT,UI_INK);lv_obj_set_style_text_font(power,ZION_FONT_SMALL,0);lv_obj_center(power);
    lv_screen_load(screen);
    printf("ZION_FONTS ui_bpp=%u small_bpp=%u fallback_bpp=%u\n",
        ((const lv_font_fmt_txt_dsc_t *)ZION_FONT->dsc)->bpp,
        ((const lv_font_fmt_txt_dsc_t *)ZION_FONT_SMALL->dsc)->bpp,
        ((const lv_font_fmt_txt_dsc_t *)zion_font_14.dsc)->bpp);
#if CONFIG_META_E2E_TEST_CONTROL
    if(!zion_font_selftest()) abort();
#endif
    bsp_lvgl_unlock();
    skip_once=service.skip_autostart;
    esp_reset_reason_t reason=esp_reset_reason();int mv=bsp_button_read_mv();
    startup_escape=mv>=0 && mv<2300;
    bool cold=reason==ESP_RST_POWERON || reason==ESP_RST_EXT || reason==ESP_RST_USB || reason==ESP_RST_JTAG;
    auto_boot=zion_should_autostart(zion_runtime_default_slot()!=-1 || (service.prefs.flags&ZION_PREFS_ENABLED)!=0,
        default_slot()!=-1,cold,skip_once || (mv>=0 && mv<2300),false);
    deadline=esp_timer_get_time()+1800000;
    if(auto_boot) snprintf(notice,sizeof(notice),"即将自启，任意键取消");
    else if(reason==ESP_RST_PANIC || reason==ESP_RST_TASK_WDT || reason==ESP_RST_INT_WDT || reason==ESP_RST_WDT)
        snprintf(notice,sizeof(notice),"玩法异常复位，已留在Zion");
    else if((service.prefs.flags&ZION_PREFS_ENABLED) && zion_default_find(&catalog,&service.prefs)<0)
        snprintf(notice,sizeof(notice),"默认玩法已变更，请重选");
    dirty=true;render();
}
void zion_launcher_fonts(void) {
    if(!bsp_lvgl_lock(1000)) return;
    printf("ZION_FONTS ui_bpp=%u small_bpp=%u fallback_bpp=%u\n",
        ((const lv_font_fmt_txt_dsc_t *)ZION_FONT->dsc)->bpp,
        ((const lv_font_fmt_txt_dsc_t *)ZION_FONT_SMALL->dsc)->bpp,
        ((const lv_font_fmt_txt_dsc_t *)zion_font_14.dsc)->bpp);
#if CONFIG_META_E2E_TEST_CONTROL
    (void)zion_font_selftest();
#endif
    fflush(stdout);bsp_lvgl_unlock();
}
void zion_launcher_tick(void) {
    /* The boot-held escape key must not swallow the user's next, unrelated
     * click after it has been released (also matters after a plugin reset). */
    if(startup_escape && bsp_button_read_mv()>=2300) startup_escape=false;
    zion_service_get(&service);
    if(service.install_id!=install_id) {
        install_id=service.install_id;menu.page=ZION_INSTALL;install_result_dismissed=false;pending=false;dirty=true;
    }
    if(service.connected || service.busy) auto_boot=false;
    if(service.generation!=generation) {
        generation=service.generation;catalog_refresh();dirty=true;
        if(menu.page==ZION_CANCEL && (service.stage==255 || service.stage==100)) menu.page=ZION_INSTALL;
        if(pending && !service.busy) {
            pending=false;
            if(!strcmp(service.detail,"removed-cleanup-pending")) snprintf(notice,sizeof(notice),"玩法已移除，清理未完成");
            else snprintf(notice,sizeof(notice),service.result==ESP_OK?"操作完成":"操作失败：%s",service.detail);
            if(pending_action==ZION_REMOVE) {zion_menu_init(&menu);}
        }
    }
    if(service.busy && service.stage<200 && menu.page!=ZION_CANCEL) {menu.page=ZION_INSTALL;dirty=true;}
    if(auto_boot && esp_timer_get_time()>=deadline) {
        auto_boot=false;
        if(zion_runtime_name(default_slot())) (void)zion_runtime_request(default_slot());
        else (void)zion_service_request(ZION_BOOT,default_slot());
    }
    if(esp_timer_get_time()-last_refresh>=500000) {last_refresh=esp_timer_get_time();dirty=true;}
    if(dirty) render();
}
void zion_launcher_snapshot(void) {
    int marker=-1,aligned=0,qr_visible=0,settings_marked=0,footer_fixed=0,footer_edge=0;
    if(bsp_lvgl_lock(200)) {
        lv_obj_update_layout(screen);
        for(int i=0;i<4;i++) if(!lv_obj_has_flag(rows[i],LV_OBJ_FLAG_HIDDEN) && !lv_obj_has_flag(marks[i],LV_OBJ_FLAG_HIDDEN)) marker=i;
        aligned=lv_obj_get_x(rows[0])==lv_obj_get_x(rows[4]) && lv_obj_get_width(rows[0])==lv_obj_get_width(rows[4]);
        qr_visible=qr && !lv_obj_has_flag(qr,LV_OBJ_FLAG_HIDDEN) && lv_obj_get_width(qr)>0;
        settings_marked=!strcmp(lv_label_get_text(names[5]),"> 设置");
        footer_edge=(int)(lv_obj_get_x(footer_right)+lv_obj_get_width(footer_right));
        footer_fixed=menu.page==ZION_LIST && !strcmp(lv_label_get_text(footer),"短按OK启动") &&
            !strcmp(lv_label_get_text(footer_right),"长按OK管理") && footer_edge==230 &&
            lv_obj_get_style_text_align(footer_right,0)==LV_TEXT_ALIGN_RIGHT &&
            !lv_obj_has_flag(footer_right,LV_OBJ_FLAG_HIDDEN);
        bsp_lvgl_unlock();
    }
    printf("ZION_UI page=%d selected=%d apps=%d target=%d default=%d auto=%d busy=%d online=%d ap=%d free=%lu largest=%lu result=%d marker=%d aligned=%d qr=%d settings_marked=%d footer_fixed=%d footer_right=%d\n",
        menu.page,menu.selected,apps,target(),default_slot(),auto_boot,service.busy,service.online,service.ap,
        (unsigned long)meta_carve_free(&catalog),(unsigned long)meta_carve_largest_gap(&catalog),(int)service.result,marker,aligned,qr_visible,settings_marked,footer_fixed,footer_edge);
    fflush(stdout);
    if(menu.page==ZION_INSTALL) {
        printf("ZION_INSTALL_UI id=%u stage=%u percent=%u name=%s package=%lu installed=%lu known=%d reason=%s cleanup=%u cleanup_error=%d\n",
            service.install_id,service.stage,service.percent,service.install.name,
            (unsigned long)service.install.package_bytes,(unsigned long)service.install.install_bytes,
            service.install.install_known,service.detail,service.install.cleanup,service.install.cleanup_error);fflush(stdout);
        printf("ZION_TRANSFER_UI bytes=%lu layout=%lu bps=%lu ms=%lu read_ms=%lu write_ms=%lu\n",
            (unsigned long)service.install.received_bytes,(unsigned long)service.install.layout_bytes,
            (unsigned long)service.install.bytes_per_second,(unsigned long)service.install.elapsed_ms,
            (unsigned long)service.install.read_ms,(unsigned long)service.install.write_ms);fflush(stdout);
    }
}

#if CONFIG_META_E2E_TEST_CONTROL && LV_USE_SNAPSHOT
/* Test-only RGB565 render, not the I1 source or the object's visibility flag.
 * Only the official public Mini Program code is exported, never private pages. */
void zion_launcher_qr_pixels(void) {
    if(!bsp_lvgl_lock(1000)) {printf("ZION_QR_PIXELS error=lock\n");return;}
    if(!qr) {bsp_lvgl_unlock();printf("ZION_QR_PIXELS error=no-qr\n");return;}
    unsigned width=(unsigned)lv_obj_get_width(qr),height=(unsigned)lv_obj_get_height(qr);
    int32_t old_y=lv_image_get_offset_y(qr);
    lv_image_align_t old_align=lv_image_get_inner_align(qr);
    bsp_lvgl_unlock();
    static char hex_row[1024];
    /* Eight rows need ~2.4 KiB instead of a ~44 KiB contiguous allocation.
     * Restore the live object's geometry before releasing the LVGL lock. */
    for(unsigned base=0;base<height;base+=8) {
        if(!bsp_lvgl_lock(1000)) {printf("ZION_QR_PIXELS error=lock\n");return;}
        unsigned count=height-base<8?height-base:8;
        lv_obj_set_height(qr,(int32_t)count);lv_image_set_inner_align(qr,LV_IMAGE_ALIGN_TOP_LEFT);
        lv_image_set_offset_y(qr,-(int32_t)base);
        lv_draw_buf_t *buf=lv_snapshot_take(qr,LV_COLOR_FORMAT_RGB565);
        lv_obj_set_height(qr,(int32_t)height);lv_image_set_inner_align(qr,old_align);lv_image_set_offset_y(qr,old_y);
        lv_obj_update_layout(qr);
        if(!buf) {bsp_lvgl_unlock();printf("ZION_QR_PIXELS error=snapshot\n");fflush(stdout);return;}
        unsigned stride=buf->header.stride;
        if(!base) printf("ZION_QR_PIXELS width=%u height=%u stride=%u\n",width,height,stride);
        if(stride*2+1>sizeof(hex_row) || stride*count>buf->data_size) {
            lv_draw_buf_destroy(buf);bsp_lvgl_unlock();printf("ZION_QR_PIXELS error=shape\n");return;
        }
        bsp_lvgl_unlock();
        const char digits[]="0123456789abcdef";
        for(unsigned y=0;y<count;y++) {
            for(unsigned x=0;x<stride;x++) {
                uint8_t b=buf->data[y*stride+x];hex_row[2*x]=digits[b>>4];hex_row[2*x+1]=digits[b&15];
            }
            hex_row[2*stride]=0;printf("ZION_QR_ROW y=%u data=%s\n",base+y,hex_row);
        }
        if(bsp_lvgl_lock(1000)) {lv_draw_buf_destroy(buf);bsp_lvgl_unlock();}
    }
    printf("ZION_QR_END\n");fflush(stdout);
}
#else
void zion_launcher_qr_pixels(void) {}
#endif

void zion_launcher_suspend(void) {auto_boot=false;dirty=false;screen=NULL;}
