#include <stdio.h>
#include "workload.h"
int main(void) {
    workload_init();for(unsigned i=0;i<200;i++) workload_slice(i,4096);
    cpu_result_t r=workload_result();
    printf("hash=%lu cycles=%lu pc=%lu irq=%u nmi=%u jammed=%u\n",(unsigned long)r.hash,(unsigned long)r.cycles,
        (unsigned long)r.pc,r.irq,r.nmi,r.jammed);
    return r.jammed || !r.irq || !r.nmi;
}
