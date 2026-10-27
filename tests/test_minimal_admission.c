/* Exercise the real streaming parser and allocator with actual firmware files.
 * No hardware writes; command arguments are merged firmware paths. */
#include "zion592_image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv) {
    meta_carve_t current={0},next;
    for(int arg=1;arg<argc;arg++) {
        FILE *f=fopen(argv[arg],"rb");assert(f);
        assert(fseek(f,0,SEEK_END)==0);long len=ftell(f);rewind(f);
        assert(len>0 && len<=0x800000);
        z592_image_t image;z592_image_init(&image,(uint32_t)len,true);
        unsigned char chunk[1024];size_t n;
        while((n=fread(chunk,1,sizeof(chunk),f))) assert(z592_image_feed(&image,chunk,n));
        fclose(f);assert(z592_image_finish(&image));
        int slot=-1;
        bool fit=z592_image_allocate(&image,&current,-1,900000u+(unsigned)arg,&next,&slot);
        printf("{\"file_bytes\":%ld,\"app_bytes\":%lu,\"app_need\":%lu,\"data_count\":%u,\"fit\":%s",
            len,(unsigned long)image.apps[image.selected].image_len,
            (unsigned long)meta_carve_need(image.apps[image.selected].image_len),
            image.data_count,fit?"true":"false");
        if(fit) {
            assert(slot>=0 && slot<META_CARVE_MAX_SLOTS);
            next.slot[slot].state=META_SLOT_VALID;
            current=next;
            printf(",\"slots\":%u,\"free\":%lu,\"largest_gap\":%lu",current.count,
                (unsigned long)meta_carve_free(&current),(unsigned long)meta_carve_largest_gap(&current));
        }
        puts("}");
    }
    return 0;
}
