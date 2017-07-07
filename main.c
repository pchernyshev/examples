#include <stdbool.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define STR(x) #x
#define BIT(x) (1 << (x))

static int print_polling(const char str[]);


/* Led handling routines */
/* Led is connected to PB7, Digital Pin 13*/
#define DDR_LED  DDRB
#define PORT_LED PORTB
static const uint8_t LED_BIT = BIT(7);

static void init_led(void) {
	DDR_LED |= LED_BIT;
}

static void set_led(const uint8_t on) {
	if (on)
		PORT_LED |= LED_BIT;
	else
		PORT_LED &= ~LED_BIT;
}



/* Button handing routines */
/* Button is connected to PB6, Digital Pin 12*/
#define DDR_BUTTON  DDRB
#define PORT_BUTTON PORTB
#define PIN_BUTTON PINB
static const uint8_t BUTTON_BIT = 1 << 6;

static void init_button(void) {
	DDR_BUTTON &= ~BUTTON_BIT;
	PORT_BUTTON |= BUTTON_BIT;
}

static bool is_button_pressed(void) {
	return (PIN_BUTTON & BUTTON_BIT) == 0;
}



/* External interrupt routines */
/* External interrupt is connected to PB5, Digital Pin 11*/
#define DDR_INT  DDRB
#define PORT_INT  PORTB
#define EXT_INTERRUPT_vect PCINT0_vect
#define EXT_INTERRUPT_MASK PCMSK0
#define EXT_INTERRUPT_CONTROL PCICR
#define EXT_INTERRUPT_CONTROL_BIT 1
static const uint8_t INT_BIT = 1 << 5;

static void init_interrupt(void) {
	DDR_INT  &= ~INT_BIT;
	PORT_INT |= INT_BIT;
	EXT_INTERRUPT_MASK |= INT_BIT;
	EXT_INTERRUPT_CONTROL |= EXT_INTERRUPT_CONTROL_BIT;
}

ISR(EXT_INTERRUPT_vect) {
	print_polling("Interrupt\r\n");
}



/* Timer routines */
//TODO: cleanup
static void init_timers(void) {
	/* Timer initialization, 0 to comparator, see ISR below */
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); /* comparator and prescaler */
	OCR1A = 15624;
	TIMSK1 = 1 << OCIE1A;

	/* Timer initialization, via overflow */
	TCCR3A = 0;
	TCCR3B = (1 << CS32) | (1 << CS30); /* prescaler */
	TCNT3 = 65535-8000;
	TIMSK3 = 1 << TOIE3;
}

ISR(TIMER1_COMPA_vect) {
	print_polling("X\r\n");
}

ISR(TIMER3_OVF_vect) {
	print_polling("Y\r\n");
	TCNT3 = 65535-8000;

}



/* UART routines */
//TODO cleanup
static void init_uart(const unsigned long baudrate) {
	const uint16_t ubrr = F_CPU/8/baudrate - 1;

	UBRR0H = ubrr >> 8;
	UBRR0L = ubrr & 0xff;
	UCSR0A |= 1 << U2X0;

	UCSR0B = 1 << TXEN0;

	/* Set frame format: 8data, 1stop bit */
	UCSR0C = 3 << UCSZ00;
}

/* Primitive synchronous print */
static int print_polling(const char str[]) {
	size_t i = 0;
	const size_t N = strlen(str);

	for (i = 0; i < N; i++) {
		UDR0 = str[i];
		while (!(UCSR0A & (1 << UDRE0)));
	}

	return N;
}

/* Bad example of the delay, but good example of a volatile mine */
static void mydelay_us(uint32_t us) {
	const uint8_t TICKS_IN_1uS = F_CPU/1000000L;
	const uint16_t CALIBRATION = 8; /* Need to estimate tight loop value more precisely */
	const uint16_t DURATION_OF_U8MAX_TICKS = UINT8_MAX / (TICKS_IN_1uS/CALIBRATION);
	volatile uint8_t clocks = 0;

	while (us > DURATION_OF_U8MAX_TICKS) {
		while (__builtin_expect((--clocks),1));
		us -= DURATION_OF_U8MAX_TICKS;
	}

	clocks = us * TICKS_IN_1uS;
	while (__builtin_expect((--clocks),1));
}

static void mydelay_ms(unsigned short ms) {
	mydelay_us((uint32_t)(ms) * 1000);
}



void setup() {
	cli();

	init_uart(115200);
	init_led();
	init_button();
	init_interrupt();
	init_timers();

	sei();

	print_polling("Initialization finished\r\n");
}


int main()
{
	const unsigned DELAY_MS = 200;

	setup();

	while(1) {
		if (is_button_pressed())
			set_led(1);
		else
			set_led(0);

		mydelay_ms(DELAY_MS);
	}

	/* Not reachable */
	return 0;
}
