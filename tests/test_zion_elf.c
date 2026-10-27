#include "zion_elf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned live, calls, fail_call;
static unsigned reject_once_call;
typedef struct fragment {struct fragment *next;size_t offset,size;} fragment_t;
static fragment_t *fragments;
static void *fragment_execution;
static void *readonly_execution;
static bool fragmented;
static unsigned fragment_count,fragment_fail_after=UINT_MAX;
static size_t largest_code,largest_data;
static size_t nonexec_limit=SIZE_MAX,stage_live,stage_peak;
static unsigned committed,commit_fail_after=UINT_MAX;
static bool readonly_commit_fail;
static void *reserved_buffer;
static size_t reserved_capacity,reserved_peak,prepared_maximum;
static const char *contract_symbol;
static size_t last_nonexec_request,reject_once_size;
static unsigned export_notifications;
static bool entry_notified;
static void exported(const char *name,uintptr_t address) {
    assert(address);export_notifications++;
    if(!strcmp(name,"zion_plugin_entry")) entry_notified=true;
}
static void *allocate(size_t n,bool executable) {
    calls++;if(fail_call && calls>=fail_call) return NULL;
    if(reject_once_call && calls==reject_once_call) {reject_once_call=0;return NULL;}
    if(!executable) {
        last_nonexec_request=n;
        if(reject_once_size && n==reject_once_size) {reject_once_size=0;return NULL;}
    }
    if(!executable && n>nonexec_limit) return NULL;
    size_t *largest=executable?&largest_code:&largest_data;if(n>*largest) *largest=n;
    void *p=calloc(1,n);if(p) live++;if(fragmented && executable) fragment_execution=p;return p;
}
static void *writable(void *p,bool executable) {(void)executable;return p;}
static void *allocate_readonly(size_t n) {readonly_execution=allocate(n,false);return readonly_execution;}
static void release(void *p) {
    if(p==readonly_execution) readonly_execution=NULL;
    if(p && p==fragment_execution) {
        while(fragments) {fragment_t *next=fragments->next;stage_live-=fragments->size;free(fragments);fragments=next;}
        fragment_execution=NULL;
    }
    if(p) {assert(live);live--;free(p);}
}
static void *fragment_write(void *execution,size_t offset,size_t size) {
    assert(execution==fragment_execution || execution==readonly_execution);
    if(fragment_count==fragment_fail_after) return NULL;
    fragment_t *part=malloc(sizeof(*part)+size);assert(part);
    *part=(fragment_t){fragments,offset,size};fragments=part;fragment_count++;
    stage_live+=size;if(stage_live>stage_peak) stage_peak=stage_live;return part+1;
}
static bool fragment_commit(void *execution,size_t offset,size_t size,void *staging) {
    assert((execution==fragment_execution || execution==readonly_execution) && fragments && staging==fragments+1);
    assert(fragments->offset==offset && fragments->size==size);
    if(committed==commit_fail_after || (readonly_commit_fail && execution==readonly_execution)) return false;
    memcpy((uint8_t *)execution+offset,staging,size);
    fragment_t *next=fragments->next;stage_live-=size;free(fragments);fragments=next;
    committed++;return true;
}
static bool reserve_buffer(size_t code_max,size_t readonly_max) {
    assert(!reserved_buffer);prepared_maximum=code_max>readonly_max?code_max:readonly_max;
    assert(prepared_maximum);reserved_peak=0;reserved_capacity=prepared_maximum;
    reserved_buffer=allocate(reserved_capacity,false);return reserved_buffer!=NULL;
}
static void release_buffer(void) {
    release(reserved_buffer);reserved_buffer=NULL;reserved_capacity=0;
}
static void *reserved_write(void *execution,size_t offset,size_t size) {
    (void)execution;(void)offset;assert(reserved_buffer&&size<=reserved_capacity);
    if(size>reserved_peak) reserved_peak=size;
    return reserved_buffer;
}
static bool reserved_commit(void *execution,size_t offset,size_t size,void *staging) {
    assert(staging==reserved_buffer&&size<=reserved_capacity);
    memcpy((uint8_t *)execution+offset,staging,size);return true;
}
static uintptr_t resolve(const char *name) {
    if(contract_symbol) {
        if(!strcmp(name,contract_symbol)) return 0x10000000u;
#define ZION_IMPORT(symbol,target) if(!strcmp(name,#symbol)) return 0x10000000u;
#include "zion_idf_exports.inc"
#undef ZION_IMPORT
#define ZION_AUTO_IMPORT(index,symbol) if(!strcmp(name,symbol)) return 0x10000000u;
#include "zion_idf_auto_imports.inc"
#undef ZION_AUTO_IMPORT
    }
    if(!strcmp(name,"memcpy") || !strcmp(name,"memcmp") || !strcmp(name,"memset") || !strcmp(name,"strlen") || !strcmp(name,"__divdi3") ||
       !strcmp(name,"__udivdi3") || !strcmp(name,"__umoddi3")) return 0x10000000u;
    return 0;
}
static void write32(unsigned char *p,unsigned v) {for(unsigned i=0;i<4;i++) p[i]=(unsigned char)(v>>(8*i));}
static unsigned read32(const unsigned char *p) {return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static void test_actual_allocation(void) {
    unsigned char fixture[176]={0};memcpy(fixture,"\177ELF\1\1\1",7);
    fixture[16]=1;fixture[18]=243;fixture[20]=1;fixture[40]=52;fixture[46]=40;fixture[48]=3;
    write32(fixture+32,56);
    /* NOBITS code/data exercise allocation without requiring a large input file. */
    write32(fixture+96+4,8);write32(fixture+96+8,6);write32(fixture+96+20,70000);write32(fixture+96+32,4);
    write32(fixture+136+4,8);write32(fixture+136+8,2);write32(fixture+136+20,300000);write32(fixture+136+32,4);
    const zion_elf_ops_t ops={allocate,writable,release,resolve,NULL,NULL,NULL,exported,NULL,NULL,NULL};zion_elf_t image;
    calls=0;largest_code=largest_data=0;
    assert(zion_elf_load(fixture,sizeof(fixture),&ops,&image)!=0);
    assert(!strcmp(image.error,"elf:entry-or-ABI") && !live);
    assert(!export_notifications);
    assert(largest_code==70000 && largest_data>=300000);
    for(unsigned failure=1;failure<=4;failure++) {
        calls=0;fail_call=failure;
        assert(zion_elf_load(fixture,sizeof(fixture),&ops,&image)!=0 && !live);
        assert(!strcmp(image.error,failure==4?"elf:section-memory":"elf:out-of-memory"));
    }
    fail_call=0;calls=0;
}
int main(int argc,char **argv) {
    test_actual_allocation();
    assert(argc==2 || argc==3);contract_symbol=argc==3?argv[2]:NULL;FILE *file=fopen(argv[1],"rb");assert(file);
    assert(fseek(file,0,SEEK_END)==0);long size=ftell(file);assert(size>52);rewind(file);
    unsigned char *bytes=malloc((size_t)size);assert(bytes && fread(bytes,1,(size_t)size,file)==(size_t)size);fclose(file);
    const zion_elf_ops_t ops={allocate,writable,release,resolve,NULL,NULL,NULL,exported,NULL,NULL,NULL};zion_elf_t image;
    int load_result=zion_elf_load(bytes,(size_t)size,&ops,&image);
    if(load_result) fprintf(stderr,"ELF load rejected: stage=%s relocation=%u\n",image.error?image.error:"unknown",image.relocation_type);
    assert(load_result==0);
    assert(image.entry && image.code_bytes>0 && image.data_bytes>0 && !image.error);
    assert(export_notifications && entry_notified);
    printf("ELF object: code=%u data=%u\n",(unsigned)image.code_bytes,(unsigned)image.data_bytes);
    assert(live==2);zion_elf_release(&ops,&image);assert(!live);
    unsigned char *before=malloc((size_t)size);assert(before);memcpy(before,bytes,(size_t)size);
    assert(zion_elf_load_mapped(bytes,(size_t)size,&ops,&image)==0);
    size_t flat_table_request=last_nonexec_request;
    bool has_mapped_data=image.data_bytes!=0;
    assert(!memcmp(before,bytes,(size_t)size)); /* relocation must not write Flash */
    printf("ELF mapped: code=%u data=%u borrowed=%u\n",(unsigned)image.code_bytes,
           (unsigned)image.data_bytes,(unsigned)image.mapped_bytes);
    zion_elf_release(&ops,&image);assert(!live);free(before);
    /* Force only the whole-state allocation to fail. The real object's RW
     * sections must still relocate through packed arenas, including large
     * indivisible arrays. Every later allocation failure must roll back. */
    if(has_mapped_data) {
    calls=0;reject_once_call=2;
    assert(zion_elf_load_mapped(bytes,(size_t)size,&ops,&image)==0);
    assert(!reject_once_call && image.data_blocks && !image.data && image.data_block_count);
    unsigned arena_allocations=calls;
    printf("ELF packed state: arenas=%u bytes=%u\n",image.data_block_count,(unsigned)image.data_bytes);
    zion_elf_release(&ops,&image);assert(!live);
    for(unsigned failure=1;failure<=arena_allocations;failure++) {
        calls=0;reject_once_call=2;fail_call=failure;
        assert(zion_elf_load_mapped(bytes,(size_t)size,&ops,&image)!=0 && !live);
    }
    fail_call=0;reject_once_call=0;
    }
    /* Simulate exactly the temporary table allocation failure, independently
     * of an app's own indivisible object sizes. Preserve every relocation and
     * ABI check, then inject failure at every partial page-table allocation. */
    calls=0;reject_once_size=flat_table_request;
    assert(zion_elf_load_mapped(bytes,(size_t)size,&ops,&image)==0);
    assert(!reject_once_size && image.mapping_page_count>0);
    unsigned paged_allocations=calls;
    printf("ELF mapping fallback: pages=%u rejected_flat_bytes=%u\n",image.mapping_page_count,(unsigned)flat_table_request);
    zion_elf_release(&ops,&image);assert(!live);
    for(unsigned failure=1;failure<=paged_allocations;failure++) {
        calls=0;reject_once_size=flat_table_request;fail_call=failure;
        assert(zion_elf_load_mapped(bytes,(size_t)size,&ops,&image)!=0 && !live);
    }
    fail_call=0;reject_once_size=0;
    fragmented=true;fragment_count=0;fragment_fail_after=UINT_MAX;
    const zion_elf_ops_t fragmented_ops={allocate,writable,release,resolve,fragment_write,NULL,NULL,NULL,NULL,NULL,NULL};
    assert(zion_elf_load_mapped(bytes,(size_t)size,&fragmented_ops,&image)==0 && fragment_count);
    printf("ELF fragmented staging: %u independently allocated sections\n",fragment_count);
    zion_elf_release(&fragmented_ops,&image);assert(!live && !fragments);
    const zion_elf_ops_t streaming_ops={allocate,writable,release,resolve,fragment_write,fragment_commit,NULL,NULL,NULL,NULL,NULL};
    fragment_count=committed=0;stage_peak=0;calls=0;
    /* The real C3 had a 45,056-byte largest block. A metadata-heavy module
     * must not copy the whole section table into one larger allocation. */
    if(contract_symbol) nonexec_limit=45056;
    bool oversized_object=false;
    unsigned table=read32(bytes+32),sections=(unsigned)bytes[48]|(unsigned)bytes[49]<<8;
    for(unsigned i=0;i<sections;i++) {
        const unsigned char *header=bytes+table+i*40;
        if((read32(header+8)&7u)==3u && read32(header+20)>nonexec_limit) oversized_object=true;
    }
    int streaming_result=zion_elf_load_mapped(bytes,(size_t)size,&streaming_ops,&image);
    if(oversized_object) {
        /* Passport OS has an indivisible 48 KiB audio workspace. An allocator
         * capped at 44 KiB must reject it cleanly; this is not a file-size cap.
         * Run the remaining streaming/relocation checks without that synthetic
         * cap, and keep physical acceptance separate. */
        assert(streaming_result!=0 && !live && !fragments && !stage_live);
        assert(!strcmp(image.error,"elf:out-of-memory"));
        puts("ELF bounded allocator: expected OOM for indivisible original object");
        nonexec_limit=SIZE_MAX;fragment_count=committed=0;calls=0;stage_peak=0;
        streaming_result=zion_elf_load_mapped(bytes,(size_t)size,&streaming_ops,&image);
    }
    assert(streaming_result==0);
    assert(committed==fragment_count && !fragments && !stage_live && stage_peak<image.code_bytes);
    printf("ELF streaming: sections=%u peak_code_stage=%u total_code=%u\n",committed,(unsigned)stage_peak,(unsigned)image.code_bytes);
    if(image.data_blocks) {
        assert(!image.data && image.data_block_count>1);
        unsigned allocations=calls;
        printf("ELF writable fallback: blocks=%u bytes=%u\n",image.data_block_count,(unsigned)image.data_bytes);
        zion_elf_release(&streaming_ops,&image);assert(!live);
        // Exhaustion after any partial fallback must release every prior block.
        for(unsigned failure=1;failure<=allocations;failure++) {
            calls=0;fail_call=failure;fragment_count=committed=0;stage_peak=0;
            assert(zion_elf_load_mapped(bytes,(size_t)size,&streaming_ops,&image)!=0);
            assert(!live && !fragments && !stage_live);
        }
        fail_call=0;
    }
    zion_elf_release(&streaming_ops,&image);assert(!live);nonexec_limit=SIZE_MAX;
    const zion_elf_ops_t cached_ops={allocate,writable,release,resolve,fragment_write,fragment_commit,allocate_readonly,NULL,NULL,NULL,NULL};
    fragment_count=committed=0;
    assert(zion_elf_load_mapped(bytes,(size_t)size,&cached_ops,&image)==0);
    assert(committed==fragment_count && !fragments && !stage_live);
    assert(image.readonly_bytes==0 || image.readonly==readonly_execution);
    printf("ELF readonly cache: RAM=%u cached=%u borrowed=%u\n",(unsigned)image.data_bytes,
        (unsigned)image.readonly_bytes,(unsigned)image.mapped_bytes);
    bool has_readonly=image.readonly_bytes!=0;
    zion_elf_release(&cached_ops,&image);assert(!live && !readonly_execution);
    const zion_elf_ops_t reserved_ops={allocate,writable,release,resolve,reserved_write,reserved_commit,allocate_readonly,NULL,NULL,reserve_buffer,release_buffer};
    calls=0;assert(zion_elf_load_mapped(bytes,(size_t)size,&reserved_ops,&image)==0);
    assert(!reserved_buffer && reserved_peak==prepared_maximum);
    unsigned reserved_allocations=calls;
    printf("ELF peak scratch reserved before metadata: bytes=%u\n",(unsigned)prepared_maximum);
    zion_elf_release(&reserved_ops,&image);assert(!live && !readonly_execution);
    for(unsigned failure=1;failure<=reserved_allocations;failure++) {
        calls=0;fail_call=failure;
        assert(zion_elf_load_mapped(bytes,(size_t)size,&reserved_ops,&image)!=0);
        assert(!live && !reserved_buffer && !readonly_execution);
    }
    fail_call=0;
    if(has_readonly) {
        fragment_count=committed=0;readonly_commit_fail=true;
        assert(zion_elf_load_mapped(bytes,(size_t)size,&cached_ops,&image)!=0 && !live && !stage_live);
        assert(!strcmp(image.error,"elf:code-commit"));readonly_commit_fail=false;
    }
    fragment_count=committed=0;commit_fail_after=0;
    assert(zion_elf_load_mapped(bytes,(size_t)size,&cached_ops,&image)!=0 && !live && !stage_live);
    commit_fail_after=UINT_MAX;
    fragment_count=committed=0;commit_fail_after=1;
    assert(zion_elf_load_mapped(bytes,(size_t)size,&streaming_ops,&image)!=0 && !live && !fragments && !stage_live);
    assert(!strcmp(image.error,"elf:code-commit"));commit_fail_after=UINT_MAX;
    fragment_count=0;fragment_fail_after=0;
    assert(zion_elf_load_mapped(bytes,(size_t)size,&fragmented_ops,&image)!=0 && !live && !fragments);
    assert(!strcmp(image.error,"elf:out-of-memory"));fragmented=false;
    if(contract_symbol) {
        const char *saved_contract=contract_symbol;contract_symbol="zion_idf_contract_wrong";
        assert(zion_elf_load(bytes,(size_t)size,&ops,&image)!=0 && !live);
        contract_symbol=saved_contract;
    }
    assert(zion_elf_load(bytes,51,&ops,&image)!=0 && !live);
    bytes[0]=0;assert(zion_elf_load(bytes,(size_t)size,&ops,&image)!=0 && !live);bytes[0]=127;
    unsigned char saved=bytes[18];bytes[18]=0;
    assert(zion_elf_load(bytes,(size_t)size,&ops,&image)!=0 && !live);bytes[18]=saved;
    /* Corrupt section-header offset: the parser must reject without reading it. */
    unsigned char old[4];memcpy(old,bytes+32,4);memset(bytes+32,255,4);
    assert(zion_elf_load(bytes,(size_t)size,&ops,&image)!=0 && !live);memcpy(bytes+32,old,4);
    /* Strip the required ABI export without changing any offsets. No global
     * definitions may escape a rejected image into the SDK dispatch layer. */
    export_notifications=0;
    bool replaced=false;
    for(long i=0;i+15<size;i++) if(!memcmp(bytes+i,"zion_plugin_abi",15)) {bytes[i]='X';replaced=true;break;}
    assert(replaced);assert(zion_elf_load(bytes,(size_t)size,&ops,&image)!=0 && !live);
    assert(!export_notifications);
    assert(!strcmp(image.error,"elf:entry-or-ABI"));free(bytes);
    puts("ELF loader: PASS (real object relocation, ABI, malformed ranges, allocation cleanup)");
}
