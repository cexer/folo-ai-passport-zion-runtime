#pragma once
#include "nes6502.h"
#include <string.h>
typedef struct {uint32 hash,cycles,pc;unsigned irq,nmi,jammed;} cpu_result_t;
/* Three populated 4 KiB banks; other virtual banks use Nofrendo's dead page. */
static uint8 cpu_memory[3][4096];
static nes6502_context context;
static nes6502_memread reads[]={{0xffffffffu,0,0}};
static nes6502_memwrite writes[]={{0xffffffffu,0,0}};
/* Original synthetic program: ALU, branches, zero-page, stack, JSR/RTS and IRQ/NMI.
 * This is not a game ROM and contains no third-party cartridge content. */
static void workload_init(void) {
    static const uint8 code[]={0x78,0xd8,0xa2,0xff,0x9a,0x58,0xa9,0,0x85,0,0x85,1,0xa2,0,
        0xe6,0,0xd0,2,0xe6,1,0xe8,0x8a,0x48,0x20,0x50,0x80,0x68,0x49,0x5a,0x9d,0,2,0x4c,0x0e,0x80};
    static const uint8 sub[]={0xc8,0x98,0x18,0x65,0,0x60};
    memset(cpu_memory,0,sizeof(cpu_memory));memset(&context,0,sizeof(context));
    memcpy(cpu_memory[1],code,sizeof(code));memcpy(cpu_memory[1]+0x50,sub,sizeof(sub));
    cpu_memory[1][0x30]=0xe6;cpu_memory[1][0x31]=2;cpu_memory[1][0x32]=0x40;
    cpu_memory[1][0x40]=0xe6;cpu_memory[1][0x41]=3;cpu_memory[1][0x42]=0x40;
    cpu_memory[2][0xffc]=0;cpu_memory[2][0xffd]=0x80;cpu_memory[2][0xffe]=0x30;cpu_memory[2][0xfff]=0x80;
    cpu_memory[2][0xffa]=0x40;cpu_memory[2][0xffb]=0x80;
    context.mem_page[0]=cpu_memory[0];context.mem_page[8]=cpu_memory[1];context.mem_page[15]=cpu_memory[2];
    context.read_handler=reads;context.write_handler=writes;nes6502_setcontext(&context);nes6502_reset();
}
static void workload_slice(unsigned index,unsigned cycles) {
    if(index%7==0) nes6502_nmi();
    if(index%11==0) nes6502_irq();
    (void)nes6502_execute((int)cycles);
}
static cpu_result_t workload_result(void) {
    nes6502_getcontext(&context);uint32 hash=2166136261u;
    for(unsigned i=0;i<65536;i++) {uint8 *bank=context.mem_page[i/4096];hash=(hash^(bank?bank[i%4096]:0))*16777619u;}
    return (cpu_result_t){hash,nes6502_getcycles(false),context.pc_reg,cpu_memory[0][2],cpu_memory[0][3],context.jammed};
}
