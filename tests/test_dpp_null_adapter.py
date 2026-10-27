"""Compile the real build-only SDK5 NULL-callback adapter against a SDK6 stub."""
import argparse,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-dpp-') as directory:
    d=Path(directory)
    (d/'esp_dpp.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_ERR_NOT_SUPPORTED 0x106\nesp_err_t esp_supp_dpp_init(void);\n')
    (d/'test.c').write_text('''#include <assert.h>
#include <stddef.h>
static int calls,result;
int esp_supp_dpp_init(void) {calls++;return result;}
#include "idf5-dpp.h"
int main(void) {
 result=-1;assert(esp_supp_dpp_init(NULL)==-1 && calls==1);
 result=0;assert(esp_supp_dpp_init(NULL)==0 && calls==2);
 assert(esp_supp_dpp_init((void *)1)==ESP_ERR_NOT_SUPPORTED && calls==2);
 return 0;
}
''')
    exe=d/'test.exe'
    subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(d),
        '-I'+str(root/'examples/link-apps/common'),str(d/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS real DPP adapter: SDK return propagation, NULL callback, explicit unsupported callback')
