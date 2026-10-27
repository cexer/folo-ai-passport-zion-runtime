"""Check timed ADC injection expiration, rejection and real-read fallback."""
import argparse,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-adc-pulse-') as temp:
    d=Path(temp);(d/'freertos').mkdir()
    (d/'freertos/FreeRTOS.h').write_text('''
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
''')
    (d/'esp_timer.h').write_text('#include <stdint.h>\nint64_t esp_timer_get_time(void);\n')
    (d/'bsp_pins.h').write_text('#define BSP_BTN_COUNT 3\n#define BSP_BTN_MV_TABLE {{0,150},{150,447},{447,1900}}\n')
    (d/'bsp_button.h').write_text('int bsp_button_key_voltage(unsigned key);\n')
    (d/'test.c').write_text('''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
static int64_t now;
static int configured_ok=1173;
int bsp_button_key_voltage(unsigned key) {return key==0?75:key==1?298:key==2?configured_ok:-1;}
int64_t esp_timer_get_time(void) {return now;}
int bsp_button_read_mv(void) {return 3300;}
extern bool zion_test_adc_pulse(unsigned,unsigned);
extern void zion_test_adc_key(unsigned,bool);
extern int zion_test_adc_read(void);
int main(void) {
 assert(zion_test_adc_read()==3300);
 assert(zion_test_adc_pulse(2,1500));assert(zion_test_adc_read()==1173);
 now=1499999;assert(zion_test_adc_read()==1173);
 assert(!zion_test_adc_pulse(3,500));assert(!zion_test_adc_pulse(0,10001));
 assert(zion_test_adc_read()==1173);now=1500000;assert(zion_test_adc_read()==3300);
 zion_test_adc_key(0,false);assert(zion_test_adc_read()==75);
 now+=140000;assert(zion_test_adc_read()==3300);
 zion_test_adc_key(1,true);assert(zion_test_adc_read()==298);
 now+=1050000;assert(zion_test_adc_read()==3300);
 assert(zion_test_adc_pulse(0,100));assert(zion_test_adc_pulse(0,0));assert(zion_test_adc_read()==3300);
 configured_ok=725;assert(zion_test_adc_pulse(2,100));assert(zion_test_adc_read()==725);
 return 0;
}
''')
    exe=d/'test.exe'
    subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(d),
        str(root/'tests/firmware/adc_key_control.c'),str(d/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Timed ADC input: PASS (expiration, bounds, duration and hardware fallback)')
