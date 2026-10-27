#pragma once
#include "nabo_voice.h"
static int16_t sample_at(unsigned i) {
    int phase=(int)(i%80u);int triangle=phase<40?phase:80-phase;
    return (int16_t)((triangle-20)*400+((int)(i%13u)-6)*35);
}
static uint32_t pcm_hash(uint32_t hash,const int16_t *pcm,unsigned count) {
    for(unsigned i=0;i<count;i++) {uint16_t value=(uint16_t)pcm[i];hash=(hash^(value&255u))*16777619u;hash=(hash^(value>>8))*16777619u;}
    return hash;
}
static void seed_record(nv_rec_t *record,int16_t *block) {
    nv_rec_start(record);
    for(unsigned offset=0;offset<3200;offset+=320) {
        for(unsigned j=0;j<320;j++) block[j]=sample_at(offset+j);
        (void)nv_rec_push(record,block,320);
    }
}
