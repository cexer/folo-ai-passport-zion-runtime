#include "zion_package.h"
#include "meta_carve_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void write32(unsigned char *p,unsigned v) {for(unsigned i=0;i<4;i++) p[i]=(unsigned char)(v>>(i*8));}
static void seal(unsigned char *h) {write32(h+252,meta_carve_crc32(h,252));}
int main(int argc,char **argv) {
    unsigned char original[256]={0},bad[256];
    if(argc==2) {
        FILE *file=fopen(argv[1],"rb");assert(file);
        assert(fread(original,1,sizeof(original),file)==sizeof(original));fclose(file);
    } else {
        assert(argc==1);memcpy(original,"ZPK1",4);original[4]=1;original[7]=1;
        write32(original+8,1);write32(original+12,308);write32(original+16,0x80000001u);
        write32(original+20,1);write32(original+24,96);write32(original+28,256);write32(original+32,52);write32(original+36,308);
        memcpy(original+48,"Fixture",8);memcpy(original+88,"fixture",8);seal(original);
    }
    zion_package_t info;assert(zion_package_parse(original,&info));
    assert(info.elf_offset==256 && info.total>=256 && info.id!=0xffffffffu);
    memcpy(bad,original,256);bad[48]^=1;assert(!zion_package_parse(bad,&info)); /* CRC */
    memcpy(bad,original,256);write32(bad+16,0xffffffffu);seal(bad);assert(!zion_package_parse(bad,&info)); /* staging ID */
    memcpy(bad,original,256);write32(bad+44,2);seal(bad);assert(!zion_package_parse(bad,&info)); /* unknown service requirement */
    memcpy(bad,original,256);write32(bad+44,8);seal(bad);assert(zion_package_parse(bad,&info) && info.features==8);
    write32(bad+44,24);seal(bad);assert(zion_package_parse(bad,&info) && info.features==24);
    write32(bad+44,32);seal(bad);assert(!zion_package_parse(bad,&info)); /* SPIFFS requires storage */
    write32(bad+44,64);seal(bad);assert(!zion_package_parse(bad,&info)); /* unsupported capability */
    memcpy(bad,original,256);bad[48]='\n';seal(bad);assert(!zion_package_parse(bad,&info));
    memcpy(bad,original,256);memset(bad+88,'a',16);seal(bad);assert(!zion_package_parse(bad,&info)); /* unterminated key */
    memcpy(bad,original,256);write32(bad+36,0xfffffff0u);seal(bad);assert(!zion_package_parse(bad,&info)); /* offset overflow */
    memcpy(bad,original,256);write32(bad+44,2);write32(bad+136,2097152);memcpy(bad+140,"recordings",11);seal(bad);
    assert(zion_package_parse(bad,&info) && info.storage_size==2097152 && !strcmp(info.storage_label,"recordings"));
    assert(info.storage_subtype==0x81); /* Existing packages remain FAT. */
    write32(bad+44,34);seal(bad);
    assert(zion_package_parse(bad,&info) && info.storage_subtype==0x82);
    write32(bad+44,2);seal(bad);
    memcpy(bad+140,"zp_80000030\0",12);seal(bad);assert(!zion_package_parse(bad,&info));
    memset(bad+140,0,16);memcpy(bad+140,"recordings",11);
    write32(bad+44,6);write32(bad+160,40960);seal(bad);
    assert(zion_package_parse(bad,&info) && info.code_cache_size==40960);
    uint32_t extent=0;
    assert(zion_package_extent(&info,0x800000,&extent));
    assert(extent==((info.total+65535u)&~65535u)+40960);
    info.flash_offset=0x273000;assert(zion_package_extent(&info,0x800000,&extent));
    assert(((info.flash_offset+extent-info.code_cache_size)&65535u)==0);
    assert(!zion_package_extent(&info,extent-1,&extent));
    write32(bad+160,40961);seal(bad);assert(!zion_package_parse(bad,&info));
    write32(bad+160,40960);
    write32(bad+136,2097153);seal(bad);assert(!zion_package_parse(bad,&info));
    /* File admission is independent of available runtime RAM or Flash placement. */
    const unsigned sizes[]={262145u,1048577u,7000000u};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) {
        unsigned length=sizes[i];
        memcpy(bad,original,256);write32(bad+32,length);write32(bad+36,256+length);
        write32(bad+40,0);write32(bad+12,256+length);seal(bad);
        assert(zion_package_parse(bad,&info) && info.elf_size==length);
    }
    puts("ZPK1: PASS (real headers, CRC, identity, service flags, lengths, name/key and large ELF admission)");
    return 0;
}
