#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "uart.h"
#include "i2c.h"

#define BIT(x) (1 << (x))

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
	//TIMSK1 = 1 << OCIE1A;

#if 0
	/* Timer initialization, via overflow */
	TCCR3A = 0;
	TCCR3B = (1 << CS32) | (1 << CS30); /* prescaler */
	TCNT3 = 65535-8000;
	TIMSK3 = 1 << TOIE3;
#endif
}

ISR(TIMER1_COMPA_vect) {
	print_polling("X\r\n");
}

#if 0
ISR(TIMER3_OVF_vect) {
	TCNT3 = 65535-8000;
}
#endif




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



void hw_init() {
	cli();

	init_uart(115200);
	init_led();
	init_button();
	init_interrupt();
	init_timers();
	init_i2c();

	sei();

	print_polling("Initialization finished\r\n");
}

void init() {
	size_t i = 0;
	const uint8_t CONTINOUS_HIGH_RESOLUTION = 0x10;
	const uint8_t DISPLAY_INIT[] = {
		0xae, 0x00, 0x10, 0x40, 0xb0, 0x81, 0xff, 0xa1, 0xa6, 0xc9, 0xa8,
		0x3f, 0xd3, 0x00, 0xd5, 0x80, 0xd9, 0xf1, 0xda, 0x12, 0xdb, 0x40,
		0x8d, 0x14, 0xaf
	};

	i2c_send(0x23, 1, &CONTINOUS_HIGH_RESOLUTION);

#if 1
	for (i = 0; i <sizeof(DISPLAY_INIT); i++) {
		uint8_t cmd[] = { 0, 0 };
		cmd[1] = DISPLAY_INIT[i];
		i2c_send(0x3c, sizeof(cmd), cmd);
		mydelay_ms(50);
	}
#endif

	DDRB |= 0x7;
	PORTB |= 0x7;

	//DIO  = PB1
	//SCLK = PB0
	//RCLK = PB2

		/*
	PORTB &= ~0x2;
		mydelay_ms(1);
	for (i = 0; i < 16; i++) {
		PORTB |= 0x4;
		mydelay_ms(1);
		PORTB &= ~0x4;
		mydelay_ms(1);

	}

	PORTB &= ~0x2;
	for (i = 0; i < 16; i++) {
		PORTB &= ~0x1;
		mydelay_ms(1);
		PORTB |= 0x1;
		mydelay_ms(1);
	}

	PORTB |= 0x2;
	for (i = 0; i < 4; i++) {
		PORTB &= ~0x1;
		mydelay_ms(1);
		PORTB |= 0x1;
		mydelay_ms(1);
	}

	PORTB &= ~0x4;
		mydelay_ms(1);
	PORTB |= 0x4;
		mydelay_ms(1);
	*/
}

void read_sensor()
{
	uint16_t brightness = 0;
	if (i2c_receive(0x23, sizeof(brightness), (uint8_t *)&brightness)) {
		char s[sizeof("0x0000\r\n")];
		sprintf(s, "%#04x\r\n", brightness);
	}
}

int main()
{
	const unsigned DELAY_MS = 1000;

	hw_init();
	init();

	while(1) {
#if 1 
		if (is_button_pressed())
			set_led(1);
		else
			set_led(0);
#endif

		mydelay_ms(DELAY_MS);
		read_sensor();
	}

	/* Not reachable */
	return 0;
}
