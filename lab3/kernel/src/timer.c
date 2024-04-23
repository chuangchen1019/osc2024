#include "timer.h"
#include "mini_uart.h"
#include "utils.h"

#define XSTR(s) STR(s)
#define STR(s) #s

void core_timer_enable() {
    __asm__ __volatile__(
        "mov x1, 1\n\t"
        "msr cntp_ctl_el0, x1\n\t" // cntp_ctl_el0[0]: enable, Control register for the EL1 physical timer.
                                   // cntp_tval_el0: Holds the timer value for the EL1 physical timer
        "mov x2, 2\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t"
        "str w2, [x1]\n\t"         // QA7_rev3.4.pdf: Core0 Timer IRQ allows Non-secure physical timer(nCNTPNSIRQ)
    );
}

void core_timer_disable() {
    __asm__ __volatile__(
        "mov x2, 0\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t"
        "str w2, [x1]\n\t"         // QA7_rev3.4.pdf: Mask all timer interrupt
    );
}

// set timer interrupt time to [expired_time] seconds after now (relatively)
void set_timer_interrupt(unsigned long long seconds) {
    unsigned long long freq;
    unsigned long long ticks;

    __asm__ __volatile__(
        "mrs %0, cntfrq_el0\n\t" : "=r"(freq)
    );
    ticks = freq * seconds;
    __asm__ __volatile__(
        "msr cntp_tval_el0, %0\n\t" : "=r"(ticks)
    );
}

// directly set timer interrupt time to a cpu tick  (directly)
void set_timer_interrupt_by_tick(unsigned long long tick) {
    __asm__ __volatile__(
        "msr cntp_cval_el0, %0\n\t"  //cntp_cval_el0 -> absolute timer
        :"=r" (tick)
    );
}

// get cpu tick add some second
unsigned long long get_cpu_tick_plus_s(unsigned long long seconds){
    unsigned long long cntpct_el0=0;
    __asm__ __volatile__("mrs %0, cntpct_el0\n\t": "=r"(cntpct_el0)); // tick auchor
    unsigned long long cntfrq_el0=0;
    __asm__ __volatile__("mrs %0, cntfrq_el0\n\t": "=r"(cntfrq_el0)); // tick frequency
    return (cntpct_el0 + cntfrq_el0*seconds);
}


void set_alert_2S(char* str) {
    unsigned long long cntpct_el0;
    unsigned long long cntfrq_el0;
    __asm__ __volatile__("mrs %0, cntpct_el0\n\t": "=r"(cntpct_el0)); // tick anchor
    __asm__ __volatile__("mrs %0, cntfrq_el0\n\t": "=r"(cntfrq_el0)); // tick frequency
    
    uart_puts("\r\n[Start Alert]\r\n");
    
    // for(int i=0;i<1000000000;i++){} 
    
    char buf[VSPRINT_MAX_BUF_SIZE];
    sprintf(buf, "[Interrupt][el1_irq][%s] %d seconds after booting\n", str, cntpct_el0/cntfrq_el0);
    uart_puts(buf);

    unsigned long long interrupt_time = get_cpu_tick_plus_s(2);
    set_timer_interrupt_by_tick(interrupt_time);
    // Add timer
}

void timer_interrupt_handler() {
    // // 這裡插入你希望在時間到時執行的代碼
    // uart_puts("Timer interrupt triggered!\r\n");

    // // 重設計時器（例如每2秒）
    // set_time_out(2);
}
