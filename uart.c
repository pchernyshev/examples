#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "uart.h"

#define BIT(n) (1 << n)

static char print_buffer[255];
static uint8_t print_start = 0, print_end = 0;

void init_uart(const unsigned long baudrate)
{
	const uint16_t ubrr = F_CPU / 8 / baudrate - 1;

#if MCU == atmega2560
	PRR0 &= ~BIT(PRUSART0);
#else
	PRR &= ~BIT(PRUSART0);
#endif
	UBRR0H = ubrr >> 8;
	UBRR0L = ubrr & 0xff;
	UCSR0A |= BIT(U2X0);

	UCSR0B = BIT(UDRIE0) | BIT(TXEN0);

	/* Set frame format: 8data, 1stop bit */
	UCSR0C = 3 << UCSZ00;
}

int print_polling(const char str[])
{
	size_t i = 0;
	const size_t N = strlen(str);

	cli();
	for (i = 0; i < N; i++) {
		UDR0 = str[i];
		while (!(UCSR0A & (1 << UDRE0)));
	}
	sei();

	return N;
}

static inline void update_udr()
{
	if (print_start != print_end)
		return;

	UDR0 = print_buffer[print_start];
	print_start = (print_start + 1) % sizeof(print_buffer);
}

void printb(const char fmt[], ...)
{
	const typeof(print_start) BUF_SIZE = sizeof(print_buffer);
	const typeof(print_start) AVAILABLE_TO_OVF =
		sizeof(print_buffer) - print_end;
	const typeof(print_start) AVAILABLE_BUF_SPACE =
		(BUF_SIZE - print_end + print_start - 1) % BUF_SIZE;
	int n;

	char str[MAX_BUFFERABLE_STR_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsnprintf(str, sizeof(str), fmt, args);
	n = strlen(str);
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

#if MCU == atmega2560
ISR(USART0_UDRE_vect)
#else  /* !atmega2560 */
ISR(USART_UDRE_vect)
#endif /* MCU == atmega2560 */
{
	update_udr();
}

