#ifndef _UART_H
#define _UART_H

#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_BUFFERABLE_STR_LENGTH 64

/** UART Initialization function
 *  Enables UART interrupt and powers up the UART module
 */
void init_uart(const unsigned long baudrate);

/** Primitive synchronous printing.
 *
 *  Calls to print_polling normally should not be mixed with buffered interrupt
 *  prints, as print_polling disables UART interrupt.
 *  @param str NULL-terminated string to be printed.
 *  @return number of printed characters
 */
int print_polling(const char str[]);

void printb(const char fmt[], ...);

#endif /* _UART_H */
