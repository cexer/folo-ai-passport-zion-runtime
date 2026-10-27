#include "fga_format.h"

#include <string.h>

#define FGA_FRAME_RECORD_SIZE 8u
#define FGA_DELTA_RECORD_SIZE 6u
#define FGA_CRC_CHUNK_SIZE 512u
#define FGA_DELTA_CHUNK_SIZE 512u

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool range_valid(uint32_t offset, uint32_t len, uint32_t end)
{
    return offset <= end && len <= end - offset;
}

uint32_t fga_crc32_update(uint32_t state, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        state ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            state = (state >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(state & 1u));
    }
    return state;
}

bool fga_read_header(fga_reader_t *reader, uint32_t file_size, fga_header_t *header)
{
    uint8_t raw[FGA_HEADER_SIZE];
    if (!reader || !reader->read || !header) return false;
    if (file_size < FGA_HEADER_SIZE + FGA_FRAME_RECORD_SIZE + FGA_FRAME_BYTES ||
        file_size > FGA_MAX_FILE_SIZE) return false;
    if (!reader->read(reader->ctx, 0, raw, sizeof(raw))) return false;
    if (memcmp(raw, "FGA1", 4) != 0) return false;
    if (get_u16(raw + 4) != 1 || get_u16(raw + 6) != FGA_HEADER_SIZE) return false;
    if (get_u16(raw + 8) != FGA_WIDTH || get_u16(raw + 10) != FGA_HEIGHT) return false;
    for (size_t i = 28; i < sizeof(raw); i++)
        if (raw[i] != 0) return false;

    header->width = get_u16(raw + 8);
    header->height = get_u16(raw + 10);
    header->frame_count = get_u16(raw + 12);
    header->loop_count = get_u16(raw + 14);
    header->duration_ms = get_u32(raw + 16);
    header->payload_size = get_u32(raw + 20);
    header->payload_crc32 = get_u32(raw + 24);
    header->file_size = file_size;
    header->first_frame_offset = FGA_HEADER_SIZE;

    if (header->frame_count == 0 || header->frame_count > FGA_MAX_FRAMES) return false;
    if (header->duration_ms == 0 || header->duration_ms > FGA_MAX_DURATION_MS) return false;
    return header->payload_size == file_size - FGA_HEADER_SIZE;
}

static bool read_frame_record(fga_reader_t *reader, const fga_header_t *header,
                              uint32_t frame_offset, fga_frame_t *frame,
                              uint32_t *data_offset)
{
    uint8_t raw[FGA_FRAME_RECORD_SIZE];
    if (!range_valid(frame_offset, sizeof(raw), header->file_size) ||
        !reader->read(reader->ctx, frame_offset, raw, sizeof(raw))) return false;
    if (raw[1] != 0 || raw[0] > FGA_FRAME_DELTA) return false;

    frame->kind = (fga_frame_kind_t)raw[0];
    frame->delay_ms = get_u16(raw + 2);
    frame->data_size = get_u32(raw + 4);
    *data_offset = frame_offset + FGA_FRAME_RECORD_SIZE;
    if (frame->delay_ms < FGA_MIN_DELAY_MS || frame->delay_ms > FGA_MAX_DURATION_MS)
        return false;
    if (!range_valid(*data_offset, frame->data_size, header->file_size)) return false;
    frame->next_offset = *data_offset + frame->data_size;
    return true;
}

static bool process_delta(fga_reader_t *reader, uint32_t data_offset, uint32_t data_size,
                          uint8_t *framebuffer,fga_write_fn write,void *write_ctx)
{
    typedef struct {
        fga_reader_t *reader;
        uint32_t offset;
        uint32_t remaining;
        size_t position;
        size_t length;
        uint8_t bytes[FGA_DELTA_CHUNK_SIZE];
    } delta_stream_t;

    delta_stream_t stream = {
        .reader = reader,
        .offset = data_offset,
        .remaining = data_size,
    };
    uint32_t cursor = 0;
    while (stream.remaining > 0 || stream.position < stream.length) {
        uint8_t command[FGA_DELTA_RECORD_SIZE];
        size_t copied = 0;
        while (copied < sizeof(command)) {
            if (stream.position == stream.length) {
                stream.length = stream.remaining < sizeof(stream.bytes)
                    ? stream.remaining : sizeof(stream.bytes);
                if (stream.length == 0 ||
                    !reader->read(reader->ctx, stream.offset, stream.bytes, stream.length))
                    return false;
                stream.offset += (uint32_t)stream.length;
                stream.remaining -= (uint32_t)stream.length;
                stream.position = 0;
            }
            size_t available = stream.length - stream.position;
            size_t needed = sizeof(command) - copied;
            size_t amount = available < needed ? available : needed;
            memcpy(command + copied, stream.bytes + stream.position, amount);
            stream.position += amount;
            copied += amount;
        }

        uint32_t skip = get_u32(command);
        uint32_t run = get_u16(command + 4);
        if (skip > FGA_FRAME_PIXELS - cursor) return false;
        cursor += skip;
        if (run == 0 || run > FGA_FRAME_PIXELS - cursor) return false;

        uint32_t bytes = run * 2u;
        if (bytes > stream.remaining + stream.length - stream.position) return false;
        uint32_t copied_bytes = 0;
        while (copied_bytes < bytes) {
            if (stream.position == stream.length) {
                stream.length = stream.remaining < sizeof(stream.bytes)
                    ? stream.remaining : sizeof(stream.bytes);
                if (stream.length == 0 ||
                    !reader->read(reader->ctx, stream.offset, stream.bytes, stream.length))
                    return false;
                stream.offset += (uint32_t)stream.length;
                stream.remaining -= (uint32_t)stream.length;
                stream.position = 0;
            }
            size_t available = stream.length - stream.position;
            uint32_t needed = bytes - copied_bytes;
            size_t amount = available < needed ? available : needed;
            if (framebuffer)
                memcpy(framebuffer + cursor * 2u + copied_bytes,
                       stream.bytes + stream.position, amount);
            if(write && !write(write_ctx,cursor*2u+copied_bytes,stream.bytes+stream.position,amount)) return false;
            stream.position += amount;
            copied_bytes += (uint32_t)amount;
        }
        cursor += run;
    }
    return stream.remaining == 0 && stream.position == stream.length;
}

static bool process_frame(fga_reader_t *reader, const fga_header_t *header,
                          uint32_t frame_offset, uint16_t frame_index,
                          uint8_t *framebuffer, fga_frame_t *frame,fga_write_fn write,void *write_ctx)
{
    uint32_t data_offset;
    if (!read_frame_record(reader, header, frame_offset, frame, &data_offset)) return false;
    if (frame_index == 0) {
        if (frame->kind != FGA_FRAME_FULL || frame->data_size != FGA_FRAME_BYTES)
            return false;
    } else if (frame->kind != FGA_FRAME_DELTA) {
        return false;
    }

    if (frame->kind == FGA_FRAME_FULL) {
        if (framebuffer && !reader->read(reader->ctx, data_offset,
                                         framebuffer, FGA_FRAME_BYTES))
            return false;
        if(write) {
            uint8_t chunk[512];
            for(uint32_t pos=0;pos<FGA_FRAME_BYTES;) {
                uint32_t n=FGA_FRAME_BYTES-pos;if(n>sizeof(chunk)) n=sizeof(chunk);
                if(!reader->read(reader->ctx,data_offset+pos,chunk,n) || !write(write_ctx,pos,chunk,n)) return false;
                pos+=n;
            }
        }
        return true;
    }
    return process_delta(reader, data_offset, frame->data_size, framebuffer,write,write_ctx);
}

bool fga_validate(fga_reader_t *reader, uint32_t file_size, fga_header_t *header_out)
{
    fga_header_t header;
    if (!fga_read_header(reader, file_size, &header)) return false;

    uint8_t chunk[FGA_CRC_CHUNK_SIZE];
    uint32_t state = 0xffffffffu;
    uint32_t offset = header.first_frame_offset;
    while (offset < file_size) {
        uint32_t len = file_size - offset;
        if (len > sizeof(chunk)) len = sizeof(chunk);
        if (!reader->read(reader->ctx, offset, chunk, len)) return false;
        state = fga_crc32_update(state, chunk, len);
        offset += len;
    }
    if ((state ^ 0xffffffffu) != header.payload_crc32) return false;

    offset = header.first_frame_offset;
    uint32_t duration = 0;
    for (uint16_t i = 0; i < header.frame_count; i++) {
        fga_frame_t frame;
        if (!process_frame(reader, &header, offset, i, NULL, &frame,NULL,NULL)) return false;
        if (duration > FGA_MAX_DURATION_MS - frame.delay_ms) return false;
        duration += frame.delay_ms;
        offset = frame.next_offset;
    }
    if (offset != file_size || duration != header.duration_ms) return false;
    if (header_out) *header_out = header;
    return true;
}

bool fga_read_frame(fga_reader_t *reader, const fga_header_t *header,
                    uint32_t frame_offset, uint8_t *framebuffer,
                    size_t framebuffer_size, fga_frame_t *frame)
{
    if (!reader || !reader->read || !header || !frame || !framebuffer ||
        framebuffer_size < FGA_FRAME_BYTES) return false;
    uint16_t frame_index = frame_offset == header->first_frame_offset ? 0 : 1;
    return process_frame(reader, header, frame_offset, frame_index, framebuffer, frame,NULL,NULL);
}
bool fga_read_frame_to_sink(fga_reader_t *reader,const fga_header_t *header,
    uint32_t offset,fga_write_fn write,void *ctx,fga_frame_t *frame) {
    if(!reader || !reader->read || !header || !write || !frame) return false;
    return process_frame(reader,header,offset,offset==header->first_frame_offset?0:1,NULL,frame,write,ctx);
}
