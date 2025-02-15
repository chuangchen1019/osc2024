#include "uart1.h"
#include "shell.h"
#include "memory.h"
#include "utils.h"
#include "dtb.h"
#include "exception.h"
#include "timer.h"
#include "sched.h"
#include "vfs.h"

char* dtb_ptr;
extern void load_context(void *curr_context);
extern thread_t *curr_thread;

void main(char* arg){
    // char input_buffer[CMD_MAX_LEN];

    dtb_ptr = arg;
    traverse_device_tree(dtb_ptr, dtb_callback_initramfs);

    uart_init();
    uart_sendline("\n");
    // uart_puts("loading dtb from: 0x%x\n", arg);
    irqtask_list_init();
    timer_list_init();
    uart_interrupt_enable();
    uart_flush_FIFO();

    init_allocator();
    init_thread_sched();
    init_rootfs();
    
    el1_interrupt_enable();  // enable interrupt in EL1 -> EL1
    load_context(&curr_thread->context);
}
