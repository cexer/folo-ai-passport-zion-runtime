/* Test-only line demultiplexer. The unchanged application owns the USB reader;
 * diagnostics share that reader rather than racing another task for bytes. */
#include "driver/usb_serial_jtag.h"
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
extern void zion_usb_test_control(const char *line);
enum {CONTROL_LINE_BYTES=512};
static QueueHandle_t control_queue;
static void control_worker(void *unused) {
    (void)unused;char command[CONTROL_LINE_BYTES];
    for(;;) if(xQueueReceive(control_queue,command,portMAX_DELAY)==pdTRUE)
        zion_usb_test_control(command);
}
bool zion_test_usb_control_init(void) {
    if(control_queue) return true;
    control_queue=xQueueCreate(2,CONTROL_LINE_BYTES);
    if(!control_queue) return false;
    /* Passport OS's serial reader has only 2 KiB and deliberately delegates
     * heavy work to another task. Our diagnostic parser/capture must do the
     * same. The observed panic was in interrupt context while ser_cmd ran;
     * this avoids assuming that task-name alone proves its stack overflow. */
    if(xTaskCreate(control_worker,"zion-test-cmd",6144,NULL,3,NULL)!=pdPASS) {
        vQueueDelete(control_queue);control_queue=NULL;return false;
    }
    return true;
}
#ifdef ZION_STANDALONE_TEST
extern int __real_usb_serial_jtag_read_bytes(void *,uint32_t,uint32_t);
#define RAW_READ __real_usb_serial_jtag_read_bytes
#define READ_NAME __wrap_usb_serial_jtag_read_bytes
#else
#define RAW_READ usb_serial_jtag_read_bytes
#define READ_NAME zion_test_usb_read
#endif
int READ_NAME(void *out,uint32_t length,uint32_t wait) {
    static char line[CONTROL_LINE_BYTES];
    static unsigned used,ready,offset;
    static bool passthrough;
    if(!length) return 0;
    for(;;) {
        if(ready) {
            unsigned n=ready-offset;if(n>length) n=length;
            memcpy(out,line+offset,n);offset+=n;
            if(offset==ready) {ready=offset=used=0;}
            return (int)n;
        }
        char ch;int n=RAW_READ(&ch,1,wait);if(n<=0) return n;
        if(passthrough) {*(char *)out=ch;if(ch=='\n') passthrough=false;return 1;}
        line[used++]=ch;
        if(ch=='\n') {
            if(used>=5 && !memcmp(line,"ZION ",5)) {
                line[used-1]=0;if(used>1 && line[used-2]=='\r') line[used-2]=0;
                if(!control_queue || xQueueSend(control_queue,line,pdMS_TO_TICKS(2000))!=pdTRUE) {
                    static const char failure[]="ZION_CONTROL_UNAVAILABLE\n";
                    usb_serial_jtag_write_bytes(failure,sizeof(failure)-1,pdMS_TO_TICKS(100));
                }
                used=0;
            } else ready=used;
        } else if(used==sizeof(line)) {ready=used;passthrough=true;}
    }
}
