/* Zion device services and 592 v4 wire adapter, based on FoloToy/ai-passport-miniapp-installer (MIT).
 * Capture-only unless CONFIG_ZION_592_INSTALL enables the verified backend.
 * Legacy addresses below are wire-profile values, never local write addresses.
 */
#include "sdkconfig.h"
#ifndef CONFIG_ZION_592_INSTALL
#define CONFIG_ZION_592_INSTALL 0
#endif
#include "zion592_wire.h"
#if CONFIG_ZION_LAUNCHER
#include "zion_service.h"
#include "zion_builtin.h"
#include "zion_runtime.h"
#include "zion_runtime_model.h"
#include "zion_package.h"
#include "zion_idf_compat.h"

#include "zion_manage.h"
#include "zion_prefs.h"
#include "zion_wifi.h"
#endif
#include "meta_carve.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#if CONFIG_ZION_592_INSTALL
#include "zion592_install.h"
#include "meta_carve_flash.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#endif
#include "ui_pixel.h"
#include "esp_event.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_bt.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "driver/usb_serial_jtag.h"
#include <fcntl.h>
#include <unistd.h>
#if CONFIG_META_E2E_TEST_CONTROL
#include "esp_coexist.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

enum { IDLE=0, VERIFYING=2, READY=3, ERROR=4, CONNECTING=5, ONLINE=6, DOWNLOADING=7 };
typedef struct { uint8_t state,error; uint16_t reserved; uint32_t received,total; } __attribute__((packed)) status_t;
typedef struct {
    char magic[4]; uint8_t protocol,flags; uint16_t version;
    uint32_t flash,netcfg,recovery,nonce; char sn[13]; uint8_t reserved[3];
} __attribute__((packed)) info_t;
_Static_assert(sizeof(info_t)==40,"592 info ABI");
_Static_assert(sizeof(status_t)==12,"592 status ABI");
static const char *TAG="z592";
#if ZION_RUNTIME_TEST_KEYS
static void allocation_failed(size_t bytes,uint32_t caps,const char *name) {
    ESP_EARLY_LOGE("zion-memory","ALLOC_FAILED bytes=%u caps=0x%lx caller=%s task=%s free=%u largest=%u",
        (unsigned)bytes,(unsigned long)caps,name,pcTaskGetName(NULL),
        (unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(caps));
}
#endif
static char s_name[13];
static uint8_t s_addr_type;
static uint16_t s_conn=BLE_HS_CONN_HANDLE_NONE,s_status_handle,s_scan_handle;
static QueueHandle_t s_jobs;
static EventGroupHandle_t s_wifi_bits;
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static status_t s_status;
static unsigned s_connections,s_info_reads,s_scans,s_captures;
static unsigned s_status_reads,s_status_notifies,s_control_writes,s_secure_writes,s_busy_rejects;
static unsigned s_rejects,s_last_reject;
static bool s_connected,s_wifi_online;
static uint32_t s_capture_size,s_capture_source;
static bool s_busy,s_native_claimed;
static atomic_bool s_ble_running;
static atomic_bool s_native_keep_wifi=true;
static atomic_bool s_native_worker_stop;
static atomic_bool s_native_console_stop;
static atomic_bool s_native_owns_usb;
static unsigned s_install_stage,s_percent,s_completed;
static int s_installed_slot=-1;
static char s_install_detail[64];
#if CONFIG_ZION_592_INSTALL
static zion_install_info_t s_install_info;
static int64_t s_install_update_us;
static unsigned s_install_id;
#endif
#if CONFIG_ZION_592_INSTALL
static int64_t s_boot_at;
#if !CONFIG_ZION_LAUNCHER
static bool s_show_list;
static int s_selected;
#endif
typedef struct {bsp_btn_t button;bsp_btn_ev_t event;} zion_button_msg_t;
static QueueHandle_t s_keys;
static atomic_bool s_keys_ready;
#endif
static z592_transfer_t s_transfer;
#if !CONFIG_ZION_LAUNCHER
static lv_obj_t *s_screen,*s_label,*s_stats,*s_battery;
#endif
#if CONFIG_ZION_592_INSTALL
static void on_key(bsp_btn_t button,bsp_btn_ev_t event,void *context);
#if CONFIG_ZION_LAUNCHER && (CONFIG_META_E2E_TEST_CONTROL || ZION_RUNTIME_TEST_KEYS)
static void inject_key(bsp_btn_t button,bsp_btn_ev_t event) {
    if(zion_runtime_active() && zion_idf_polling_buttons()) {
        extern void zion_test_adc_key(unsigned,bool);
        zion_test_adc_key(button,event==BSP_BTN_LONG);return;
    }
    on_key(button,BSP_BTN_PRESS,NULL);
    if(event==BSP_BTN_CLICK) on_key(button,BSP_BTN_RELEASE,NULL);
    on_key(button,event,NULL);
    if(event==BSP_BTN_LONG) {on_key(button,BSP_BTN_RELEASE,NULL);on_key(button,BSP_BTN_CLICK,NULL);}
    if(zion_runtime_active()) zion_idf_inject_component_key(button,event);
}
static void test_edge_dispatch(unsigned key,unsigned phase) {on_key((bsp_btn_t)key,(bsp_btn_ev_t)phase,NULL);}
#endif
static esp_err_t boot_child(int slot);
#endif
#if CONFIG_ZION_LAUNCHER
static zion_prefs_t s_prefs;
static unsigned s_generation;
static esp_err_t s_result;
static bool s_ap,s_skip_autostart,s_snapshot_requested,s_boot_requested;
static char s_ap_name[33],s_ap_password[9];
#if CONFIG_META_E2E_TEST_CONTROL
static bool s_qr_pixels_requested;
static atomic_bool s_test_hold_boot;
#endif
#endif

#if CONFIG_ZION_592_INSTALL
static bool install_autoboot_allowed(void) {
#if CONFIG_ZION_LAUNCHER
    /* Keep BLE and the manager alive for another installation. Only a device
     * OK action may launch the completed app, including after protocol cmd 4. */
    return false;
#else
    return true;
#endif
}
#endif
static void radio_snapshot(const char *reason) {
    if(!atomic_load(&s_ble_running)) {ESP_LOGI(TAG,"ZION_RADIO inactive reason=%s",reason);return;}
    struct ble_gap_conn_desc conn;wifi_ps_type_t ps=WIFI_PS_MIN_MODEM;wifi_ap_record_t ap={0};
    int gap=ble_gap_conn_find(s_conn,&conn);esp_err_t power=esp_wifi_get_ps(&ps),wifi=esp_wifi_sta_get_ap_info(&ap);
    ESP_LOGI(TAG,"Z592_LINK reason=%s ble=%d interval_us=%lu latency=%u timeout_ms=%lu ps=%d power_result=%d rssi=%d channel=%u wifi_result=%d wnd=%u mbox=%u",
        reason,gap,(unsigned long)(gap?0:conn.conn_itvl*1250u),(unsigned)(gap?0:conn.conn_latency),
        (unsigned long)(gap?0:conn.supervision_timeout*10u),(int)ps,(int)power,(int)ap.rssi,(unsigned)ap.primary,(int)wifi,
        (unsigned)CONFIG_LWIP_TCP_WND_DEFAULT,(unsigned)CONFIG_LWIP_TCP_RECVMBOX_SIZE);
}
static void reject_record(unsigned stage,unsigned command,unsigned length) {
    portENTER_CRITICAL(&s_lock);s_rejects++;s_last_reject=stage;portEXIT_CRITICAL(&s_lock);
    ESP_LOGW(TAG,"Z592 REJECT stage=%u cmd=0x%02x len=%u",stage,command,length);
}
static void snapshot_print(void) {
    status_t status;unsigned conn,info,scan,capture,rejects,stage;uint32_t file_bytes;bool online;
    portENTER_CRITICAL(&s_lock);
    status=s_status;conn=s_connections;info=s_info_reads;scan=s_scans;capture=s_captures;
    rejects=s_rejects;stage=s_last_reject;online=s_wifi_online;file_bytes=s_capture_size;
    portEXIT_CRITICAL(&s_lock);
    const esp_app_desc_t *desc=esp_app_get_description();
    printf("Z592_SNAPSHOT state=%u error=%u wifi=%u connections=%u info=%u scans=%u captures=%u rejects=%u last_reject=%u bytes=%lu completed=%u stage=%u slot=%d heap=%lu largest=%lu elf=%02x%02x%02x%02x\n",
        status.state,status.error,online,conn,info,scan,capture,rejects,stage,(unsigned long)file_bytes,
        s_completed,s_install_stage,s_installed_slot,(unsigned long)esp_get_free_heap_size(),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        desc->app_elf_sha256[0],desc->app_elf_sha256[1],desc->app_elf_sha256[2],desc->app_elf_sha256[3]);
    fflush(stdout);
    portENTER_CRITICAL(&s_lock);
    unsigned reads=s_status_reads,notifies=s_status_notifies,controls=s_control_writes,secure=s_secure_writes,busy=s_busy_rejects;
    portEXIT_CRITICAL(&s_lock);
    printf("Z592_GATT reads=%u notifies=%u controls=%u secure=%u busy_rejects=%u\n",reads,notifies,controls,secure,busy);fflush(stdout);
}
/* Keep USB control reachable; large installation stacks are no longer needed.
 * Installing during a running app is rejected rather than touching its Flash. */
void zion_service_native_control(const char *line) {
#if ZION_RUNTIME_TEST_KEYS
            unsigned edge_key,edge_phase;
            if(sscanf(line,"ZION ADC %u %u",&edge_key,&edge_phase)==2) {
                extern bool zion_test_adc_pulse(unsigned,unsigned);
                bool valid=zion_runtime_active() && zion_test_adc_pulse(edge_key,edge_phase);
                printf("ZION_ADC_DONE key=%u ms=%u result=%d\n",edge_key,edge_phase,valid?0:-1);
            }
            if(sscanf(line,"ZION PULSE %u %u",&edge_key,&edge_phase)==2) {
                extern bool zion_test_button_pulse(unsigned,unsigned,void (*)(unsigned,unsigned));
                if(!zion_runtime_active() || edge_key>=bsp_button_configured_count() ||
                   !zion_test_button_pulse(edge_key,edge_phase,test_edge_dispatch)) puts("ZION_PULSE_FAIL");
            }
            if(sscanf(line,"ZION EDGE %u %u",&edge_key,&edge_phase)==2) {
                bool valid=zion_runtime_active() && edge_key<bsp_button_configured_count() &&
                    (edge_phase==BSP_BTN_PRESS || edge_phase==BSP_BTN_RELEASE);
                if(valid) on_key((bsp_btn_t)edge_key,(bsp_btn_ev_t)edge_phase,NULL);
                printf("ZION_EDGE_DONE key=%u phase=%u result=%d\n",edge_key,edge_phase,valid?0:-1);
            }
            if(!strcmp(line,"ZION KEY UP")) inject_key(BSP_BTN_UP,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY DOWN")) inject_key(BSP_BTN_DOWN,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY OK")) inject_key(BSP_BTN_OK,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY LONG")) inject_key(BSP_BTN_OK,BSP_BTN_LONG);
            if(!strcmp(line,"ZION KEY UPLONG")) inject_key(BSP_BTN_UP,BSP_BTN_LONG);
            if(!strcmp(line,"ZION KEY DOWNLONG")) inject_key(BSP_BTN_DOWN,BSP_BTN_LONG);
#endif
            if(!strncmp(line,"ZION INSTALL ",13) || !strncmp(line,"ZION LEGACY ",12))
                printf("ZION_UPLOAD_DONE result=-1 stage=application-active\n");
            if(!strcmp(line,"ZION MEM")) {
                multi_heap_info_t info;heap_caps_get_info(&info,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
                printf("ZION_MEMORY free=%u allocated=%u largest=%u\n",(unsigned)info.total_free_bytes,
                    (unsigned)info.total_allocated_bytes,(unsigned)info.largest_free_block);
            }
#if ZION_RUNTIME_TEST_KEYS
            extern bool zion_test_capture_begin(void);
            extern void zion_test_tasks(void);
            if(!strcmp(line,"ZION CAPTURE")) printf("ZION_LCD_ARMED result=%d\n",zion_test_capture_begin());
            if(!strcmp(line,"ZION TASKS")) zion_test_tasks();
            extern void zion_test_runtime_trace(bool);
            if(!strcmp(line,"ZION TRACE ON")) zion_test_runtime_trace(true);
            if(!strncmp(line,"ZION KEY ",9)) printf("ZION_KEY_DONE remaining_stack=%u\n",(unsigned)uxTaskGetStackHighWaterMark(NULL));
#endif
            fflush(stdout);
}
static void native_console(void *arg) {
    (void)arg;
    char line[32];
    unsigned used=0;
    for(;;) {
        int c=getchar();
        if(c==EOF) {clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(20));continue;}
        if(c=='\n' || c=='\r') {
            line[used]=0;
            zion_service_native_control(line);
            used=0;
        } else if(used<sizeof(line)-1) line[used++]=(char)c;
        else used=0;
    }
}
static void console_worker(void *arg) {
    (void)arg;char line[128];unsigned used=0;
#if !ZION_RUNTIME_SIM
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config = {
            .rx_buffer_size = 1024, .tx_buffer_size = 1024
        };
        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
    }
    usb_serial_jtag_vfs_use_driver();
    int flags = fcntl(fileno(stdin), F_GETFL);
    configASSERT(flags >= 0 && fcntl(fileno(stdin), F_SETFL, flags | O_NONBLOCK) == 0);
    ESP_LOGI(TAG, "USB_BUFFERED rx=1024 tx=1024");
#endif
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    for(;;) {
        if(atomic_load(&s_native_console_stop)) {
            if(!atomic_load(&s_native_owns_usb))
                configASSERT(xTaskCreate(native_console,"zion-control",ZION_RUNTIME_TEST_KEYS?4096:2048,NULL,3,NULL)==pdPASS);
            xEventGroupSetBits(s_wifi_bits,BIT3);
            vTaskDelete(NULL);
        }
        int c=getchar();
        if(c==EOF) {clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(20));continue;}
        if(c=='\n' || c=='\r') {
            line[used]=0;
            unsigned long upload_size;
            char upload_sha[65];
            if(sscanf(line,"ZION LEGACY %lu %64s",&upload_size,upload_sha)==2) {
                uint8_t digest[32];bool valid=strlen(upload_sha)==64;
                for(unsigned i=0;i<32 && valid;i++) {unsigned value;if(sscanf(upload_sha+i*2,"%2x",&value)!=1) valid=false;else digest[i]=(uint8_t)value;}
                if(valid) (void)zion_legacy_receive((uint32_t)upload_size,digest);
            }
            if(sscanf(line,"ZION INSTALL %lu",&upload_size)==1) (void)zion_package_receive((uint32_t)upload_size);
            if(!strcmp(line,"Z592 STATE") || !strcmp(line,"Z592 UI")) snapshot_print();
#if CONFIG_ZION_LAUNCHER
            if(!strcmp(line,"ZION NET")) {
                esp_netif_t *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");esp_netif_ip_info_t ip;
                if(sta && esp_netif_get_ip_info(sta,&ip)==ESP_OK) printf("ZION_NET address=" IPSTR "\n",IP2STR(&ip.ip));
                fflush(stdout);
            }
            if(!strcmp(line,"ZION STATE")) {portENTER_CRITICAL(&s_lock);s_snapshot_requested=true;portEXIT_CRITICAL(&s_lock);}
            if(!strcmp(line,"ZION FONTS")) zion_launcher_fonts();
            if(!strcmp(line,"ZION RADIO")) radio_snapshot("query");
#if ZION_RUNTIME_TEST_KEYS
            if(!strcmp(line,"ZION KEY UP")) inject_key(BSP_BTN_UP,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY DOWN")) inject_key(BSP_BTN_DOWN,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY OK")) inject_key(BSP_BTN_OK,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY LONG")) inject_key(BSP_BTN_OK,BSP_BTN_LONG);
            if(!strcmp(line,"ZION KEY UPLONG")) inject_key(BSP_BTN_UP,BSP_BTN_LONG);
            if(!strcmp(line,"ZION KEY DOWNLONG")) inject_key(BSP_BTN_DOWN,BSP_BTN_LONG);
            if(!strcmp(line,"ZION STAY")) {
                s_prefs.flags|=ZION_PREFS_SKIP_ONCE;
                printf("ZION_STAY result=%d\n",(int)zion_prefs_save(&s_prefs));fflush(stdout);
            }
#endif
#if CONFIG_META_E2E_TEST_CONTROL
            if(!strcmp(line,"ZION QR")) {portENTER_CRITICAL(&s_lock);s_qr_pixels_requested=true;portEXIT_CRITICAL(&s_lock);}
            if(!strcmp(line,"ZION PS ON")) z592_install_test_power_save(1);
            if(!strcmp(line,"ZION PS OFF")) z592_install_test_power_save(0);
            if(!strcmp(line,"ZION HOLD ON")) atomic_store(&s_test_hold_boot,true);
            if(!strcmp(line,"ZION HOLD OFF")) atomic_store(&s_test_hold_boot,false);
            if(!strcmp(line,"ZION BLE SLOW")) {
                struct ble_gap_upd_params params={.itvl_min=80,.itvl_max=160,.latency=0,.supervision_timeout=600};
                int result=ble_gap_update_params(s_conn,&params);
                ESP_LOGI(TAG,"Z592_TEST_LINK request=100-200ms result=%d",result);radio_snapshot("update-request");
            }
            if(!strcmp(line,"ZION COEX WIFI") || !strcmp(line,"ZION COEX BALANCE")) {
                esp_coex_prefer_t mode=!strcmp(line,"ZION COEX WIFI")?ESP_COEX_PREFER_WIFI:ESP_COEX_PREFER_BALANCE;
                esp_err_t result=esp_coex_preference_set(mode);
                ESP_LOGI(TAG,"Z592_TEST_COEX mode=%d result=%d",(int)mode,(int)result);
            }
#if !ZION_RUNTIME_TEST_KEYS
            if(!strcmp(line,"ZION KEY UP")) inject_key(BSP_BTN_UP,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY DOWN")) inject_key(BSP_BTN_DOWN,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY OK")) inject_key(BSP_BTN_OK,BSP_BTN_CLICK);
            if(!strcmp(line,"ZION KEY LONG")) inject_key(BSP_BTN_OK,BSP_BTN_LONG);
            if(!strcmp(line,"ZION KEY UPLONG")) inject_key(BSP_BTN_UP,BSP_BTN_LONG);
            if(!strcmp(line,"ZION KEY DOWNLONG")) inject_key(BSP_BTN_DOWN,BSP_BTN_LONG);
#endif
#endif
#endif
            used=0;
        } else if(used<sizeof(line)-1) line[used++]=(char)c;
        else used=0;
    }
}

static status_t status_get(void) {
    status_t copy; portENTER_CRITICAL(&s_lock);copy=s_status;portEXIT_CRITICAL(&s_lock);return copy;
}
static void notify_status(void) {
    if(!atomic_load(&s_ble_running)) return;
    portENTER_CRITICAL(&s_lock);s_status_notifies++;portEXIT_CRITICAL(&s_lock);
    if (s_conn==BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG,"Z592 STATUS_NOTIFY skipped=no_connection");return;
    }
    status_t copy=status_get();
    struct os_mbuf *om=ble_hs_mbuf_from_flat(&copy,sizeof(copy));
    if (om) {
        int rc=ble_gatts_notify_custom(s_conn,s_status_handle,om);
        ESP_LOGI(TAG,"Z592 STATUS_NOTIFY rc=%d",rc);
    }
}
static void state_set(uint8_t state,uint8_t error) {
    portENTER_CRITICAL(&s_lock);s_status.state=state;s_status.error=error;
#if CONFIG_ZION_LAUNCHER
    s_generation++;
#endif
    portEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG,"Z592 STATE state=%u error=%u backend=%d heap=%lu largest=%lu",state,error,CONFIG_ZION_592_INSTALL,
        (unsigned long)esp_get_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    notify_status();
}
static bool layout_valid(void) {
    const esp_partition_t *app=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_FACTORY,NULL);
    const esp_partition_t *card=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"cardid");
    const esp_partition_t *store=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"store");
    return app && card && store && app->address==0x10000 &&
        app->size==(CONFIG_ZION_592_INSTALL?META_FACTORY_SIZE:0x170000) &&
        card->address==0x356000 && store->address==0x35a000;
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;(void)data;
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        portENTER_CRITICAL(&s_lock);s_wifi_online=true;zion_runtime_network(true);portEXIT_CRITICAL(&s_lock);
        xEventGroupSetBits(s_wifi_bits,BIT0);
        state_set(ONLINE,0);
    } else if (base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        portENTER_CRITICAL(&s_lock);s_wifi_online=false;zion_runtime_network(false);portEXIT_CRITICAL(&s_lock);
        xEventGroupClearBits(s_wifi_bits,BIT0);
    }
}
static void scan_networks(bool web) {
    uint16_t count=16;
    wifi_ap_record_t *records=calloc(count,sizeof(*records));
    esp_err_t err=records?esp_wifi_scan_start(NULL,true):ESP_ERR_NO_MEM;
    if(err==ESP_OK) err=esp_wifi_scan_get_ap_records(&count,records);
#if CONFIG_ZION_LAUNCHER
    if(web) {
        zion_wifi_scan_complete(records,err==ESP_OK?count:0,err);free(records);
        ESP_LOGI(TAG,"ZION WEB_SCAN count=%u result=%s",err==ESP_OK?count:0,esp_err_to_name(err));return;
    }
#else
    (void)web;
#endif
    if(err!=ESP_OK) {free(records);state_set(ERROR,1);return;}
    if (err==ESP_OK) for (uint16_t i=0;i<count;i++) {
        records[i].ssid[32]=0;
        uint8_t packet[35]={(uint8_t)records[i].authmode,(uint8_t)records[i].rssi,0};
        packet[2]=(uint8_t)strlen((const char *)records[i].ssid);
        memcpy(packet+3,records[i].ssid,packet[2]);
        struct os_mbuf *om=ble_hs_mbuf_from_flat(packet,3+packet[2]);
        if (om && s_conn!=BLE_HS_CONN_HANDLE_NONE) {
            int rc=ble_gatts_notify_custom(s_conn,s_scan_handle,om);
            if(rc) ESP_LOGW(TAG,"Z592 SCAN_NOTIFY rc=%d bytes=%u",rc,3+packet[2]);
        }
        else if (om) os_mbuf_free_chain(om);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    free(records);
    uint8_t end[3]={0xff,0,0};
    struct os_mbuf *om=ble_hs_mbuf_from_flat(end,sizeof(end));
    if (om && s_conn!=BLE_HS_CONN_HANDLE_NONE) {
        int rc=ble_gatts_notify_custom(s_conn,s_scan_handle,om);
        ESP_LOGI(TAG,"Z592 SCAN_END_NOTIFY rc=%d",rc);
    }
    else if (om) os_mbuf_free_chain(om);
    portENTER_CRITICAL(&s_lock);s_scans++;portEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG,"Z592 SCAN_DONE count=%u rc=%s",count,esp_err_to_name(err));
}
static esp_err_t wifi_connect(const z592_request_t *job) {
#if CONFIG_ZION_LAUNCHER
    if(job->source_offset==0 && s_boot_requested) return ESP_ERR_INVALID_STATE;
#endif
    wifi_config_t cfg={0};
    memcpy(cfg.sta.ssid,job->ssid,strlen(job->ssid));memcpy(cfg.sta.password,job->password,strlen(job->password));
    cfg.sta.threshold.authmode=WIFI_AUTH_OPEN;
    (void)esp_wifi_disconnect();xEventGroupClearBits(s_wifi_bits,BIT0|BIT1);state_set(CONNECTING,0);
    esp_err_t err=esp_wifi_set_config(WIFI_IF_STA,&cfg);
    if(err==ESP_OK) err=esp_wifi_connect();
    memset(&cfg,0,sizeof(cfg));
    if(err==ESP_OK) {
        EventBits_t bits=xEventGroupWaitBits(s_wifi_bits,BIT0|BIT1,pdFALSE,pdFALSE,pdMS_TO_TICKS(20000));
#if CONFIG_ZION_LAUNCHER
        /* A foreground/default launch must not wait twenty seconds for a
         * missing router. Stored credentials remain intact when preempted. */
        if(job->source_offset==0 && (bits&BIT1) && !(bits&BIT0)) return ESP_ERR_INVALID_STATE;
#endif
        if(!(bits&BIT0)) err=ESP_ERR_TIMEOUT;
    }
#if CONFIG_ZION_592_INSTALL
    if(err==ESP_OK) {
        nvs_handle_t h;err=nvs_open("zion592",NVS_READWRITE,&h);
        if(err==ESP_OK) {
            err=nvs_set_str(h,"ssid",job->ssid);
            if(err==ESP_OK) err=nvs_set_str(h,"password",job->password);
            if(err==ESP_OK) err=nvs_commit(h);
            nvs_close(h);
        }
    }
#endif
    if(err!=ESP_OK) state_set(ERROR,1);
    return err;
}

#if CONFIG_ZION_592_INSTALL
static void install_progress(unsigned stage,unsigned percent,const char *detail,const zion_install_info_t *info,void *context) {
    (void)context;
    portENTER_CRITICAL(&s_lock);
    s_install_stage=stage;s_percent=percent;
    s_install_info=*info;
    s_install_update_us=esp_timer_get_time();
    snprintf(s_install_detail,sizeof(s_install_detail),"%s",detail);
    s_status.total=s_capture_size;
    s_status.received=(uint32_t)(((uint64_t)s_capture_size*percent)/100);
    portEXIT_CRITICAL(&s_lock);
    state_set(stage==255?ERROR:stage==100?READY:stage>=40?VERIFYING:DOWNLOADING,stage==255?1:0);
}
#endif
static void network_worker(void *arg) {
    (void)arg;
    int64_t reconnect_at=0;
    z592_request_t *job=calloc(1,sizeof(*job));
    if (!job) {state_set(ERROR,1);vTaskDelete(NULL);return;}
    for (;;) {
        if(atomic_load(&s_native_worker_stop)) {
            free(job);xEventGroupSetBits(s_wifi_bits,BIT2);vTaskDelete(NULL);return;
        }
#if !ZION_RUNTIME_SIM
        if(zion_runtime_active() && atomic_load(&s_native_keep_wifi)) {
            /* Radio ownership stays with ZION after entering a native app. */
            int64_t now=esp_timer_get_time();
            portENTER_CRITICAL(&s_lock);bool native_online=s_wifi_online;portEXIT_CRITICAL(&s_lock);
            if(!native_online && now>=reconnect_at) {
                reconnect_at=now+3000000;
                (void)esp_wifi_connect();
            }
        }
#else
        (void)reconnect_at;
#endif
#if CONFIG_ZION_LAUNCHER
        if(zion_wifi_ap_expired()) {
            zion_wifi_ap_stop();
            portENTER_CRITICAL(&s_lock);s_ap=false;s_generation++;portEXIT_CRITICAL(&s_lock);
            ESP_LOGI(TAG,"ZION_PORTAL AP closed after success handoff");
        }
#endif
        if(xQueueReceive(s_jobs,job,pdMS_TO_TICKS(250))!=pdTRUE) continue;
        if (job->type==9) scan_networks(job->source_offset==1);
        else if (job->type==1) {
            /* HTTP configuration is asynchronous. Let its acknowledgement leave
             * the STA socket before reconfiguring that same interface. */
            if(job->source_offset==1) vTaskDelay(pdMS_TO_TICKS(500));
            esp_err_t wifi_result=wifi_connect(job);
#if !CONFIG_ZION_LAUNCHER
            (void)wifi_result;
#endif
#if CONFIG_ZION_LAUNCHER
            if(job->source_offset==1 && s_ap) zion_wifi_connect_result(wifi_result);
            else if(s_wifi_online) {zion_wifi_ap_stop();s_ap=false;}
            portENTER_CRITICAL(&s_lock);if(job->source_offset==1) {s_busy=false;s_result=wifi_result;s_install_stage=0;snprintf(s_install_detail,sizeof(s_install_detail),"%s",esp_err_to_name(wifi_result));}s_generation++;portEXIT_CRITICAL(&s_lock);
#endif
        }
#if CONFIG_ZION_LAUNCHER
        else if(job->type>=21 && job->type<=26) {
            zion_action_t action=(zion_action_t)(job->type-20);int slot=(int)job->source_offset;
            esp_err_t err=ESP_OK;bool cleanup=false;
            if(action==ZION_BOOT || action==ZION_DEFAULT || action==ZION_REMOVE) {
                const meta_carve_t *c=meta_carve_flash_carve();
                bool builtin_default=(action==ZION_DEFAULT || action==ZION_REMOVE) && zion_runtime_name(slot)!=NULL;
                if(!builtin_default && (slot<0 || slot>=c->count || c->slot[slot].play_id!=job->file_size)) err=ESP_ERR_INVALID_STATE;
            }
            if(err==ESP_OK) switch(action) {
            case ZION_BOOT:err=boot_child(slot);break;
            case ZION_DEFAULT:
                if(zion_runtime_name(slot)) {
                    bool enabled=zion_runtime_default_slot()!=slot;
                    if(enabled) err=zion_default_update(&s_prefs,-1);
                    if(err==ESP_OK && !zion_runtime_set_default(enabled?slot:-1)) err=ESP_FAIL;
                } else {
                    err=zion_default_update(&s_prefs,zion_default_find(meta_carve_flash_carve(),&s_prefs)==slot?-1:slot);
                    if(err==ESP_OK && !zion_runtime_set_default(-1)) err=ESP_FAIL;
                }
                break;
            case ZION_REMOVE:
                if(slot<=ZION_PACKAGE_SLOT_BASE) {
                    if(zion_runtime_default_slot()==slot && !zion_runtime_set_default(-1)) err=ESP_FAIL;
                    if(err==ESP_OK && zion_package_remove(slot)) err=ESP_FAIL;
                } else err=zion_delete(&s_prefs,slot,&cleanup);
                break;
            case ZION_FORGET: {
                zion_wifi_ap_stop();s_ap=false;
                nvs_handle_t h;err=nvs_open("zion592",NVS_READWRITE,&h);
                if(err==ESP_OK) {err=nvs_erase_all(h);if(err==ESP_OK) err=nvs_commit(h);
            nvs_close(h);}
                if(err==ESP_OK) {(void)esp_wifi_disconnect();s_wifi_online=false;}
                if(err!=ESP_OK) break;
            }
            __attribute__((fallthrough));
            case ZION_WIFI_OPEN:
                err=zion_wifi_ap_start(s_ap_name,s_ap_password);s_ap=err==ESP_OK;break;
            case ZION_WIFI_CLOSE:zion_wifi_ap_stop();s_ap=false;break;
            default:err=ESP_ERR_INVALID_ARG;break;
            }
            portENTER_CRITICAL(&s_lock);s_result=err;s_install_stage=0;s_busy=false;if(action==ZION_BOOT) s_boot_requested=false;s_generation++;
            snprintf(s_install_detail,sizeof(s_install_detail),"%s",cleanup?"removed-cleanup-pending":esp_err_to_name(err));portEXIT_CRITICAL(&s_lock);
            ESP_LOGI(TAG,"ZION_ACTION action=%d slot=%d result=%s cleanup=%d",action,slot,esp_err_to_name(err),cleanup);
        }
#endif
#if CONFIG_ZION_592_INSTALL
        else if(job->type==2) {
#if CONFIG_ZION_LAUNCHER
            zion_wifi_ap_stop();s_ap=false;
#endif
            int installed=-1;
            radio_snapshot("install-begin");
            esp_err_t err=z592_install_request(job,install_progress,NULL,&installed);
            radio_snapshot("install-end");
            portENTER_CRITICAL(&s_lock);
            s_busy=false;
#if CONFIG_ZION_LAUNCHER
            s_result=err;s_generation++;
#endif
            if(err==ESP_OK) {s_completed++;s_installed_slot=installed;s_boot_at=install_autoboot_allowed()?esp_timer_get_time()+1500000:0;}
            portEXIT_CRITICAL(&s_lock);
        }
#endif
#if CONFIG_ZION_LAUNCHER
        meta_carve_flash_workspace_release();
#endif
        memset(job,0,sizeof(*job));
    }
}
static int control_access(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg) {
    portENTER_CRITICAL(&s_lock);s_control_writes++;portEXIT_CRITICAL(&s_lock);
    (void)conn;(void)handle;(void)arg;
    uint8_t p[Z592_TRANSFER_MAX+3];uint16_t n=OS_MBUF_PKTLEN(ctx->om);
    if (!n || n>sizeof(p) || ble_hs_mbuf_to_flat(ctx->om,p,sizeof(p),NULL)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    ESP_LOGI(TAG,"Z592 CONTROL cmd=0x%02x len=%u",p[0],n);
    if (p[0]==3) {
#if CONFIG_ZION_592_INSTALL
        if(s_busy) {z592_install_cancel();return 0;}
#endif
        z592_abort(&s_transfer);state_set(s_wifi_online?ONLINE:IDLE,0);return 0;
    }
    if (p[0]==5) {notify_status();return 0;}
    /* Reboot is deliberately acknowledged but suppressed: retain evidence. */
    if (p[0]==4) {
#if CONFIG_ZION_592_INSTALL
        if(s_installed_slot>=0 && !s_busy && install_autoboot_allowed()) s_boot_at=esp_timer_get_time()+200000;
#endif
        ESP_LOGI(TAG,"Z592 REBOOT_REQUEST queued_if_complete=1");return 0;
    }
    reject_record(13,p[0],n);state_set(ERROR,1);return 0;
}
static int secure_access(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg) {
    portENTER_CRITICAL(&s_lock);s_secure_writes++;if(s_busy) s_busy_rejects++;portEXIT_CRITICAL(&s_lock);
    (void)conn;(void)handle;(void)arg;
    uint8_t p[Z592_TRANSFER_MAX+3];uint16_t n=OS_MBUF_PKTLEN(ctx->om);
    if (!n || n>sizeof(p) || ble_hs_mbuf_to_flat(ctx->om,p,sizeof(p),NULL)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if(s_busy || zion_runtime_active()) return BLE_ATT_ERR_UNLIKELY;
    if (p[0]!=0x0b) ESP_LOGI(TAG,"Z592 SECURE cmd=0x%02x len=%u",p[0],n);
    if (p[0]==0x0a) {
        if(n==4) ESP_LOGI(TAG,"Z592 BEGIN type=%u expected=%u",p[1],(unsigned)p[2]+((unsigned)p[3]<<8));
        if(z592_begin(&s_transfer,p,n)) return 0;
        reject_record(10,p[0],n);return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (p[0]==0x0b) {
        if(z592_chunk(&s_transfer,p,n)) return 0;
        reject_record(11,p[0],n);return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (p[0]==9) {
        z592_request_t job={.type=9};
        return xQueueSend(s_jobs,&job,0)==pdTRUE?0:BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (p[0]==0x0c) {
        z592_request_t job;
        ESP_LOGI(TAG,"Z592 COMMIT type=%u received=%u expected=%u",s_transfer.type,s_transfer.received,s_transfer.expected);
        if (!z592_commit(&s_transfer,&job)) {reject_record(12,p[0],n);return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
        if (job.type==1) {
            ESP_LOGI(TAG,"Z592 WIFI_REQUEST credentials=redacted persistence=%s",CONFIG_ZION_592_INSTALL?"NVS-after-connect":"RAM");
            state_set(CONNECTING,0);
            bool queued=xQueueSend(s_jobs,&job,0)==pdTRUE;
            memset(&job,0,sizeof(job));return queued?0:BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        portENTER_CRITICAL(&s_lock);
        s_captures++;s_capture_size=job.file_size;s_capture_source=job.source_offset;
        s_status.received=0;s_status.total=job.file_size;
        portEXIT_CRITICAL(&s_lock);
        ESP_LOGI(TAG,"Z592 INSTALL_CAPTURED bytes=%lu source=0x%lx url_len=%u backend=%d",
            (unsigned long)job.file_size,(unsigned long)job.source_offset,(unsigned)strlen(job.url),CONFIG_ZION_592_INSTALL);
#if CONFIG_ZION_592_INSTALL
        z592_install_prepare();
        portENTER_CRITICAL(&s_lock);
        s_busy=true;s_install_stage=10;s_percent=0;s_boot_at=0;s_installed_slot=-1;
        memset(&s_install_info,0,sizeof(s_install_info));s_install_info.package_bytes=job.file_size;s_install_info.phase=10;
        s_install_id++;snprintf(s_install_detail,sizeof(s_install_detail),"preflight");
        portEXIT_CRITICAL(&s_lock);
        bool queued=xQueueSend(s_jobs,&job,0)==pdTRUE;
        memset(&job,0,sizeof(job));
        if(!queued) {
            portENTER_CRITICAL(&s_lock);s_busy=false;s_install_stage=255;
            snprintf(s_install_detail,sizeof(s_install_detail),"install-queue-full");portEXIT_CRITICAL(&s_lock);
            state_set(ERROR,1);return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        state_set(DOWNLOADING,0);
#else
        memset(&job,0,sizeof(job));state_set(ERROR,1);
#endif
        return 0;
    }
    reject_record(14,p[0],n);return BLE_ATT_ERR_UNLIKELY;
}
static int status_access(uint16_t c,uint16_t h,struct ble_gatt_access_ctxt *ctx,void *arg) {
    portENTER_CRITICAL(&s_lock);s_status_reads++;portEXIT_CRITICAL(&s_lock);
    (void)c;(void)h;(void)arg;status_t copy=status_get();
    return os_mbuf_append(ctx->om,&copy,sizeof(copy))?BLE_ATT_ERR_INSUFFICIENT_RES:0;
}
static int info_access(uint16_t c,uint16_t h,struct ble_gatt_access_ctxt *ctx,void *arg) {
    (void)c;(void)h;(void)arg;
    info_t info={.magic={'A','I','P','R'},.protocol=4,.flags=0x0f,.version=0x0400,
        .flash=0x800000,.netcfg=0x6bc000,.recovery=0x6c0000};
    memcpy(info.sn,s_name,sizeof(s_name));
    portENTER_CRITICAL(&s_lock);s_info_reads++;portEXIT_CRITICAL(&s_lock);
    /* A new mini-program session queries INFO before STATUS. Installation
     * failure is not Wi-Fi failure: keep the UI diagnosis, but rearm the wire
     * readiness from the actual link rather than demanding credentials again.
     * Do not alter a live transfer's terminal notification or an active job. */
    uint8_t old_state=status_get().state;
    uint8_t session_state=z592_session_state(old_state,s_busy,s_wifi_online);
    if(session_state!=old_state) {
        ESP_LOGI(TAG,"Z592 SESSION_REARM online=%u",s_wifi_online);
        state_set(session_state,0);
    }
    const esp_app_desc_t *desc=esp_app_get_description();
    ESP_LOGI(TAG,"Z592 INFO_READ wire_profile=592-v4 virtual_layout=1 local_layout=zion conn=%u stored=%u elf=%02x%02x%02x%02x",
        c,s_conn,desc->app_elf_sha256[0],desc->app_elf_sha256[1],desc->app_elf_sha256[2],desc->app_elf_sha256[3]);
    return os_mbuf_append(ctx->om,&info,sizeof(info))?BLE_ATT_ERR_INSUFFICIENT_RES:0;
}
static int scan_access(uint16_t c,uint16_t h,struct ble_gatt_access_ctxt *ctx,void *arg) {
    (void)c;(void)h;(void)ctx;(void)arg;return 0;
}
static const struct ble_gatt_svc_def services[]={
    {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=BLE_UUID16_DECLARE(0xfff0),
     .characteristics=(struct ble_gatt_chr_def[]){
        {.uuid=BLE_UUID16_DECLARE(0xfff1),.access_cb=control_access,.flags=BLE_GATT_CHR_F_WRITE},
        {.uuid=BLE_UUID16_DECLARE(0xfff3),.access_cb=status_access,.val_handle=&s_status_handle,.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_NOTIFY},
        {.uuid=BLE_UUID16_DECLARE(0xfff4),.access_cb=info_access,.flags=BLE_GATT_CHR_F_READ},
        {.uuid=BLE_UUID16_DECLARE(0xfff5),.access_cb=scan_access,.val_handle=&s_scan_handle,.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_NOTIFY},
        {.uuid=BLE_UUID16_DECLARE(0xfff6),.access_cb=secure_access,.flags=BLE_GATT_CHR_F_WRITE},{0}}}, {0}};
static int gap_event(struct ble_gap_event *event,void *arg);
static int advertise(void) {
    if(!atomic_load(&s_ble_running) || s_native_claimed) return 0;
    const ble_uuid16_t uuid=BLE_UUID16_INIT(0xfff0);
    struct ble_hs_adv_fields fields={.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP,
        .name=(const uint8_t *)s_name,.name_len=12,.name_is_complete=1,
        .uuids16=(ble_uuid16_t *)&uuid,.num_uuids16=1,.uuids16_is_complete=1};
    int rc=ble_gap_adv_set_fields(&fields);if(rc) return rc;
    struct ble_gap_adv_params params={.conn_mode=BLE_GAP_CONN_MODE_UND,.disc_mode=BLE_GAP_DISC_MODE_GEN};
    rc=ble_gap_adv_start(s_addr_type,NULL,BLE_HS_FOREVER,&params,gap_event,NULL);
    ESP_LOGI(TAG,"Z592 ADVERTISE rc=%d",rc);return rc;
}
static int gap_event(struct ble_gap_event *event,void *arg) {
    (void)arg;
    if(event->type==BLE_GAP_EVENT_CONNECT || event->type==BLE_GAP_EVENT_LINK_ESTAB)
        ESP_LOGI(TAG,"Z592 GAP type=%u status=%d conn=%u",event->type,event->connect.status,event->connect.conn_handle);
    if (event->type==BLE_GAP_EVENT_CONNECT || event->type==BLE_GAP_EVENT_LINK_ESTAB) {
        const uint16_t handle=event->connect.conn_handle;
        struct ble_gap_conn_desc desc;
        bool usable=z592_connection_usable(event->connect.status,ble_gap_conn_find(handle,&desc)==0);
        if (usable && s_conn!=handle) {
            s_conn=handle;
            portENTER_CRITICAL(&s_lock);s_connected=true;s_connections++;portEXIT_CRITICAL(&s_lock);
#if CONFIG_ZION_592_INSTALL
#if !CONFIG_ZION_LAUNCHER
            s_show_list=false;
#endif
#endif
            ESP_LOGI(TAG,"Z592 CONNECT live_link=1 event_status=%d",event->connect.status);notify_status();
        } else if (!usable && !ble_gap_adv_active()) (void)advertise();
    } else if(event->type==BLE_GAP_EVENT_CONN_UPDATE) {
        ESP_LOGI(TAG,"Z592_LINK_UPDATE status=%d",event->conn_update.status);radio_snapshot("gap-update");
    } else if (event->type==BLE_GAP_EVENT_DISCONNECT) {
        s_conn=BLE_HS_CONN_HANDLE_NONE;z592_abort(&s_transfer);
        portENTER_CRITICAL(&s_lock);s_connected=false;portEXIT_CRITICAL(&s_lock);
        ESP_LOGI(TAG,"Z592 DISCONNECT reason=%d",event->disconnect.reason);(void)advertise();
    } else if (event->type==BLE_GAP_EVENT_ADV_COMPLETE) (void)advertise();
    else if (event->type==BLE_GAP_EVENT_SUBSCRIBE)
        ESP_LOGI(TAG,"Z592 SUBSCRIBE status=%d scan=%d notify=%d",
            event->subscribe.attr_handle==s_status_handle,event->subscribe.attr_handle==s_scan_handle,event->subscribe.cur_notify);
    return 0;
}
static void on_sync(void) {
    if(!ble_hs_util_ensure_addr(0) && !ble_hs_id_infer_auto(0,&s_addr_type)) (void)advertise();
}
static void host_task(void *arg) {(void)arg;nimble_port_run();nimble_port_freertos_deinit();}
#if CONFIG_ZION_592_INSTALL
static void on_key(bsp_btn_t button,bsp_btn_ev_t event,void *context) {
    (void)context;static zion_runtime_key_gate_t gate;
    if(zion_runtime_active() && (unsigned)button==3) {
        zion_runtime_key((unsigned)button,(unsigned)event);return;
    }
    /* A held OK spanning software reset can produce a delayed CLICK in the
     * new launcher. Require a fresh PRESS after the frontend becomes ready. */
    if(!zion_runtime_key_accept(&gate,(unsigned)button,(unsigned)event,atomic_load(&s_keys_ready))) return;
    if(zion_runtime_active()) {zion_runtime_key((unsigned)button,(unsigned)event);return;}
    if(event==BSP_BTN_RELEASE) return;
    zion_button_msg_t key={button,event};
    if(s_keys) (void)xQueueSend(s_keys,&key,0);
}
static esp_err_t boot_child(int slot) {
    esp_err_t err=z592_boot_installed(slot);
    if(err!=ESP_OK) {snprintf(s_install_detail,sizeof(s_install_detail),"boot:%s",esp_err_to_name(err));state_set(ERROR,6);return err;}
    ESP_LOGI(TAG,"Z592 BOOT slot=%d",slot);
    (void)ble_gap_adv_stop();
    if(s_conn!=BLE_HS_CONN_HANDLE_NONE) (void)ble_gap_terminate(s_conn,BLE_ERR_REM_USER_CONN_TERM);
    vTaskDelay(pdMS_TO_TICKS(200));esp_restart();return ESP_OK;
}
#endif
#if !CONFIG_ZION_LAUNCHER
static void ui_update(void) {
    status_t state=status_get();
    unsigned conn,info,scan,capture;uint32_t size,offset;bool connected;
    portENTER_CRITICAL(&s_lock);
    conn=s_connections;info=s_info_reads;scan=s_scans;capture=s_captures;
    size=s_capture_size;offset=s_capture_source;connected=s_connected;
    portEXIT_CRITICAL(&s_lock);
    int soc=bsp_battery_soc();
    if (!bsp_lvgl_lock(200)) return;
    char detail[64];unsigned install_stage,percent;
    portENTER_CRITICAL(&s_lock);install_stage=s_install_stage;percent=s_percent;memcpy(detail,s_install_detail,sizeof(detail));portEXIT_CRITICAL(&s_lock);
    const char *text=capture?"已收到安装指令\n研究模式：未执行安装":
        state.state==ONLINE?"Wi-Fi 已连接\n请在社区选择玩法安装":
        state.state==CONNECTING?"正在连接 Wi-Fi":state.state==ERROR?"配网失败，请重试":
        connected?"蓝牙已连接\n等待小程序操作":"等待小程序连接\n请打开设备页面";
    lv_label_set_text(s_label,text);
#if CONFIG_ZION_592_INSTALL
    if(s_busy || state.state==READY || install_stage==255) {
        const char *phase=install_stage==10?"正在校验下载包":install_stage==20?"正在准备存储":
            install_stage==30?"正在写入玩法":install_stage==40?"正在校验应用":
            install_stage==50?"正在保存安装记录":install_stage==100?"安装成功，即将启动":"安装失败";
        lv_label_set_text_fmt(s_label,"%s  %u%%\n%s",phase,percent,detail);
    } else if(s_show_list) {
        const meta_carve_t *carve=meta_carve_flash_carve();
        char list[240]={0};size_t used=0;
        for(unsigned i=0;i<carve->count && i<4;i++) {
            used+=(size_t)snprintf(list+used,sizeof(list)-used,"%s %.32s\n",s_selected==(int)i?">":" ",carve->slot[i].name[0]?carve->slot[i].name:"未完成玩法");
            if(used>=sizeof(list)) break;
        }
        if(!carve->count) snprintf(list,sizeof(list),"尚未安装玩法\n请从小程序安装");
        lv_label_set_text(s_label,list);
    }
#else
    (void)install_stage;(void)percent;(void)detail;
#endif
    lv_label_set_text_fmt(s_stats,"连接 %u  信息 %u\n扫描 %u  指令 %u\n文件 %luK\n源偏移 0x%lx",
        conn,info,scan,capture,(unsigned long)(size/1024),(unsigned long)offset);
    if(soc>=0) lv_label_set_text_fmt(s_battery,"%d%%",soc);else lv_label_set_text(s_battery,"?");
    bsp_lvgl_unlock();
}
#endif
void app_main(void) {
#if CONFIG_ZION_592_INSTALL
    ESP_LOGI(TAG,"ZION592 INSTALL verified backend enabled");
    ESP_ERROR_CHECK(meta_carve_flash_ensure());
#if CONFIG_ZION_LAUNCHER
    /* Canonical state is persistent; scratch must not occupy the idle UI heap. */
    meta_carve_flash_workspace_release();
#endif
#else
    ESP_LOGI(TAG,"ZION592 PROBE capture-only play_writes=0 virtual_wire_layout=592v4");
#endif
    if(!layout_valid()) {ESP_LOGE(TAG,"Z592 LOCAL_LAYOUT_REJECTED");return;}
    ESP_ERROR_CHECK(nvs_flash_init()); /* Never erase NVS on initialization failure. */
    zion_package_recover();
    zion_runtime_init();
#if CONFIG_ZION_592_INSTALL
    const esp_partition_t *ota=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,NULL);
    if(ota) ESP_ERROR_CHECK(esp_partition_erase_range(ota,0,ota->size));
    s_keys=xQueueCreate(8,sizeof(zion_button_msg_t));
    if(!s_keys) return;
    ESP_ERROR_CHECK(bsp_button_init(on_key,NULL));
#if !CONFIG_ZION_LAUNCHER
    s_show_list=true;
#else
    esp_err_t pe=zion_prefs_load(&s_prefs);s_skip_autostart=pe!=ESP_OK || (s_prefs.flags&ZION_PREFS_SKIP_ONCE);
    if(s_prefs.flags&ZION_PREFS_SKIP_ONCE) {s_prefs.flags&=~ZION_PREFS_SKIP_ONCE;(void)zion_prefs_save(&s_prefs);}
#endif
#endif
    ESP_ERROR_CHECK(bsp_i2c_init());
#if ZION_RUNTIME_TEST_KEYS
    ESP_ERROR_CHECK(heap_caps_register_failed_alloc_callback(allocation_failed));
#endif
    ESP_ERROR_CHECK(bsp_display_init());
    if(!bsp_lvgl_init()) return;
    bsp_display_backlight(100);(void)bsp_battery_init();
#if !CONFIG_ZION_LAUNCHER
    if(bsp_lvgl_lock(1000)) {
        s_screen=ui_pixel_screen_create(CONFIG_ZION_592_INSTALL?"ZION / 小程序安装":"ZION / 小程序研究");
        s_label=ui_pixel_label(s_screen,"正在准备蓝牙",ZION_FONT,UI_PAPER);
        lv_obj_set_pos(s_label,10,54);lv_obj_set_width(s_label,220);
        s_stats=ui_pixel_label(s_screen,"",ZION_FONT,UI_INK2);lv_obj_set_pos(s_stats,10,128);
        s_battery=ui_pixel_label(s_screen,"?",ZION_FONT,UI_PAPER);lv_obj_set_pos(s_battery,194,9);
        lv_obj_t *note=ui_pixel_label(s_screen,CONFIG_ZION_592_INSTALL?"小程序配网并安装\n上下选择，短按OK启动":"仅接收指令，不写入玩法\n不会自动启动默认玩法",ZION_FONT,UI_INK2);
        lv_obj_set_pos(note,10,242);lv_screen_load(s_screen);bsp_lvgl_unlock();
    }
#endif
    s_wifi_bits=xEventGroupCreate();s_jobs=xQueueCreate(3,sizeof(z592_request_t));
    if(!s_wifi_bits || !s_jobs) return;
#if ZION_RUNTIME_SIM
    ESP_LOGW(TAG,"ZION_RUNTIME_SIM radio=disabled pcm=disabled");
#else
    ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    if(!esp_netif_create_default_wifi_sta()) return;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL));
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_start());
    uint8_t mac[6];ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_STA));
    for(unsigned i=0;i<6;i++) snprintf(s_name+2*i,3,"%02x",mac[i]);
    ESP_ERROR_CHECK(nimble_port_init());atomic_store(&s_ble_running,true);ble_svc_gap_init();ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(s_name));
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    ble_hs_cfg.sync_cb=on_sync;
    if(xTaskCreate(network_worker,"z592-net",CONFIG_ZION_592_INSTALL?8192:6144,NULL,5,NULL)!=pdPASS) return;
    if(xTaskCreate(console_worker,"z592-console",8192,NULL,3,NULL)!=pdPASS) return;
    nimble_port_freertos_init(host_task);
#if CONFIG_ZION_592_INSTALL
    nvs_handle_t saved;
    if(nvs_open("zion592",NVS_READONLY,&saved)==ESP_OK) {
        z592_request_t *job=calloc(1,sizeof(*job));
        if(job) {
            size_t ssid_len=sizeof(job->ssid),pass_len=sizeof(job->password);
            if(nvs_get_str(saved,"ssid",job->ssid,&ssid_len)==ESP_OK &&
               nvs_get_str(saved,"password",job->password,&pass_len)==ESP_OK) {
                job->type=1;(void)xQueueSend(s_jobs,job,0);
            }
            memset(job,0,sizeof(*job));free(job);
        }
        nvs_close(saved);
    }
#endif
#endif /* ZION_RUNTIME_SIM */
#if CONFIG_ZION_LAUNCHER
    ESP_LOGI(TAG,"ZION_PRODUCT ready version=%s",esp_app_get_description()->version);
    zion_launcher_init();
    atomic_store(&s_keys_ready,true);
#endif
#if ZION_RUNTIME_SIM
    if(xTaskCreate(console_worker,"z592-console",8192,NULL,3,NULL)!=pdPASS) return;
    if(xTaskCreate(network_worker,"z592-net",8192,NULL,5,NULL)!=pdPASS) return;
#endif
    unsigned ticks=0;
#if CONFIG_ZION_LAUNCHER
    unsigned swallowed=0;
#endif
    for(;;) {
#if CONFIG_ZION_592_INSTALL
        zion_button_msg_t key;
#if CONFIG_ZION_LAUNCHER
        if(xQueueReceive(s_keys,&key,pdMS_TO_TICKS(50))==pdTRUE) {
            bool cancelled=!zion_runtime_active() && key.event==BSP_BTN_PRESS && zion_launcher_cancel_auto();
            zion_key_phase_t phase=key.event==BSP_BTN_PRESS?ZION_PRESS_EVENT:key.event==BSP_BTN_CLICK?ZION_CLICK_EVENT:ZION_HOLD_EVENT;
            bool dispatch=(key.event==BSP_BTN_PRESS || key.event==BSP_BTN_CLICK || key.event==BSP_BTN_LONG) &&
                zion_key_dispatch(&swallowed,(unsigned)key.button,phase,cancelled);
            if(zion_runtime_active()) {zion_runtime_key((unsigned)key.button,(unsigned)key.event);dispatch=false;}
            if(dispatch && (key.event!=BSP_BTN_LONG || key.button==BSP_BTN_OK))
                zion_launcher_key(key.event==BSP_BTN_LONG?ZION_LONG:key.button==BSP_BTN_UP?ZION_UP:key.button==BSP_BTN_DOWN?ZION_DOWN:ZION_OK);
        }
        if(!zion_runtime_active()) zion_launcher_tick();
        zion_runtime_tick();
        /* Native input goes directly to its queue. This manager's large startup
         * stack has no further owner; the idle hook wakes the plugin after reaping. */
        if(zion_runtime_manager_done()) vTaskDelete(NULL);
        bool snapshot;portENTER_CRITICAL(&s_lock);snapshot=s_snapshot_requested;s_snapshot_requested=false;portEXIT_CRITICAL(&s_lock);
        if(snapshot && !zion_runtime_active()) zion_launcher_snapshot();
#if CONFIG_META_E2E_TEST_CONTROL
        bool qr_pixels;portENTER_CRITICAL(&s_lock);qr_pixels=s_qr_pixels_requested;s_qr_pixels_requested=false;portEXIT_CRITICAL(&s_lock);
        if(qr_pixels && !zion_runtime_active()) zion_launcher_qr_pixels();
#endif
#else
        if(xQueueReceive(s_keys,&key,pdMS_TO_TICKS(50))==pdTRUE && !s_busy) {
            const meta_carve_t *carve=meta_carve_flash_carve();
            if(key.event==BSP_BTN_LONG && key.button==BSP_BTN_OK) s_show_list=!s_show_list;
            else if(key.event==BSP_BTN_CLICK && carve->count) {
                if(key.button==BSP_BTN_UP || key.button==BSP_BTN_DOWN) {
                    s_selected=(s_selected+carve->count+(key.button==BSP_BTN_UP?-1:1))%carve->count;s_show_list=true;
                } else if(key.button==BSP_BTN_OK && s_show_list) boot_child(s_selected);
            }
        }
#endif
        int64_t boot_at;int installed;
        portENTER_CRITICAL(&s_lock);boot_at=s_boot_at;installed=s_installed_slot;portEXIT_CRITICAL(&s_lock);
        if(boot_at && esp_timer_get_time()>=boot_at) {s_boot_at=0;boot_child(installed);}
#if !CONFIG_ZION_LAUNCHER
        if((ticks%10)==0) ui_update();
#endif
        if((ticks++%100)==0) snapshot_print();
#else
        ui_update();if((ticks++%10)==0) snapshot_print();vTaskDelay(pdMS_TO_TICKS(500));
#endif
    }
}

#if CONFIG_ZION_LAUNCHER
bool zion_service_catalog(meta_carve_t *c) {
    portENTER_CRITICAL(&s_lock);
    bool ok=!s_busy;if(ok) *c=*meta_carve_flash_carve();
    portEXIT_CRITICAL(&s_lock);return ok;
}
void zion_service_get(zion_service_state_t *v) {
#if CONFIG_ZION_592_INSTALL
    int64_t now=esp_timer_get_time();
#endif
    portENTER_CRITICAL(&s_lock);
    memset(v,0,sizeof(*v));v->busy=s_busy;v->online=s_wifi_online;v->connected=s_connected;
    v->state=s_status.state;v->error=s_status.error;v->stage=s_install_stage;v->percent=s_percent;
    v->generation=s_generation;v->result=s_result;v->prefs=s_prefs;v->skip_autostart=s_skip_autostart;
#if CONFIG_ZION_592_INSTALL
    v->install=s_install_info;v->install_id=s_install_id;v->installed_slot=s_installed_slot;
    if(v->busy && (v->stage==30 || v->stage==10) && v->install.elapsed_ms && s_install_update_us && now>s_install_update_us) {
        v->install.elapsed_ms+=(uint32_t)((now-s_install_update_us)/1000);
        uint32_t received=v->stage==30?v->install.received_bytes:v->install.layout_bytes;
        v->install.bytes_per_second=v->install.elapsed_ms?(uint32_t)(((uint64_t)received*1000)/v->install.elapsed_ms):0;
    }
#endif
    v->ap=s_ap;memcpy(v->ap_name,s_ap_name,sizeof(v->ap_name));memcpy(v->ap_password,s_ap_password,sizeof(v->ap_password));
    memcpy(v->detail,s_install_detail,sizeof(v->detail));portEXIT_CRITICAL(&s_lock);
}
bool zion_service_request(zion_action_t action,int slot) {
    if(zion_runtime_active()) return false;
    if(action==ZION_ABORT) {z592_install_cancel();return true;}
    z592_request_t job={.type=(uint8_t)(20+action),.source_offset=(uint32_t)slot};
    portENTER_CRITICAL(&s_lock);
    if(s_busy || s_boot_at || zion_runtime_active()) {portEXIT_CRITICAL(&s_lock);return false;}
    const meta_carve_t *c=meta_carve_flash_carve();
    if(slot>=0 && slot<c->count) job.file_size=c->slot[slot].play_id;
    s_busy=true;s_install_stage=200;if(action==ZION_BOOT) s_boot_requested=true;s_generation++;portEXIT_CRITICAL(&s_lock);
    if(action==ZION_BOOT) xEventGroupSetBits(s_wifi_bits,BIT1);
    if(xQueueSend(s_jobs,&job,0)==pdTRUE) return true;
    portENTER_CRITICAL(&s_lock);s_busy=false;if(action==ZION_BOOT) s_boot_requested=false;s_generation++;portEXIT_CRITICAL(&s_lock);return false;
}
bool zion_service_wifi(const char *ssid,const char *password) {
    if(zion_runtime_active()) return false;
    z592_request_t job={.type=1,.source_offset=1};
    if(strlen(ssid)>32 || strlen(password)>64) return false;
    memcpy(job.ssid,ssid,strlen(ssid)+1);memcpy(job.password,password,strlen(password)+1);
    portENTER_CRITICAL(&s_lock);
    if(s_busy) {portEXIT_CRITICAL(&s_lock);memset(&job,0,sizeof(job));return false;}
    s_busy=true;s_install_stage=200;s_generation++;portEXIT_CRITICAL(&s_lock);
    bool queued=xQueueSend(s_jobs,&job,0)==pdTRUE;memset(&job,0,sizeof(job));
    if(!queued) {portENTER_CRITICAL(&s_lock);s_busy=false;s_generation++;portEXIT_CRITICAL(&s_lock);}
    return queued;
}
bool zion_service_scan(void) {
    if(zion_runtime_active()) return false;
    z592_request_t job={.type=9,.source_offset=1};
    portENTER_CRITICAL(&s_lock);bool busy=s_busy || s_boot_at;portEXIT_CRITICAL(&s_lock);
    return !busy && xQueueSend(s_jobs,&job,0)==pdTRUE;
}
bool zion_service_native_claim(void) {
    portENTER_CRITICAL(&s_lock);
    bool ok=!s_busy && !s_boot_at && !s_ap;
    if(ok) {s_native_claimed=true;s_busy=true;s_install_stage=200;s_generation++;}
    portEXIT_CRITICAL(&s_lock);return ok;
}
void zion_service_native_release(void) {
    portENTER_CRITICAL(&s_lock);
    if(s_native_claimed) {s_native_claimed=false;s_busy=false;s_generation++;}
    portEXIT_CRITICAL(&s_lock);
}
void zion_service_native_handoff(bool keep_network,bool keep_bluetooth,bool owns_usb) {
    atomic_store(&s_native_owns_usb,owns_usb);
    /* The full USB installer is not part of the application's public services. */
    atomic_store(&s_native_console_stop,true);
    EventBits_t console=xEventGroupWaitBits(s_wifi_bits,BIT3,pdFALSE,pdFALSE,pdMS_TO_TICKS(1000));
    if(!(console&BIT3)) ESP_LOGE(TAG,"NATIVE_CONSOLE stop-timeout");
    atomic_store(&s_native_keep_wifi,keep_network);
    unsigned before=esp_get_free_heap_size();
#if !ZION_RUNTIME_SIM
    if(atomic_exchange(&s_ble_running,false)) {
        (void)ble_gap_adv_stop();
        if(s_conn!=BLE_HS_CONN_HANDLE_NONE) (void)ble_gap_terminate(s_conn,BLE_ERR_REM_USER_CONN_TERM);
        int stopped=nimble_port_stop();esp_err_t deinit=stopped==0?nimble_port_deinit():ESP_FAIL;
        /* esp_bt_mem_release is irreversible until reset. A BLE application must
         * be able to initialize its own host and services after this handoff. */
        esp_err_t released=deinit==ESP_OK && !keep_bluetooth?esp_bt_mem_release(ESP_BT_MODE_BLE):ESP_OK;
        ESP_LOGI(TAG,"NATIVE_BLE stop=%d deinit=%d release=%d retained=%d",stopped,(int)deinit,(int)released,keep_bluetooth);
    }
    if(!keep_network) {
        atomic_store(&s_native_worker_stop,true);
        portENTER_CRITICAL(&s_lock);s_boot_requested=true;portEXIT_CRITICAL(&s_lock);
        xEventGroupSetBits(s_wifi_bits,BIT1);
        z592_request_t wake={0};(void)xQueueSend(s_jobs,&wake,0);
        EventBits_t stopped=xEventGroupWaitBits(s_wifi_bits,BIT2,pdFALSE,pdFALSE,pdMS_TO_TICKS(1000));
        if(stopped&BIT2) {vQueueDelete(s_jobs);s_jobs=NULL;}
        else ESP_LOGE(TAG,"NATIVE_NETWORK worker-stop-timeout");
        (void)esp_event_handler_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event);
        (void)esp_event_handler_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event);
        (void)esp_wifi_stop();(void)esp_wifi_deinit();
        esp_netif_t *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if(sta) esp_netif_destroy_default_wifi(sta);
        portENTER_CRITICAL(&s_lock);s_wifi_online=false;portEXIT_CRITICAL(&s_lock);
        zion_runtime_network(false);
    }
#endif
    /* Self-deleted FreeRTOS workers release their stacks from the idle task. */
    vTaskDelay(pdMS_TO_TICKS(10));
    multi_heap_info_t memory;heap_caps_get_info(&memory,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    ESP_LOGI(TAG,"NATIVE_MEMORY free=%u allocated=%u largest=%u",(unsigned)memory.total_free_bytes,
        (unsigned)memory.total_allocated_bytes,(unsigned)memory.largest_free_block);
    ESP_LOGI(TAG,"NATIVE_HANDOFF network=%d heap_before=%u heap_after=%lu",keep_network,before,(unsigned long)esp_get_free_heap_size());
}
static uint32_t s_package_footprint;
void zion_service_package_installed(int slot) {
    portENTER_CRITICAL(&s_lock);s_installed_slot=slot;portEXIT_CRITICAL(&s_lock);
}
void zion_service_package_footprint(uint32_t bytes) {
    portENTER_CRITICAL(&s_lock);s_package_footprint=bytes;portEXIT_CRITICAL(&s_lock);
}
void zion_service_package_progress(const char *name,uint32_t received,uint32_t total,int status,const char *detail) {
    portENTER_CRITICAL(&s_lock);
    if(received==0 && status==0) {s_install_id++;s_installed_slot=-1;memset(&s_install_info,0,sizeof(s_install_info));s_package_footprint=0;}
    s_install_stage=status<0?255:status>0?100:30;
    s_percent=status>0?100:total?(unsigned)((uint64_t)received*100/total):0;
    s_install_info.package_bytes=total;s_install_info.received_bytes=received;
    s_install_info.install_bytes=s_package_footprint?s_package_footprint:(total+4095u)&~4095u;s_install_info.install_known=true;
    if(name) snprintf(s_install_info.name,sizeof(s_install_info.name),"%s",name);
    snprintf(s_install_detail,sizeof(s_install_detail),"%s",detail?detail:"");
    s_result=status<0?ESP_FAIL:ESP_OK;s_generation++;
    portEXIT_CRITICAL(&s_lock);
}
#endif
