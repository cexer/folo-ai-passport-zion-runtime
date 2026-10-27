/* Test-only ADC samples. The original application still owns debounce, long
 * presses and its event queue. Uninjected reads use the real calibrated ADC. */
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "bsp_pins.h"
#include <stdbool.h>
#include <stdint.h>
static portMUX_TYPE adc_lock=portMUX_INITIALIZER_UNLOCKED;
static int64_t release_us;
static int value_mv;
#ifdef ZION_STANDALONE_TEST
static const uint16_t windows[BSP_BTN_COUNT][2]=BSP_BTN_MV_TABLE;
extern int __real_bsp_button_read_mv(void);
#define REAL_READ __real_bsp_button_read_mv
#define READ_NAME __wrap_bsp_button_read_mv
#else
#include "bsp_button.h"
extern int bsp_button_read_mv(void);
#define REAL_READ bsp_button_read_mv
#define READ_NAME zion_test_adc_read
#endif
bool zion_test_adc_pulse(unsigned key,unsigned milliseconds) {
    int voltage;
#ifdef ZION_STANDALONE_TEST
    voltage=key<BSP_BTN_COUNT?(windows[key][0]+windows[key][1])/2:-1;
#else
    /* Passport OS's OK range is 450..1000 mV: the old host midpoint 1173
     * falls outside it. Follow the real negotiated ladder for every app. */
    voltage=bsp_button_key_voltage(key);
#endif
    if(voltage<0 || milliseconds>10000) return false;
    int64_t until=esp_timer_get_time()+(int64_t)milliseconds*1000;
    portENTER_CRITICAL(&adc_lock);
    value_mv=voltage;release_us=until;
    portEXIT_CRITICAL(&adc_lock);
    return true;
}
void zion_test_adc_key(unsigned key,bool long_press) {
    (void)zion_test_adc_pulse(key,long_press?1050:140);
}
int READ_NAME(void) {
    int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&adc_lock);
    bool active=now<release_us;int value=value_mv;
    portEXIT_CRITICAL(&adc_lock);
    return active?value:REAL_READ();
}
