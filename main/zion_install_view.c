#include "zion_install_view.h"
#include <stdio.h>
#include <string.h>
uint32_t zion_install_color(unsigned stage) {
    return stage==255?0xE43B2Fu:stage==100?0x6FD18Cu:0xEACD62u;
}

const char *zion_install_reason(const char *detail) {
    static const struct {const char *code,*text;} reasons[]={
        {"source-size","安装包：文件大小超出范围"},
        {"source-partition-md5","分区表：校验失败"},
        {"source-partition-range","分区表：地址或类型不支持"},
        {"source-partition-overlap","分区表：重叠或名称重复"},
        {"source-app-layout","应用：布局或数量不支持"},
        {"source-app-invalid","应用：布局或镜像头无效"},
        {"source-app-changed","应用：下载内容发生变化"},
        {"source-layout-changed","分区表：下载内容发生变化"},
        {"unsupported-child-data","资源：分区类型或数量不支持"},
        {"factory-image-invalid","应用：factory 镜像无效"},
        {"ota-image-invalid","应用：OTA 镜像无效"},
        {"ota-image-ambiguous","应用：无法确定 OTA 镜像"},
        {"source-truncated","安装包：数据不完整"},
        {"source-stream-length","安装包：长度不匹配"},
        {"signature-tail-truncated","签名尾部：数据不完整"},
        {"allocation-no-space","存储：空间、连续区或槽位不足"},
        {"allocation-app-too-large","存储：程序超出连续空间"},
        {"signature-tail-capacity","签名尾部：空间不足"},
        {"signature-tail-readback","签名尾部：回读失败"},
        {"allocation-slot-limit","存储：玩法槽位已满"},
        {"allocation-data-limit","存储：资源分区数量已满"},
        {"https-client-allocation","下载：网络内存不足"},
        {"preflight-allocation","检查：内存不足"},
        {"tail-allocation","签名尾部：内存不足"},
        {"https-open","下载：HTTPS 连接失败"},
        {"https-status-or-length","下载：HTTP 状态或长度不符"},
        {"https-read-timeout","下载：读取超时"},
        {"https-read-truncated","下载：数据提前结束"},
        {"https-read-error","下载：读取失败"},
        {"package-sha256","安装包：SHA256 校验失败"},
        {"reserve-or-erase","存储：分配记录或擦除失败"},
        {"flash-write-readback","Flash：写入或回读校验失败"},
        {"app-header-publish","应用头：写入失败"},
        {"app-image-verification","应用：镜像校验失败"},
        {"name-metadata-write","名称：写入失败"},
        {"record-or-table-commit","安装记录：提交失败"},
        {"failed-header-cleanup","清理：无效应用头擦除失败"},
        {"cancelled","操作：用户已取消"},
        {"backend-error","安装后端：内部错误"},
        {"install-queue-full","调度：安装队列已满"},
    };
    if(!detail) return "安装后端：原因未知";
    for(unsigned i=0;i<sizeof(reasons)/sizeof(reasons[0]);i++)
        if(!strcmp(detail,reasons[i].code)) return reasons[i].text;
    return detail; /* Preserve unknown machine codes instead of hiding the layer. */
}
static void size_text(char *out,size_t capacity,uint32_t bytes) {
    /* Integer-only formatting: stable on the host and the no-PSRAM target. */
    uint64_t hundredths=((uint64_t)bytes*100+524288)/1048576;
    snprintf(out,capacity,"%lu.%02lu MiB",(unsigned long)(hundredths/100),(unsigned long)(hundredths%100));
}
void zion_install_format(char *out,size_t capacity,const zion_install_info_t *info,
                         unsigned stage,unsigned percent,const char *detail) {
    const char *phase=stage==5?"正在获取玩法名称":stage==10?"正在读取安装布局":stage==20?"正在准备存储":
        stage==30?"正在下载并写入":stage==40?"正在校验应用":
        stage==50?"正在保存安装记录":stage==100?"安装完成":
        stage==255?(!strcmp(detail?detail:"","cancelled")?"安装已取消":
                     info->phase<=10?"无法安装":"安装失败"):"正在处理";
    char package[32],installed[32],progress[24],speed[40],diagnostic[96]="";
    const char *cleanup=info->cleanup==1?"\n空间已回收":info->cleanup==2?"\n清理未完成，请在管理中删除":
        info->cleanup==3?"\n有效安装已保留，请重启恢复":"";
    size_text(package,sizeof(package),info->package_bytes);
    if(info->install_known) size_text(installed,sizeof(installed),info->install_bytes);
    else snprintf(installed,sizeof(installed),"%s",stage==255?"未能确定":"正在解析");
    snprintf(progress,sizeof(progress),"%u%%",percent>100?100:percent);
    snprintf(speed,sizeof(speed),"%lu.%01lu KiB/s",(unsigned long)(info->bytes_per_second/1024),
        (unsigned long)((info->bytes_per_second%1024)*10/1024));
    if(stage==255 && (info->read_result || info->read_errno || info->tls_error))
        snprintf(diagnostic,sizeof(diagnostic),"\n读取%d errno%d TLS%d",info->read_result,info->read_errno,info->tls_error);
    snprintf(out,capacity,"%s\n\n%s：%s\n下载：%s\n占用：%s\n进度：%s\n速度：%s%s%s%s%s",
        phase,info->project_name?"工程":"名称",info->name[0]?info->name:"暂未获取",package,installed,progress,speed,
        stage==255?"\n\n":"",stage==255?zion_install_reason(detail):"",diagnostic,cleanup);
}
