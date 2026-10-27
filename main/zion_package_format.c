#include "zion_package.h"
#include "zion_sdk.h"
#include "meta_carve_store.h"
#include "meta_name.h"
#include <string.h>
static uint32_t rd32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
bool zion_package_parse(const uint8_t h[ZION_PACKAGE_HEADER],zion_package_t *p) {
    if(!h || !p || memcmp(h,"ZPK1",4) || h[4]!=1 || h[5] || h[6] || h[7]!=1 ||
       rd32(h+252)!=meta_carve_crc32(h,252)) return false;
    memset(p,0,sizeof(*p));
    p->flags=rd32(h+8);p->total=rd32(h+12);p->id=rd32(h+16);p->abi=rd32(h+20);p->api_size=rd32(h+24);
    p->elf_offset=rd32(h+28);p->elf_size=rd32(h+32);p->asset_offset=rd32(h+36);p->asset_size=rd32(h+40);
    p->features=rd32(h+44);p->entry_stack=rd32(h+156);
    if(p->entry_stack && (p->entry_stack<2048 || (p->entry_stack&3u))) return false;
    if(p->features&4u) {
        p->code_cache_size=rd32(h+160);
        if(!p->code_cache_size || (p->code_cache_size&4095u)) return false;
    }
    memcpy(p->name,h+48,40);memcpy(p->key,h+88,16);memcpy(p->sha256,h+104,32);
    if((p->features&ZION_PACKAGE_FEATURE_SPIFFS) && !(p->features&2u)) return false;
    if((p->flags!=1 && p->flags!=3) || (p->features&~63u) || p->total<ZION_PACKAGE_HEADER ||
       !(p->id&0x80000000u) || p->id==UINT32_MAX || p->abi!=ZION_ABI_VERSION ||
       p->api_size<offsetof(zion_api_t,uptime_ms) || p->api_size>sizeof(zion_api_t) ||
       p->elf_offset!=ZION_PACKAGE_HEADER || p->elf_size<52 ||
       p->elf_size>p->total-p->elf_offset || p->asset_offset!=p->elf_offset+p->elf_size ||
       p->asset_size!=p->total-p->asset_offset || !p->name[0] || !memchr(h+48,0,40) || !p->key[0] || !memchr(h+88,0,16)) return false;
    if(p->features&2u) {
        p->storage_subtype=(p->features&ZION_PACKAGE_FEATURE_SPIFFS)?0x82:0x81;
        p->storage_size=rd32(h+136);memcpy(p->storage_label,h+140,16);
        if(p->storage_size<4096 || (p->storage_size&4095u) || !p->storage_label[0] || !memchr(h+140,0,16)) return false;
        if(!strncmp(p->storage_label,"zp_",3) || !strcmp(p->storage_label,"nvs") ||
           !strcmp(p->storage_label,"factory") || !strcmp(p->storage_label,"phy_init") ||
           !strcmp(p->storage_label,"cardid") || !strcmp(p->storage_label,"store") ||
           !strcmp(p->storage_label,"otadata")) return false;
        for(unsigned i=0;p->storage_label[i];i++) if(!((p->storage_label[i]>='a' && p->storage_label[i]<='z') ||
           (p->storage_label[i]>='0' && p->storage_label[i]<='9') || p->storage_label[i]=='_')) return false;
    }
    if(strlen(p->name)>31 || !meta_name_utf8_ok(p->name,strlen(p->name))) return false;
    for(unsigned i=0;p->name[i];i++) if((unsigned char)p->name[i]<32) return false;
    for(unsigned i=0;p->key[i];i++) if(!((p->key[i]>='a' && p->key[i]<='z') ||
       (p->key[i]>='0' && p->key[i]<='9') || p->key[i]=='-') || i>=12) return false;
    return true;
}

bool zion_package_extent(const zion_package_t *p,uint32_t flash_size,uint32_t *extent) {
    if(!p || !extent || p->total>UINT32_MAX-4095u) return false;
    uint32_t file=(p->total+4095u)&~4095u,padding=0;
    if(p->code_cache_size) {
        if(p->flash_offset>UINT32_MAX-file) return false;
        padding=(0u-(p->flash_offset+file))&65535u;
    }
    if(file>flash_size || padding>flash_size-file || p->code_cache_size>flash_size-file-padding) return false;
    *extent=file+padding+p->code_cache_size;return true;
}
