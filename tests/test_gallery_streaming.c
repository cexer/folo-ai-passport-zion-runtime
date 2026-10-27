#include "fga_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TILE_BYTES (FGA_FRAME_BYTES/5)
typedef struct {const uint8_t *bytes;size_t size;} source_t;
typedef struct {uint8_t tile[5][TILE_BYTES+2];} surface_t;
static bool read_bytes(void *context,uint32_t offset,void *out,size_t size) {
    source_t *s=context;if(offset>s->size || size>s->size-offset) return false;
    memcpy(out,s->bytes+offset,size);return true;
}
static bool write_bytes(void *context,uint32_t offset,const void *input,size_t size) {
    surface_t *s=context;const uint8_t *bytes=input;
    if(offset>FGA_FRAME_BYTES || size>FGA_FRAME_BYTES-offset) return false;
    while(size) {
        unsigned index=offset/TILE_BYTES,position=offset%TILE_BYTES;
        size_t count=TILE_BYTES-position;if(count>size) count=size;
        memcpy(s->tile[index]+1+position,bytes,count);offset+=(uint32_t)count;bytes+=count;size-=count;
    }
    return true;
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);
    assert(!fseek(file,0,SEEK_END));long size=ftell(file);assert(size>0);rewind(file);
    uint8_t *data=malloc((size_t)size);assert(data && fread(data,1,(size_t)size,file)==(size_t)size);fclose(file);
    source_t source={data,(size_t)size};fga_reader_t reader={read_bytes,&source};fga_header_t header;
    assert(fga_validate(&reader,(uint32_t)size,&header));assert(header.frame_count==3);
    uint8_t *contiguous=calloc(1,FGA_FRAME_BYTES);surface_t *surface=malloc(sizeof(*surface));assert(contiguous && surface);
    memset(surface,0,sizeof(*surface));
    for(unsigned i=0;i<5;i++) surface->tile[i][0]=surface->tile[i][TILE_BYTES+1]=0x5a;
    uint32_t offset=header.first_frame_offset;
    for(unsigned i=0;i<header.frame_count;i++) {
        fga_frame_t full,stream;
        assert(fga_read_frame(&reader,&header,offset,contiguous,FGA_FRAME_BYTES,&full));
        assert(fga_read_frame_to_sink(&reader,&header,offset,write_bytes,surface,&stream));
        assert(full.next_offset==stream.next_offset && full.delay_ms==stream.delay_ms);
        for(unsigned tile=0;tile<5;tile++) {
            assert(surface->tile[tile][0]==0x5a && surface->tile[tile][TILE_BYTES+1]==0x5a);
            assert(!memcmp(surface->tile[tile]+1,contiguous+tile*TILE_BYTES,TILE_BYTES));
        }
        offset=full.next_offset;
    }
    assert(offset==(uint32_t)size);
    assert(write_bytes(surface,TILE_BYTES-2,"ABCD",4));
    assert(!memcmp(surface->tile[0]+1+TILE_BYTES-2,"AB",2));
    assert(!memcmp(surface->tile[1]+1,"CD",2));
    assert(surface->tile[0][TILE_BYTES+1]==0x5a && surface->tile[1][0]==0x5a);
    assert(!write_bytes(surface,FGA_FRAME_BYTES-1,"xx",2));
    data[24]^=1;assert(!fga_validate(&reader,(uint32_t)size,&header));
    free(surface);free(contiguous);free(data);
    puts("gallery streaming: PASS (full/delta parity, five tile guards, CRC rejection)");
}
