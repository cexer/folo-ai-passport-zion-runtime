#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Snapshot copied by the service; never expose worker-owned pointers to LVGL. */
typedef struct {
    char name[33];
    uint32_t package_bytes,install_bytes;
    uint32_t received_bytes,layout_bytes,bytes_per_second,elapsed_ms;
    uint32_t read_ms,write_ms;
    int read_result,read_errno,tls_error;
    unsigned phase;
    bool install_known;
    bool community_title;
    bool project_name;
    /* 0: none, 1: reclaimed, 2: cleanup pending, 3: valid install retained. */
    unsigned cleanup;
    int cleanup_error;
} zion_install_info_t;
const char *zion_install_reason(const char *detail);
uint32_t zion_install_color(unsigned stage);
void zion_install_format(char *out,size_t capacity,const zion_install_info_t *info,
                         unsigned stage,unsigned percent,const char *detail);
