"""The real test-only sleep guard must reset, without returning to a torn-down app."""
import argparse,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-sleep-guard-') as folder:
    d=Path(folder)
    (d/'esp_system.h').write_text('_Noreturn void esp_restart(void);\n')
    (d/'esp_log.h').write_text('#define ESP_LOGW(...) ((void)0)\n')
    (d/'test.c').write_text('''#include <assert.h>
#include <setjmp.h>
static jmp_buf escaped;
static int resets;
_Noreturn void esp_restart(void) {resets++;longjmp(escaped,1);}
void __wrap_esp_deep_sleep_start(void);
int main(void) {if(!setjmp(escaped)){__wrap_esp_deep_sleep_start();assert(0);}assert(resets==1);return 0;}
''')
    exe=d/'test.exe'
    subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(d),str(d/'test.c'),
        str(root/'tests/firmware/sleep_guard.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS real test-only sleep guard resets once and never returns to app')
