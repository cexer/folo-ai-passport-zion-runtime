#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fga_format.h"

typedef bool (*animation_read_fn)(void *ctx, uint32_t media_index,
                                  uint32_t expected_generation, uint32_t offset,
                                  void *out, size_t len);
typedef bool (*animation_read_frame_fn)(void *ctx, uint32_t media_index,
                                        uint32_t expected_generation,
                                        const fga_header_t *header, uint32_t offset,
                                        uint8_t *framebuffer, fga_frame_t *frame);
typedef uint32_t (*animation_generation_fn)(void *ctx);

typedef struct {
    bool active;
    uint32_t media_index;
    uint32_t generation_at_open;
    uint32_t next_offset;
    uint16_t next_frame;
    uint16_t completed_loops;
    int64_t next_deadline_us;
    fga_header_t header;
    animation_read_fn read;
    animation_read_frame_fn read_frame;
    animation_generation_fn generation;
    void *read_ctx;
} animation_player_t;

void animation_player_init(animation_player_t *player, animation_read_fn read,
                           animation_read_frame_fn read_frame,
                           animation_generation_fn generation, void *read_ctx);
void animation_player_stop(animation_player_t *player);
bool animation_player_open(animation_player_t *player, uint32_t media_index,
                           uint32_t generation, uint32_t file_size,
                           int64_t now_us, uint16_t *framebuffer);
bool animation_player_tick(animation_player_t *player, int64_t now_us,
                           uint16_t *framebuffer, bool *redraw);
uint32_t animation_player_wait_ms(const animation_player_t *player,
                                  int64_t now_us, uint32_t maximum_ms);
bool animation_player_active(const animation_player_t *player);
