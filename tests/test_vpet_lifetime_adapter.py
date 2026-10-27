"""Application-specific adapter check using the candidate's actual model type."""
import argparse,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);p.add_argument('--candidate',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];headers=a.candidate.resolve()/'main'
assert (headers/'vpet_app.h').is_file()
with tempfile.TemporaryDirectory(prefix='vpet-lifetime-') as temp:
    d=Path(temp);(d/'bsp_button.h').write_text('typedef int bsp_btn_t;typedef int bsp_btn_ev_t;\n')
    (d/'test.c').write_text('''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "vpet_app.h"
static vpet_model_t *retained;
static uintptr_t input;
static bool battery;
bool __real_vpet_app_start(vpet_model_t *model,bool ready) {retained=model;battery=ready;return ready;}
extern bool __wrap_vpet_app_start(vpet_model_t *,bool);
static void start(void) {
 vpet_model_t local;memset(&local,0,sizeof(local));local.alive=1;local.seed=0x12345678;local.trainings=23;
 input=(uintptr_t)&local;assert(__wrap_vpet_app_start(&local,true));
 local.seed=0;assert(retained->seed==0x12345678);
}
int main(void) {
 start();assert((uintptr_t)retained!=input && battery);
 assert(retained->alive==1 && retained->trainings==23 && retained->seed==0x12345678);
 vpet_model_t next;memset(&next,0,sizeof(next));next.trainings=77;
 assert(!__wrap_vpet_app_start(&next,false));assert(!battery && retained->trainings==77);
 return 0;
}
''')
    exe=d/'test.exe'
    subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(d),'-I'+str(headers),
        str(root/'examples/link-apps/vpet/context-lifetime.c'),str(d/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('V-Pet context adapter: PASS (actual model type, independent storage, forwarding)')
