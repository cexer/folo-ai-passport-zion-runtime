#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FGA_WIDTH 240u
#define FGA_HEIGHT 320u
#define FGA_FRAME_PIXELS (FGA_WIDTH * FGA_HEIGHT)
#define FGA_FRAME_BYTES (FGA_FRAME_PIXELS * 2u)
#define FGA_HEADER_SIZE 36u
#define FGA_MAX_FILE_SIZE (2560u * 1024u)
#define FGA_MAX_FRAMES 100u
#define FGA_MAX_DURATION_MS 10000u
#define FGA_MIN_DELAY_MS 100u

typedef enum {
    FGA_FRAME_FULL = 0,
    FGA_FRAME_DELTA = 1,
} fga_frame_kind_t;

typedef bool (*fga_read_fn)(void *ctx, uint32_t offset, void *out, size_t len);

typedef struct {
    fga_read_fn read;
    void *ctx;
} fga_reader_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t frame_count;
    uint16_t loop_count;
    uint32_t duration_ms;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t file_size;
    uint32_t first_frame_offset;
} fga_header_t;

typedef struct {
    fga_frame_kind_t kind;
    uint16_t delay_ms;
    uint32_t data_size;
    uint32_t next_offset;
} fga_frame_t;

/* CRC state starts at 0xffffffff and is xor'ed with 0xffffffff when complete. */
uint32_t fga_crc32_update(uint32_t state, const uint8_t *data, size_t len);

bool fga_read_header(fga_reader_t *reader, uint32_t file_size, fga_header_t *header);
bool fga_validate(fga_reader_t *reader, uint32_t file_size, fga_header_t *header);
bool fga_read_frame(fga_reader_t *reader, const fga_header_t *header,
                    uint32_t frame_offset, uint8_t *framebuffer,
                    size_t framebuffer_size, fga_frame_t *frame);
/* ZION adaptation: same parser, with a bounded streaming surface writer. */
typedef bool (*fga_write_fn)(void *ctx,uint32_t offset,const void *bytes,size_t size);
bool fga_read_frame_to_sink(fga_reader_t *reader,const fga_header_t *header,
    uint32_t offset,fga_write_fn write,void *ctx,fga_frame_t *frame);
