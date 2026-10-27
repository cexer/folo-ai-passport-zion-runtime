"""Compile the real USB demultiplexer: parser work runs off the app's reader."""
import argparse,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-control-worker-') as tmp:
 d=Path(tmp);(d/'freertos').mkdir();(d/'driver').mkdir()
 (d/'freertos/FreeRTOS.h').write_text('''#pragma once
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY (~0u)
''')
 (d/'freertos/queue.h').write_text('''#pragma once
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned,unsigned);
void vQueueDelete(QueueHandle_t);
int xQueueSend(QueueHandle_t,const void *,unsigned);
int xQueueReceive(QueueHandle_t,void *,unsigned);
''')
 (d/'freertos/task.h').write_text('int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,void *);\n')
 (d/'driver/usb_serial_jtag.h').write_text('''#include <stdint.h>
#include <stddef.h>
int usb_serial_jtag_read_bytes(void *,uint32_t,uint32_t);
int usb_serial_jtag_write_bytes(const void *,size_t,uint32_t);
''')
 (d/'test.c').write_text('''#include <assert.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static char messages[2][512],seen[8][512];
static unsigned count,head,callbacks,created,deleted,fail_queue,fail_task,replies;
static const char *input;static size_t at;
static void (*worker)(void *);static bool worker_context;static jmp_buf done;
void *xQueueCreate(unsigned n,unsigned size){assert(n==2&&size==512);return fail_queue?NULL:messages;}
void vQueueDelete(void *q){assert(q==messages);deleted++;}
int xQueueSend(void *q,const void *s,unsigned wait){assert(q==messages&&wait==2000);if(count==2)return 0;memcpy(messages[(head+count)%2],s,512);count++;return 1;}
int xQueueReceive(void *q,void *s,unsigned wait){assert(q==messages&&wait==~0u&&worker_context);if(!count)longjmp(done,1);memcpy(s,messages[head],512);head=(head+1)%2;count--;return 1;}
int xTaskCreate(void (*f)(void *),const char *name,unsigned bytes,void *arg,unsigned priority,void *handle){assert(!strcmp(name,"zion-test-cmd")&&bytes==6144&&!arg&&priority==3&&!handle);created++;worker=f;return fail_task?0:1;}
int usb_serial_jtag_read_bytes(void *p,uint32_t bytes,uint32_t wait){(void)wait;assert(bytes==1);if(!input[at])return 0;*(char *)p=input[at++];return 1;}
int usb_serial_jtag_write_bytes(const void *p,size_t bytes,uint32_t wait){assert(wait==100&&bytes==25&&!memcmp(p,"ZION_CONTROL_UNAVAILABLE\\n",25));replies++;return (int)bytes;}
void zion_usb_test_control(const char *line){assert(worker_context&&callbacks<8);strcpy(seen[callbacks++],line);}
extern bool zion_test_usb_control_init(void);
extern int zion_test_usb_read(void *,uint32_t,uint32_t);
static void run_worker(void){worker_context=true;if(!setjmp(done))worker(NULL);worker_context=false;}
int main(void){
 fail_queue=1;assert(!zion_test_usb_control_init());fail_queue=0;fail_task=1;assert(!zion_test_usb_control_init()&&deleted==1);
 fail_task=0;assert(zion_test_usb_control_init());assert(zion_test_usb_control_init()&&created==2);
 input="ZION MEM\\nZION ADC 2 140\\nplain\\n";char out[16]={0};
 assert(zion_test_usb_read(out,3,100)==3&&!memcmp(out,"pla",3));assert(callbacks==0&&count==2);
 run_worker();assert(callbacks==2&&!strcmp(seen[0],"ZION MEM")&&!strcmp(seen[1],"ZION ADC 2 140"));
 assert(zion_test_usb_read(out,16,100)==3&&!memcmp(out,"in\\n",3));
 input="ZION FIRST\\nZION SECOND\\nZION THIRD\\n";at=0;assert(zion_test_usb_read(out,16,100)==0&&replies==1&&callbacks==2);
 run_worker();assert(callbacks==4&&!strcmp(seen[2],"ZION FIRST")&&!strcmp(seen[3],"ZION SECOND"));
 puts("USB worker: PASS (no inline parser on app stack, FIFO, exact passthrough, bounded busy reply, init rollback)");
}
''')
 exe=d/'test.exe'
 subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I'+str(d),str(d/'test.c'),str(root/'tests/firmware/usb_line_control.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
