#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"

static int i;
static int x=1;

void kernel_init(boot_info_t boot_info){
    cpu_init();
    irq_init();
}


void init_main(void){
    for(;;){}
}