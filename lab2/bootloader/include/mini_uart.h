#ifndef	_MINI_UART_H
#define	_MINI_UART_H

void uart_init();
char uart_getc();
char uart_recv();
void uart_send(unsigned int c);
int  uart_puts(char* fmt, ...);
void uart_2hex(unsigned int d);

#endif  /*_MINI_UART_H */
