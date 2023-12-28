#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "os_cfg.h" 

static boot_info_t * init_boot_info;   

void kernel_init(boot_info_t * boot_info){
    init_boot_info = boot_info;
    cpu_init();
    log_init();
    irq_init();
    time_init();
}


void init_main(void) {
    
    ASSERT(3<2);
    log_printf("Kernel is running...");
    log_printf("Version: %s",OS_VERSION);
    // irq_enable_global();
    int a = 1/0;
    for (;;) {}
}