"""Native modules cannot use standalone OTA APIs to overwrite the shared host."""
import argparse,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-ota-guard-') as tmp:
    d=pathlib.Path(tmp)
    (d/'esp_ota_ops.h').write_text('''#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
typedef uint32_t esp_ota_handle_t;
typedef struct {int placeholder;} esp_partition_t;
#define ESP_ERR_NOT_SUPPORTED 0x106
''')
    (d/'test.c').write_text('''#include <assert.h>
#include "zion_idf_ota.h"
int main(void) {
 esp_partition_t partition={0};esp_ota_handle_t handle=123;
 assert(!zion_idf_ota_next(NULL)&&!zion_idf_ota_next(&partition));
 assert(zion_idf_ota_begin(&partition,4096,&handle)==ESP_ERR_NOT_SUPPORTED&&handle==0);
 assert(zion_idf_ota_begin(NULL,0,NULL)==ESP_ERR_NOT_SUPPORTED);
 assert(zion_idf_ota_write(123,"data",4)==ESP_ERR_NOT_SUPPORTED);
 assert(zion_idf_ota_abort(123)==ESP_ERR_NOT_SUPPORTED);
 assert(zion_idf_ota_end(123)==ESP_ERR_NOT_SUPPORTED);
 assert(zion_idf_ota_boot(&partition)==ESP_ERR_NOT_SUPPORTED);
 assert(zion_idf_ota_valid()==ESP_ERR_NOT_SUPPORTED);return 0;
}
''')
    exe=d/'ota.exe'
    subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(d),'-I'+str(root/'main'),str(root/'main/zion_idf_ota.c'),str(d/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: plugin OTA has no slot and rejects writes/boot changes without SDK flash providers')
