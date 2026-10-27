#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int zion_test_fgetc(FILE *);
#ifdef ZION_NATIVE_STDIO_TEST
int __real_fgetc(FILE *stream) {return fgetc(stream);}
#endif
static int calls;
int zion_test_usb_read(void *out,uint32_t bytes,uint32_t wait) {
    assert(bytes==1&&wait==100);
    if(calls++==0) {*(unsigned char *)out=0xff;return 1;}
    if(calls==2) return 0;
    *(unsigned char *)out='o';return 1;
}
int main(void) {
    assert(zion_test_fgetc(stdin)==255); /* no signed-char/EOF confusion */
    assert(zion_test_fgetc(stdin)==EOF);
    assert(zion_test_fgetc(stdin)=='o'); /* a timeout is not permanent EOF */
    FILE *file=tmpfile();assert(file);assert(fputc('x',file)=='x');rewind(file);
    assert(zion_test_fgetc(file)=='x'&&calls==3);fclose(file);
    puts("stdio broker: PASS (stdin control broker, unsigned bytes, timeout recovery, ordinary file delegation)");
}
