#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#define ZION_PACKAGE_HEADER 256u
#define ZION_PACKAGE_SUBTYPE 0x41u
#define ZION_PACKAGE_SLOT_BASE (-100)
/* Passport OS persists cards in SPIFFS. Bit 5 makes old hosts reject this
 * requirement rather than silently allocating their default FAT partition. */
#define ZION_PACKAGE_FEATURE_SPIFFS 32u
typedef struct {
    uint32_t flags,total,id,abi,api_size,elf_offset,elf_size,asset_offset,asset_size,features;
    uint8_t sha256[32];
    char name[41],key[17];
    uint32_t flash_offset,extent;
    uint32_t storage_size,entry_stack,code_cache_size;
    uint8_t storage_subtype;
    char storage_label[17];
} zion_package_t;
bool zion_package_parse(const uint8_t header[ZION_PACKAGE_HEADER],zion_package_t *out);
bool zion_package_info(int slot,zion_package_t *out);
void zion_package_refresh(void);
unsigned zion_package_count(void);
int zion_package_slot(unsigned index);
int zion_package_slot_for_id(uint32_t id);
int zion_package_receive(uint32_t bytes);
int zion_package_remove(int slot);
bool zion_package_verify(const zion_package_t *package);
int zion_legacy_receive(uint32_t bytes,const uint8_t sha256[32]);
void zion_package_recover(void);

bool zion_package_extent(const zion_package_t *package,uint32_t flash_size,uint32_t *extent);
