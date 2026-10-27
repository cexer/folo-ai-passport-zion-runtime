/* Actual carve allocator/table under a factory end which is not 64 KiB aligned. */
#include "meta_carve.h"
#include "zion_package.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert((META_FACTORY_SIZE&4095u)==0 && (META_POOL0_START&65535u)!=0);
    meta_carve_t carve={0},next;
    uint32_t initial=meta_carve_free(&carve),offset;
    assert(meta_carve_place_data(&carve,0x6000,&offset));
    uint32_t first=(META_POOL0_START+65535u)&~65535u;
    assert(offset==first);
    meta_carve_data_t data={.play_id=0x80000001u,.offset=offset,.size=0x6000,
        .type=1,.subtype=0x41,.state=META_DATA_PRISTINE,.label="zp_layout"};
    assert(meta_carve_data_append(&carve,&data));
    int app=meta_carve_place(&carve,0x20000,META_CARVE_KIND_APP,&next);
    assert(app>=0);carve=next;
    assert(carve.slot[app].offset==first+0x10000 && meta_carve_valid(&carve));
    assert(meta_carve_free(&carve)==initial-0x6000-0x20000);
    assert(meta_carve_place_data(&carve,0x190000,&offset) && offset==0x360000);
    uint8_t bytes[META_PT_SIZE];meta_pt_t table;
    assert(meta_pt_from_carve(&carve,bytes) && meta_pt_decode(bytes,&table));
    bool factory=false,identity=false;
    for(unsigned i=0;i<table.count;i++) {
        const meta_pt_entry_t *entry=&table.e[i];
        if(!strcmp(entry->label,"factory")) {factory=true;assert(entry->offset==0x10000 && entry->size==META_FACTORY_SIZE);}
        if(!strcmp(entry->label,"cardid")) {identity=true;assert(entry->offset==0x356000 && entry->size==0x4000);}
    }
    assert(factory&&identity);
    assert(meta_carve_remove(&carve,(uint8_t)app) && meta_carve_remove_data(&carve,0));
    assert(meta_carve_free(&carve)==initial && meta_carve_valid(&carve));
    /* Repeated reader update: old at 0x500000, exact new extent fits the
     * 0x360000..0x500000 hole; a worst-case padding estimate does not. */
    zion_package_t package={.total=1587344,.code_cache_size=57344};uint32_t exact;
    assert(zion_package_extent(&package,0x800000,&exact) && exact==0x19e000);
    data.offset=0x500000;data.size=exact;
    assert(meta_carve_data_append(&carve,&data) && meta_carve_valid(&carve));
    uint32_t worst=((package.total+4095u)&~4095u)+61440u+package.code_cache_size;
    assert(!meta_carve_place_data(&carve,worst,&offset));
    assert(meta_carve_place_data(&carve,exact,&offset) && offset==0x360000);
    package.flash_offset=offset;uint32_t placed;
    assert(zion_package_extent(&package,exact,&placed) && placed==exact);
    assert(carve.data_count==1 && carve.data[0].offset==0x500000); /* old remains live */
    puts("runtime layout: PASS (unaligned factory end, app alignment, identity and exact reclamation)");
    return 0;
}
