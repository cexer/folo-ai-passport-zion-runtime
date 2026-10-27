/* Native-test counterpart of the plugin's stdin broker. Reuse the same byte,
 * timeout and file-delegation logic without the C library's sticky stdin EOF.
 * Only the diagnostic fixture links this; business sources remain unchanged. */
#include <stdio.h>
extern int __real_fgetc(FILE *stream);
#define fgetc __real_fgetc
#define zion_test_fgetc __wrap_fgetc
#define zion_test_usb_read __wrap_usb_serial_jtag_read_bytes
#include "stdio_control.c"
