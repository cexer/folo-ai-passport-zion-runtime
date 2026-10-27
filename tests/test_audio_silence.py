"""Compile the real silence interceptor against a recording codec/I2S peer."""
import pathlib, subprocess, tempfile, argparse
p = argparse.ArgumentParser()
p.add_argument('--cc', required=True)
a = p.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-silent-') as d:
    d = pathlib.Path(d)
    (d/'driver').mkdir()
    (d/'esp_codec_dev.h').write_text('''#include <stdbool.h>
typedef void *esp_codec_dev_handle_t;
typedef struct {int rate;} esp_codec_dev_sample_info_t;
''')
    (d/'driver/i2s_std.h').write_text('''#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
typedef void *i2s_chan_handle_t;
#define ESP_OK 0
''')
    (d/'esp_timer.h').write_text('#include <stdint.h>\nint64_t esp_timer_get_time(void);\n')
    (d/'esp_log.h').write_text('#define ESP_LOGI(...) ((void)0)\n')
    (d/'test.c').write_text('''#include <assert.h>
#include <string.h>
#include "esp_codec_dev.h"
#include "driver/i2s_std.h"
static int mute_calls, volume_calls, writes, mode;
static size_t bytes;
static int64_t now;
int64_t esp_timer_get_time(void) {now+=1000;return now;}
int __real_esp_codec_dev_open(void *h, esp_codec_dev_sample_info_t *p) {(void)h;(void)p;return mode==3?-1:0;}
int __real_esp_codec_dev_set_out_vol(void *h,int n) {(void)h;assert(n==0);volume_calls++;return 0;}
int __real_esp_codec_dev_set_out_mute(void *h,bool n) {(void)h;assert(n);mute_calls++;return 0;}
int __real_esp_codec_dev_write(void *h,void *p,int n) {(void)h;assert(*(unsigned char *)p==77);writes++;return n;}
int __real_i2s_channel_write(void *h,const void *p,size_t n,size_t *out,uint32_t t) {
 (void)h;(void)t;
 if(!p||!n)return -2;
 for(size_t i=0;i<n;i++)assert(((const unsigned char *)p)[i]==0);
 *out=mode==1?n/2:n;bytes+=*out;return mode==1?-1:0;
}
int __wrap_esp_codec_dev_open(void *,esp_codec_dev_sample_info_t *);
int __wrap_esp_codec_dev_set_out_vol(void *,int);
int __wrap_esp_codec_dev_set_out_mute(void *,bool);
int __wrap_esp_codec_dev_write(void *,void *,int);
int __wrap_i2s_channel_write(void *,const void *,size_t,size_t *,uint32_t);
int main(void) {
 unsigned char source[1000];memset(source,77,sizeof(source));size_t written=0;
 assert(!__wrap_esp_codec_dev_open((void *)1,0));assert(mute_calls==1&&volume_calls==1);
 assert(!__wrap_esp_codec_dev_set_out_vol((void *)1,90));
 assert(!__wrap_esp_codec_dev_set_out_mute((void *)1,false));
 int before_write=mute_calls+volume_calls;
 assert(__wrap_esp_codec_dev_write((void *)1,source,1000)==1000&&writes==1);
 assert(mute_calls+volume_calls==before_write); /* no per-block I2C traffic */
 assert(!__wrap_i2s_channel_write((void *)1,source,1000,&written,100));
 assert(written==1000&&bytes==1000&&source[0]==77);
 mode=1;assert(__wrap_i2s_channel_write((void *)1,source,1000,&written,0)==-1&&written==128);
 assert(__wrap_i2s_channel_write((void *)1,0,1000,&written,0)==-2);
 mode=3;int previous=mute_calls;assert(__wrap_esp_codec_dev_open((void *)1,0)==-1&&mute_calls==previous);
 return 0;
}
''')
    exe = d/'test.exe'
    subprocess.run([a.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(d),
                    str(root/'tests/firmware/audio_silence.c'), str(d/'test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: forced codec mute, zero PCM, untouched source, partial/error propagation')
