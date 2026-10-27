/* Shared hardware ownership below unchanged applications' ordinary SDK calls. */
#include "zion_idf_compat.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_button.h"
#include "bsp_pins.h"
#include "esp_lvgl_port.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "button_adc.h"
#include "iot_button.h"
#include "nvs_flash.h"
#include "driver/usb_serial_jtag.h"
#include <string.h>

static esp_err_t reuse_system(void) {return ESP_OK;}
static esp_err_t deny_nvs_erase(void) {return ESP_ERR_INVALID_STATE;}
static esp_err_t reuse_spi(spi_host_device_t host,const spi_bus_config_t *cfg,spi_dma_chan_t dma) {
    (void)dma;
    if(host!=SPI2_HOST || !cfg || !bsp_display_panel()) return ESP_ERR_NOT_SUPPORTED;
    return ESP_OK;
}
static esp_err_t reuse_io(esp_lcd_spi_bus_handle_t bus,const esp_lcd_panel_io_spi_config_t *cfg,esp_lcd_panel_io_handle_t *out) {
    if(bus!=(esp_lcd_spi_bus_handle_t)SPI2_HOST || !cfg || !out || !bsp_display_io()) return ESP_ERR_NOT_SUPPORTED;
    *out=bsp_display_io();return ESP_OK;
}
static esp_err_t reuse_panel(esp_lcd_panel_io_handle_t io,const esp_lcd_panel_dev_config_t *cfg,esp_lcd_panel_handle_t *out) {
    if(io!=bsp_display_io() || !cfg || !out || cfg->bits_per_pixel!=16) return ESP_ERR_NOT_SUPPORTED;
    *out=bsp_display_panel();return *out?ESP_OK:ESP_ERR_INVALID_STATE;
}
static esp_err_t reuse_panel_init(esp_lcd_panel_handle_t panel) {return panel==bsp_display_panel()?ESP_OK:ESP_ERR_NOT_SUPPORTED;}
static esp_err_t reuse_lvgl(const lvgl_port_cfg_t *cfg) {return cfg && lv_display_get_default()?ESP_OK:ESP_ERR_INVALID_STATE;}
static lv_display_t *reuse_display(const lvgl_port_display_cfg_t *cfg) {
    return cfg && cfg->panel_handle==bsp_display_panel()?lv_display_get_default():NULL;
}
static esp_err_t reuse_i2c(const i2c_master_bus_config_t *cfg,i2c_master_bus_handle_t *out) {
    if(!cfg || !out || cfg->i2c_port!=I2C_NUM_0) return ESP_ERR_NOT_SUPPORTED;
    *out=bsp_i2c_bus();return *out?ESP_OK:ESP_ERR_INVALID_STATE;
}
typedef struct {button_handle_t handle;button_event_t event;button_cb_t callback;void *user;} registration_t;
static registration_t registrations[16];
static unsigned registration_count;
static button_handle_t injected;
static unsigned injected_ms;
static esp_err_t reuse_button(const button_config_t *cfg,const button_adc_config_t *adc,button_handle_t *out) {
    if(!cfg || !adc || !out || adc->unit_id!=BSP_BTN_ADC_UNIT || adc->adc_channel!=BSP_BTN_ADC_CHANNEL) return ESP_ERR_NOT_SUPPORTED;
    *out=bsp_button_handle(adc->button_index);if(!*out) return ESP_ERR_INVALID_ARG;
    if(cfg->long_press_time) (void)iot_button_set_param(*out,BUTTON_LONG_PRESS_TIME_MS,(void *)(uintptr_t)cfg->long_press_time);
    return ESP_OK;
}
static esp_err_t register_button(button_handle_t handle,button_event_t event,button_event_args_t *args,button_cb_t cb,void *user) {
    if(registration_count>=16) return ESP_ERR_NO_MEM;
    esp_err_t e=iot_button_register_cb(handle,event,args,cb,user);
    if(e==ESP_OK) registrations[registration_count++]=(registration_t){handle,event,cb,user};
    return e;
}
static uint32_t pressed_time(button_handle_t handle) {return handle==injected?injected_ms:iot_button_get_pressed_time(handle);}
void zion_idf_inject_component_key(unsigned key,unsigned phase) {
    button_handle_t handle=bsp_button_handle(key);
    if(!handle) return;
    button_event_t events[3];unsigned count=0;
    if(phase==BSP_BTN_CLICK) {events[count++]=BUTTON_PRESS_DOWN;events[count++]=BUTTON_PRESS_UP;events[count++]=BUTTON_SINGLE_CLICK;}
    else if(phase==BSP_BTN_LONG) {events[count++]=BUTTON_LONG_PRESS_START;events[count++]=BUTTON_PRESS_UP;}
    injected=handle;injected_ms=phase==BSP_BTN_LONG?5000:100;
    for(unsigned e=0;e<count;e++) for(unsigned i=0;i<registration_count;i++) {
        registration_t *r=&registrations[i];if(r->handle==handle && r->event==events[e]) r->callback(handle,r->user);
    }
    injected=NULL;
}
static esp_err_t reuse_usb(usb_serial_jtag_driver_config_t *cfg) {
    if(!cfg) return ESP_ERR_INVALID_ARG;
    return usb_serial_jtag_is_driver_installed()?ESP_OK:usb_serial_jtag_driver_install(cfg);
}
uintptr_t zion_idf_board_resolve(const char *name) {
#define MAP(symbol,target) if(!strcmp(name,#symbol)) return (uintptr_t)&target;
    MAP(usb_serial_jtag_driver_install,reuse_usb)
    MAP(esp_netif_init,reuse_system)
    MAP(esp_event_loop_create_default,reuse_system)
    MAP(nvs_flash_erase,deny_nvs_erase)
    MAP(spi_bus_initialize,reuse_spi)
    MAP(esp_lcd_new_panel_io_spi,reuse_io)
    MAP(esp_lcd_new_panel_st7789,reuse_panel)
    MAP(esp_lcd_panel_reset,reuse_panel_init)
    MAP(esp_lcd_panel_init,reuse_panel_init)
    MAP(lvgl_port_init,reuse_lvgl)
    MAP(lvgl_port_add_disp,reuse_display)
    MAP(i2c_new_master_bus,reuse_i2c)
    MAP(iot_button_new_adc_device,reuse_button)
    MAP(iot_button_register_cb,register_button)
    MAP(iot_button_get_pressed_time,pressed_time)
#undef MAP
    return 0;
}
