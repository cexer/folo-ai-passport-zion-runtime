#include "zion_sdk.h"
#include "animation_player.h"
const uint32_t zion_plugin_abi=ZION_ABI_VERSION;
const uint32_t zion_plugin_api_size=sizeof(zion_api_t);
static const zion_api_t *host;
static bool read_bytes(void *ctx,uint32_t offset,void *out,size_t count) {
    (void)ctx;return host->asset_read(1,offset,out,(uint32_t)count)==0;
}
static bool surface_write(void *ctx,uint32_t offset,const void *data,size_t count) {
    (void)ctx;return host->frame_write(offset,data,(uint32_t)count)==0;
}
static bool animation_read(void *ctx,uint32_t index,uint32_t generation,uint32_t offset,void *out,size_t count) {
    (void)index;(void)generation;return read_bytes(ctx,offset,out,count);
}
static uint32_t generation(void *ctx) {(void)ctx;return 1;}
static bool frame_read(void *ctx,uint32_t index,uint32_t version,const fga_header_t *header,
                       uint32_t offset,uint8_t *unused,fga_frame_t *frame) {
    (void)ctx;(void)index;(void)version;(void)unused;
    fga_reader_t reader={read_bytes,0};
    return fga_read_frame_to_sink(&reader,header,offset,surface_write,0,frame);
}
void zion_plugin_entry(const zion_api_t *api) {
    host=api;fga_reader_t reader={read_bytes,0};fga_header_t header;
    if(!fga_validate(&reader,api->asset_size(1),&header)) {api->log("GALLERY_FAIL format=FGA1");return;}
    api->log("GALLERY_VALIDATED format=FGA1 source=community-9");
    animation_player_t player;animation_player_init(&player,animation_read,frame_read,generation,0);
    /* Upstream scheduler forwards this token to our host-surface callback; it
     * does not dereference it. Pixel storage lives in host-owned 30 KiB tiles. */
    uint16_t surface_token=0;
    if(api->frame_begin()!=0) {api->log("GALLERY_FAIL stage=frame-memory");return;}
    bool ok=animation_player_open(&player,0,1,api->asset_size(1),(int64_t)api->uptime_ms()*1000,&surface_token);
    if(!ok) {api->frame_cancel();api->log("GALLERY_FAIL stage=open");return;}
    if(api->frame_present()!=0) {api->log("GALLERY_FAIL stage=present");return;}
    api->log("GALLERY_FRAME first=1 tiles=5");
    bool paused=false;
    for(;;) {
        zion_event_t event;
        if(api->event_wait(&event,20)==1 && event.type==ZION_EVENT_KEY && event.value==ZION_KEY_CLICK) {
            if(event.code==ZION_KEY_OK) {paused=!paused;api->log(paused?"GALLERY_PAUSED":"GALLERY_PLAYING");}
            if(event.code==ZION_KEY_UP || event.code==ZION_KEY_DOWN) {
                player.next_deadline_us=0;api->log("GALLERY_STEP");
            }
        }
        if(paused) continue;
        if(api->frame_begin()!=0) {api->log("GALLERY_FAIL stage=frame-lock");return;}
        bool redraw=false;
        ok=animation_player_tick(&player,(int64_t)api->uptime_ms()*1000,&surface_token,&redraw);
        if(!ok) {api->frame_cancel();api->log("GALLERY_FAIL stage=decode");return;}
        if(redraw) {(void)api->frame_present();api->log("GALLERY_FRAME delta=1");}
        else api->frame_cancel();
    }
}
