#include <stdint.h>
#include <stdio.h>
#include "fixture.h"
static nv_rec_t record;
static nv_player_t player;
static int16_t block[320];
int main(int argc,char **argv) {
    FILE *output=argc==2?fopen(argv[1],"wb"):NULL;
    if(argc==2 && !output) return 3;
    seed_record(&record,block);
    printf("ADPCM samples=%ld bytes=%d\n",(long)record.n8,nv_rec_bytes(&record));
    for(int voice=0;voice<NV_COUNT;voice++) {
        uint32_t count=0,hash=2166136261u;nv_play_start(&player,&record,voice);
        while(!player.done) {
            int n=nv_play_fill(&player,block,320);if(n<=0) break;
            count+=(uint32_t)n;hash=pcm_hash(hash,block,(unsigned)n);
            if(output && fwrite(block,sizeof(block[0]),(size_t)n,output)!=(size_t)n) return 4;
            if(count>32000) return 1;
        }
        if(!count) return 2;
        printf("VOICE %d samples=%lu hash=%08lx\n",voice,(unsigned long)count,(unsigned long)hash);
    }
    if(output && fclose(output)!=0) return 5;
    return 0;
}
