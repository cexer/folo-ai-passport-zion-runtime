#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/* Deliberately small RV32IM relocatable-object ABI, not a general Linux loader.
 * Code and data get separate mappings; the host supplies an executable alias
 * and a writable alias for the same code storage on ESP32-C3. */
typedef struct {
    void *(*allocate)(size_t size,bool executable);
    void *(*writable)(void *memory,bool executable);
    void (*release)(void *memory);
    uintptr_t (*resolve)(const char *name);
    /* Optional non-contiguous relocation staging for executable sections.
     * allocate(executable=true) still returns one contiguous execution range. */
    void *(*section_writable)(void *execution,size_t offset,size_t length);
    /* Optional streaming sink. Called once after that executable section's
     * relocations; may write/verify/release its staging before the next one. */
    bool (*section_complete)(void *execution,size_t offset,size_t length,void *staging);
    /* Optional storage for relocated, non-writable PROGBITS. Uses the same
     * streaming callbacks as code; the returned address must be data-readable. */
    void *(*allocate_readonly)(size_t size);
    /* Optional notification for validated global function definitions. The
     * receiver stages addresses only: activate after executable-cache commit.
     * No notification occurs on a rejected ELF. Names belong to input storage. */
    void (*resolved_export)(const char *name,uintptr_t address);
    /* Long trusted modules must not starve the scheduler during relocation. */
    void (*cooperate)(void);
    /* Optional streaming scratch reservation before temporary mapping pages
     * fragment remaining heap. Maxima include only sections actually staged. */
    bool (*prepare_staging)(size_t executable_max,size_t readonly_max);
    void (*release_staging)(void);
} zion_elf_ops_t;
typedef struct {
    void *code,*data,*readonly;
    void *data_blocks; /* fallback arenas; no application section crosses an arena */
    unsigned data_block_count;
    unsigned mapping_page_count; /* temporary table fallback; pages freed before entry */
    size_t code_bytes,data_bytes;
    size_t readonly_bytes;
    size_t mapped_bytes;
    uintptr_t entry;
    const char *error;
    unsigned relocation_type;
    uintptr_t constructors[64];
    unsigned constructor_count;
} zion_elf_t;
int zion_elf_load(const void *image,size_t bytes,const zion_elf_ops_t *ops,zion_elf_t *loaded);
/* Input must remain mapped until the module stops running. Only immutable
 * sections without relocation targets may borrow storage from this image. */
int zion_elf_load_mapped(const void *image,size_t bytes,const zion_elf_ops_t *ops,zion_elf_t *loaded);
void zion_elf_release(const zion_elf_ops_t *ops,zion_elf_t *loaded);
