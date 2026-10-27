/* DeviceTest-only: Passport OS owns stdin through fgetc(), whereas earlier
 * apps read usb_serial_jtag_read_bytes directly. Both must share the same
 * control-line demultiplexer, not race a second serial reader. */
#include <stdio.h>
#include <stdint.h>
extern int zion_test_usb_read(void *,uint32_t,uint32_t);
int zion_test_fgetc(FILE *stream) {
    if(stream!=stdin) return fgetc(stream);
    unsigned char ch;
    return zion_test_usb_read(&ch,1,100)>0?(int)ch:EOF;
}
