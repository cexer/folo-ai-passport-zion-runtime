#include "animation_player.h"

#include <string.h>

typedef struct {
    animation_player_t *player;
} player_reader_t;

static bool player_read(void *ctx, uint32_t offset, void *out, size_t len)
{
    player_reader_t *reader = ctx;
    animation_player_t *player = reader->player;
    return player->read(player->read_ctx, player->media_index,
                        player->generation_at_open, offset, out, len);
}

void animation_player_init(animation_player_t *player, animation_read_fn read,
                           animation_read_frame_fn read_frame,
                           animation_generation_fn generation, void *read_ctx)
{
    if (!player) return;
    memset(player, 0, sizeof(*player));
    player->read = read;
    player->read_frame = read_frame;
    player->generation = generation;
    player->read_ctx = read_ctx;
}

void animation_player_stop(animation_player_t *player)
{
    if (!player) return;
    animation_read_fn read = player->read;
    animation_read_frame_fn read_frame = player->read_frame;
    animation_generation_fn generation = player->generation;
    void *read_ctx = player->read_ctx;
    memset(player, 0, sizeof(*player));
    player->read = read;
    player->read_frame = read_frame;
    player->generation = generation;
    player->read_ctx = read_ctx;
}

bool animation_player_open(animation_player_t *player, uint32_t media_index,
                           uint32_t generation, uint32_t file_size,
                           int64_t now_us, uint16_t *framebuffer)
{
    if (!player || !player->read || !player->read_frame || !player->generation || !framebuffer ||
        player->generation(player->read_ctx) != generation) return false;
    animation_player_stop(player);
    player->media_index = media_index;
    player->generation_at_open = generation;

    player_reader_t context = { .player = player };
    fga_reader_t reader = { .read = player_read, .ctx = &context };
    if (!fga_read_header(&reader, file_size, &player->header)) {
        animation_player_stop(player);
        return false;
    }

    fga_frame_t first;
    if (!player->read_frame(player->read_ctx, player->media_index,
                            player->generation_at_open, &player->header,
                            player->header.first_frame_offset,
                            (uint8_t *)framebuffer, &first)) {
        animation_player_stop(player);
        return false;
    }
    player->active = true;
    player->next_offset = first.next_offset;
    player->next_frame = 1;
    player->next_deadline_us = now_us + (int64_t)first.delay_ms * 1000;
    return true;
}

bool animation_player_tick(animation_player_t *player, int64_t now_us,
                           uint16_t *framebuffer, bool *redraw)
{
    if (redraw) *redraw = false;
    if (!player || !framebuffer || !redraw) return false;
    if (!player->active) return true;
    if (player->generation(player->read_ctx) != player->generation_at_open) {
        animation_player_stop(player);
        return false;
    }
    if (now_us < player->next_deadline_us) return true;

    if (player->next_frame >= player->header.frame_count) {
        player->completed_loops++;
        if (player->header.loop_count != 0 &&
            player->completed_loops >= player->header.loop_count) {
            animation_player_stop(player);
            return true;
        }
        player->next_frame = 0;
        player->next_offset = player->header.first_frame_offset;
    }

    fga_frame_t frame;
    if (!player->read_frame(player->read_ctx, player->media_index,
                            player->generation_at_open, &player->header,
                            player->next_offset, (uint8_t *)framebuffer, &frame)) {
        animation_player_stop(player);
        return false;
    }
    player->next_offset = frame.next_offset;
    player->next_frame++;
    player->next_deadline_us = now_us + (int64_t)frame.delay_ms * 1000;
    *redraw = true;
    return true;
}

uint32_t animation_player_wait_ms(const animation_player_t *player,
                                  int64_t now_us, uint32_t maximum_ms)
{
    if (!player || !player->active || maximum_ms == 0) return maximum_ms;
    int64_t remaining_us = player->next_deadline_us - now_us;
    uint32_t wait_ms = remaining_us <= 0 ? 10u : (uint32_t)((remaining_us + 999) / 1000);
    if (wait_ms < 10u) wait_ms = 10u;
    if (wait_ms > maximum_ms) wait_ms = maximum_ms;
    return wait_ms;
}

bool animation_player_active(const animation_player_t *player)
{
    return player && player->active;
}
