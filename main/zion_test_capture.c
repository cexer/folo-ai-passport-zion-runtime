/* DeviceTest only: stream actual LCD flush pixels without a full-frame buffer. */
#ifdef ZION_STANDALONE_TEST
#include "esp_lvgl_port.h"
#define bsp_lvgl_lock lvgl_port_lock
#define bsp_lvgl_unlock lvgl_port_unlock
#else
#include "bsp_display.h"
#endif
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include <stdatomic.h>
#include <stdio.h>
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
static atomic_bool armed;
static atomic_uint rectangles;
/* Connect Four uses hardware landscape rotation. Capture its actual logical
 * flush geometry too, without allocating a 150 KiB screenshot surface. */
enum {CHUNK_BYTES=256};
static unsigned capture_width,capture_height;
static char row[48+CHUNK_BYTES*2];
bool zion_test_capture_begin(void) {
    if(!bsp_lvgl_lock(1000)) return false;
    lv_display_t *display=lv_display_get_default();
    capture_width=(unsigned)lv_display_get_horizontal_resolution(display);
    capture_height=(unsigned)lv_display_get_vertical_resolution(display);
    if(!((capture_width==240 && capture_height==320) || (capture_width==320 && capture_height==240))) {
        bsp_lvgl_unlock();return false;
    }
    atomic_store(&rectangles,0);atomic_store(&armed,true);lv_obj_invalidate(lv_screen_active());bsp_lvgl_unlock();return true;
}
esp_err_t __real_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int,int,int,int,const void *);
esp_err_t __wrap_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel,int x1,int y1,int x2,int y2,const void *pixels) {
    esp_err_t result=__real_esp_lcd_panel_draw_bitmap(panel,x1,y1,x2,y2,pixels);
    if(atomic_load(&armed) && atomic_fetch_add(&rectangles,1)<4) printf("ZION_LCD_RECT x1=%d y1=%d x2=%d y2=%d result=%d\n",x1,y1,x2,y2,result);
    if(result!=ESP_OK || !atomic_load(&armed) || x1!=0 || x2!=(int)capture_width || y1<0 || y2>(int)capture_height) return result;
    const unsigned char *data=pixels;const char hex[]="0123456789abcdef";
    for(int y=y1;y<y2;y++) {
        unsigned row_bytes=capture_width*2;
        for(unsigned at=0;at<row_bytes;at+=CHUNK_BYTES) {
            unsigned n=row_bytes-at;if(n>CHUNK_BYTES)n=CHUNK_BYTES;
            int prefix=snprintf(row,sizeof(row),"ZION_LCD_CHUNK y=%d offset=%u data=",y,at);
            for(unsigned i=0;i<n;i++) {unsigned byte=data[(y-y1)*row_bytes+at+i];row[prefix+2*i]=hex[byte>>4];row[prefix+2*i+1]=hex[byte&15];}
            unsigned length=(unsigned)prefix+n*2;row[length++]='\n';row[length]=0;
            /* Connect Four's 1280 hex characters exceed the 1024-byte USB
             * ring. SDK write_bytes queues a complete item or returns zero;
             * it does not stream a too-large item. Keep each chunk below the
             * existing ring size rather than increasing resident test RAM. */
            if(usb_serial_jtag_is_driver_installed()) {
                if(usb_serial_jtag_write_bytes(row,length,pdMS_TO_TICKS(2000))!=(int)length) {
                    atomic_store(&armed,false);return result;
                }
            } else fputs(row,stdout);
        }
    }
    if(y2==(int)capture_height) {atomic_store(&armed,false);printf("ZION_LCD_END width=%u height=%u format=RGB565_BE\n",capture_width,capture_height);}
    fflush(stdout);return result;
}
