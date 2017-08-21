#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

#include "uart.h"
#include "i2c.h"

#include "img.h"
#include <avr/pgmspace.h>

#define BIT(x) (1 << (x))


static const uint8_t DISPLAY_ADDR = 0x3c;
static const uint8_t GYRO_ADDR = 0x69;
static const uint8_t COMPASS_ADDR = 0x1e;
static const uint8_t ACC_ADDR = 0x53;

enum {
	DISPLAY_ON_OFF = 0xae,
	DISPLAY_SET_PAGE_ADDR   = 0xb0,
	DISPLAY_SET_LOW_COLUMN  = 0x10,
	DISPLAY_SET_HIGH_COLUMN = 0x10,
	DISPLAY_CLOCKDIV        = 0xd5,
	DISPLAY_MUXRATIO        = 0xa8,
	DISPLAY_CONTRAST        = 0x81,
	DISPLAY_INVERSION       = 0xa6,
	DISPLAY_PRECHARGEPERIOD = 0xd9,
	DISPLAY_CHARGE          = 0x8d,
	DISPLAY_ADDRESSING_MODE = 0x20
};

#if 0
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
#endif



/* Timer routines */
//TODO: cleanup
static void init_timers(void) {
	/* Timer initialization, 0 to comparator, see ISR below */
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); /* comparator and prescaler */
	OCR1A = 15624;
	//TIMSK1 = 1 << OCIE1A;
}

ISR(TIMER1_COMPA_vect) {
//	print_polling("X\r\n");
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

	clocks = (uint8_t)(us * TICKS_IN_1uS);
	while (__builtin_expect((--clocks),1));
}

static void mydelay_ms(unsigned short ms) {
	mydelay_us((uint32_t)(ms) * 1000);
}



void hw_init() {
	cli();

	init_uart(115200);
	//init_interrupt();
	init_timers();
	init_i2c();

	sei();

	printb("Initialization finished\r\n");
}

bool display_command(uint8_t N, ...)
{
	typeof(N) n = 0;
	uint32_t cint;
	uint8_t *c = (uint8_t*)&cint;
	va_list args;

	if (N > 3)
		return false;

	va_start(args, N);
	c[0] = 0;
	for (n = 1; n <= N; n++) {
		c[n] = (uint8_t) va_arg(args, int);
	}
	va_end(args);

	i2c_send(DISPLAY_ADDR, n, c);

	return true;
}

void putpixel(uint8_t b[], uint8_t x, uint8_t y, bool set)
{
	if (x < 128 && y < 64) {
		const uint8_t MASK = 1 << (y & 7);
		const uint16_t pos = ((y & ~7) << 4) + x;
		if (set)
			b[pos] |= MASK;
		else
			b[pos] &= ~MASK;
	}
}

void line(uint8_t b[], long x0, long y0, long x1, long y1)
{
	long incx = (x1 > x0)? 1: -1;
	long incy = (y1 > y0)? 1: -1;
	const long dx = ((long)x1 - x0)*incx;
	const long dy = ((long)y0 - y1)*incy;
	long e = dx + dy;

	while ((x0 != x1) || (y0 != y1)) {
		const long double_e = e << 1;
		if (x0 > 0 && y0 >0 && x0 <127 && y0 < 63)
			putpixel(b, x0, y0, true);
		if (double_e >= dy) {
			e += dy;
			x0 += incx;
		}
		if (double_e <= dx) {
			e += dx;
			y0 += incy;
		}
	}
}

struct screen {
	uint8_t data_sign_holder;
	uint8_t b[1024];
} __attribute__((packed));

void dump_buffer(struct screen *s)
{
	s->data_sign_holder = 0x40;
	i2c_send(DISPLAY_ADDR, sizeof(*s), (uint8_t*)s);
}

static struct screen s;
void init_display()
{
	display_command(1, DISPLAY_ON_OFF | 0);
	display_command(2, DISPLAY_ADDRESSING_MODE, 0);
	display_command(1, DISPLAY_INVERSION | 0);
	display_command(2, DISPLAY_CHARGE, 0x14);
	display_command(1, DISPLAY_ON_OFF | 1);

	printb("Display init_done\r\n");

	memcpy_P(s.b, header_data, sizeof(header_data));
	//line(s.b, 0, 31, 127, 31);
	//line(s.b, 127, 0, 0, 63);
	//line(s.b, 0, 0, 127, 63);
	dump_buffer(&s);
}

void init_gyro()
{
	uint8_t mode[2] = { 0x20, 0x0f };
	i2c_send(GYRO_ADDR, sizeof(mode), mode);
}

void read_gyro(int16_t v[])
{
	const uint8_t get_cmd = 0x28 | BIT(7);
	i2c_send(GYRO_ADDR, 1, &get_cmd);
	i2c_receive(GYRO_ADDR, 6, (uint8_t *)v);
#if 0
	v[0] = (v[0] << 8) | ((v[0] & 0xff00) >> 8);
	v[1] = (v[1] << 8) | ((v[1] & 0xff00) >> 8);
	v[2] = (v[2] << 8) | ((v[2] & 0xff00) >> 8);
#endif
}

void read_compass(int16_t v[])
{
	const uint8_t get_cmd = 0x03;
	i2c_receive(COMPASS_ADDR, 6, (uint8_t *)v);
	i2c_send(COMPASS_ADDR, 1, &get_cmd);
#if 1
	v[0] = (v[0] << 8) | ((v[0] & 0xff00) >> 8);
	v[1] = (v[1] << 8) | ((v[1] & 0xff00) >> 8);
	v[2] = (v[2] << 8) | ((v[2] & 0xff00) >> 8);
#endif
}

int init_compass()
{
	const uint16_t MAX_THRESHOLD = 575;
	const uint16_t MIN_THRESHOLD = 243;
	const uint8_t get_cmd = 0x03;
	uint8_t ctrl[2] = { 0x00, 0x71 };
	uint8_t gain[2] = { 0x01, 0xa0 };
	const uint8_t mode[2] = { 0x02, 0x00 };
	int16_t v[3];

	printb("Compass self-test... ");
	i2c_send(COMPASS_ADDR, sizeof(ctrl), ctrl);
	i2c_send(COMPASS_ADDR, sizeof(gain), gain);
	i2c_send(COMPASS_ADDR, sizeof(mode), mode);
	i2c_send(COMPASS_ADDR, 1, &get_cmd);

	mydelay_ms(10);

	read_compass(v);
	if ((v[0] > MAX_THRESHOLD || v[1] > MAX_THRESHOLD || v[2] > MAX_THRESHOLD) ||
		(v[0] < MIN_THRESHOLD || v[1] < MIN_THRESHOLD || v[2] < MIN_THRESHOLD)) {
		printb("error: %+hd/%+hd/%+hd\r\n", v[0], v[1], v[2]);
		return -1;
	} else
		printb("ok\r\n");

	ctrl[1] = 0x18;
	//gain[1] = 0x20;
	i2c_send(COMPASS_ADDR, sizeof(ctrl), ctrl);
	//i2c_send(COMPASS_ADDR, sizeof(gain), gain);
	i2c_send(COMPASS_ADDR, 1, &get_cmd);
	mydelay_ms(10);

	return 0;
}

void init_acc()
{
	uint8_t mode[2] = { 0x2d, 0x08 };
	i2c_send(ACC_ADDR, sizeof(mode), mode);
}

void read_acc(int16_t v[])
{
	const uint8_t get_cmd = 0x32;
	i2c_send(ACC_ADDR, 1, &get_cmd);
	i2c_receive(ACC_ADDR, 6, (uint8_t *)v);
#if 0
	v[0] = (v[0] << 8) | ((v[0] & 0xff00) >> 8);
	v[1] = (v[1] << 8) | ((v[1] & 0xff00) >> 8);
	v[2] = (v[2] << 8) | ((v[2] & 0xff00) >> 8);
#endif
}

void init() {

	init_display();
//	init_gyro();
	init_acc();
//	init_compass();

	DDRB |= 0x7;
	PORTB |= 0x7;
}

int main()
{

	hw_init();
	init();

	mydelay_ms(100);
	while(1) {
		int16_t v[3];
		double phi;
//		read_gyro(v);
//		printb("Gyro: %+6hd %+6hd %+6hd\r\n", v[0], v[1], v[2]);
//		read_compass(v);
//		printb("Comp: %+6hd %+6hd %+6hd\r\n", v[0], v[1], v[2]);
		read_acc(v);
//		printb("Accl: %+6hd %+6hd %+6hd %f.\r\n", v[0], v[1], v[2],
//				atan2(v[1], v[2])*180/3.14159);
		phi = atan2(v[1], v[2]);
		printb("Accl: %+5.1f \r\n", phi*180/3.14159);

		memset(s.b, 0, sizeof(s));
		if (v[2]) {
			const long dy = -lround(64.0*tan(phi));
			line(s.b, 0, 32+dy, 127, 32-dy);
		}

		dump_buffer(&s);
		mydelay_ms(10);
	}

	/* Not reachable */
	return 0;
}
