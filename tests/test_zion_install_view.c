#include "zion_install_view.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(zion_install_color(30)==0xEACD62u);
    assert(zion_install_color(100)==0x6FD18Cu);
    assert(zion_install_color(255)==0xE43B2Fu);
    char text[640];
    zion_install_info_t info={.package_bytes=7616468,.phase=10};
    zion_install_format(text,sizeof(text),&info,10,0,"preflight");
    assert(strstr(text,"7.26 MiB") && strstr(text,"正在解析") && strstr(text,"暂未获取"));
    strcpy(info.name,"随身小智");info.install_known=true;info.install_bytes=4497408;
    zion_install_format(text,sizeof(text),&info,30,57,"writing");
    assert(strstr(text,"随身小智") && strstr(text,"4.29 MiB") && strstr(text,"57%"));
    zion_install_format(text,sizeof(text),&info,100,100,"installed");
    assert(strstr(text,"安装完成") && !strstr(text,"即将启动"));
    info.cleanup=1;
    zion_install_format(text,sizeof(text),&info,255,57,"package-sha256");
    assert(strstr(text,"空间已回收") && strstr(text,"SHA256"));
    info.cleanup=2;
    zion_install_format(text,sizeof(text),&info,255,57,"https-read-error");
    assert(strstr(text,"清理未完成") && strstr(text,"读取失败"));
    info.cleanup=0;
    zion_install_format(text,sizeof(text),&info,255,45,"allocation-no-space");
    assert(strstr(text,"无法安装") && strstr(text,"连续区") && strstr(text,"4.29 MiB"));
    info.phase=30;
    zion_install_format(text,sizeof(text),&info,255,68,"https-read-timeout");
    assert(strstr(text,"安装失败") && strstr(text,"下载：读取超时") && strstr(text,"68%"));
    info.install_known=false;
    zion_install_format(text,sizeof(text),&info,255,0,"source-partition-md5");
    assert(strstr(text,"未能确定") && strstr(text,"分区表：校验失败"));
    assert(!strstr(text,"占用：0.00"));
    zion_install_format(text,sizeof(text),&info,255,6,"cancelled");
    assert(strstr(text,"安装已取消"));
    assert(!strcmp(zion_install_reason("future-layer-code"),"future-layer-code"));
    for(unsigned n=1;n<16;n++) {
        memset(text,0x7a,sizeof(text));
        zion_install_format(text,n,&info,255,150,"https-open");
        assert(text[n-1]==0 && text[n]=='z');
    }
    puts("PASS install view: sizes, progress, unsupported/failed/cancelled, unknown facts, bounded output");
}
