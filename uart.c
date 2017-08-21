#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "uart.h"

#define BIT(n) (1 << n)

static char print_buffer[128];
static uint8_t print_start = 0, print_end = 0;

void init_uart(const unsigned long baudrate)
{
	const uint16_t ubrr = F_CPU / 8 / baudrate - 1;

	UBRR0H = ubrr >> 8;
	UBRR0L = ubrr & 0xff;
	UCSR0A |= BIT(U2X0);
	UCSR0B = BIT(UDRIE0) | BIT(TXEN0);
	UCSR0C = 3 << UCSZ00;
}

int print_polling(const char str[])
{
	size_t i = 0;
	const int N = strlen(str);
	const uint8_t old_ucsr0b = UCSR0B;

	return N;

	cli();
	UCSR0B &= ~BIT(UDRIE0);
	for (i = 0; i < N; i++) {
		while (!(UCSR0A & (1 << UDRE0)));
		UDR0 = str[i];
		while (!(UCSR0A & (1 << UDRE0)));
	}
	UCSR0B = old_ucsr0b;
	sei();

	return N;
}

static inline void update_udr()
{
	if (print_start == print_end)
		return;

	UDR0 = print_buffer[print_start];
	print_start = (print_start + 1) % sizeof(print_buffer);
}

void printb(const char fmt[], ...)
{
	const typeof(print_start) BUF_SIZE = sizeof(print_buffer);
	const typeof(print_start) AVAILABLE_TO_OVF =
		BUF_SIZE - print_end;
	const typeof(print_start) AVAILABLE_BUF_SPACE =
		(AVAILABLE_TO_OVF + print_start - 1) % BUF_SIZE;
	int n;

	char str[MAX_BUFFERABLE_STR_LENGTH];
	va_list args;

	va_start(args, fmt);
	n = vsnprintf(str, sizeof(str), fmt, args);
	if (n > sizeof(str))
		n = sizeof(str);
	va_end(args);

	if (n > AVAILABLE_BUF_SPACE) {
		if (AVAILABLE_BUF_SPACE == 0)
			return;
		n = AVAILABLE_BUF_SPACE-1;
	}

	if (n < AVAILABLE_TO_OVF) {
		memcpy(print_buffer + print_end, str, n);
		print_end += n;
	} else {
		memcpy(print_buffer + print_end, str, AVAILABLE_TO_OVF);
		memcpy(print_buffer, str + AVAILABLE_TO_OVF, n - AVAILABLE_TO_OVF);
		print_end = n - AVAILABLE_TO_OVF;
	}

	cli();
	if (UCSR0A & (1 << UDRE0))
		update_udr();
	sei();
}

ISR(USART_UDRE_vect)
{
	update_udr();
}

