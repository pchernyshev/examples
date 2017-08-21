#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

#include "i2c.h"
#include "uart.h"

enum TWI_STATUS {
	TWI_M_START = 0x08,
	TWI_M_START_REPEAT = 0x10,
	TWI_M_ARBITRATION_LOST = 0x38,

	/* Master Transmitter */
	TWI_M_SLAW_ACK = 0x18,
	TWI_M_SLAW_NACK = 0x20,
	TWI_M_WDATA_ACK = 0x28,
	TWI_M_WDATA_NACK = 0x30,

	/* Master Receiver */
	TWI_M_SLAR_ACK = 0x40,
	TWI_M_SLAR_NACK = 0x48,
	TWI_M_RDATA_ACK = 0x50,
	TWI_M_RDATA_NACK = 0x58
};

#define I2C_DEBUG
#define I2C_STR_ERRORS

#ifdef I2C_STR_ERRORS
static const char twi_error_strings[][44] PROGMEM = {
	[ 0 ] = "Unknown status",
	[ TWI_M_START >> 3]            = "START condition has been transmitted",
	[ TWI_M_START_REPEAT >> 3]     = "A REP-START condition has been transmitted",
	[ TWI_M_ARBITRATION_LOST >> 3] = "Arbitration lost",
	[ TWI_M_SLAW_ACK >> 3]         = "SLA+W has been transmitted, got ACK",
	[ TWI_M_SLAW_NACK >> 3]        = "SLA+W has been transmitted, got NACK",
	[ TWI_M_WDATA_ACK >> 3]        = "Data has been transmitted, got ACK",
	[ TWI_M_WDATA_NACK >> 3]       = "Data has been transmitted, got NACK",
	[ TWI_M_SLAR_ACK >> 3]         = "SLA+R has been transmitted, got ACK",
	[ TWI_M_SLAR_NACK >> 3]        = "SLA+R has been transmitted, got NACK",
	[ TWI_M_RDATA_ACK >> 3]        = "Data has been received, sent ACK",
	[ TWI_M_RDATA_NACK >> 3]       = "Data has been received, sent NACK",
};
static const char *i2c_last_error;
static inline void i2c_dump_err()
{
	printb("Error: %S\r\n", i2c_last_error);
}
static inline void i2c_remember_err(const uint8_t twsr)
{
	i2c_last_error = twi_error_strings[twsr >> 3];
}
#else
static uint8_t i2c_last_error;
static inline void i2c_dump_err()
{
	printb("Error: %#02hhx\r\n", i2c_last_error);
}
static inline void i2c_remember_err(const uint8_t twsr)
{
	i2c_last_error = twsr;
}
#endif

static int i2c_check_status()
{
	uint8_t twsr = TWSR;
	enum TWI_ERROR_STATUS err = TWI_OK;

	switch (twsr) {
		case TWI_M_SLAW_NACK:
		case TWI_M_SLAR_NACK:
		case TWI_M_WDATA_NACK:
		case TWI_M_START_REPEAT:
			err = TWI_ERROR;
			break;

		case TWI_M_START:
		case TWI_M_SLAW_ACK:
		case TWI_M_SLAR_ACK:
		case TWI_M_WDATA_ACK:
		case TWI_M_RDATA_ACK:
		case TWI_M_RDATA_NACK:
			err = TWI_OK;
			break;

		case TWI_M_ARBITRATION_LOST:
			err = TWI_FATAL;
			break;

		default:
			err = TWI_FATAL;
			twsr = 0;
			break;
	}

	i2c_remember_err(twsr);
	return err;
}

static int i2c_wait()
{
	while (!(TWCR & _BV(TWINT)));
	return i2c_check_status();
}

#define I2C_TX  (1 << TWINT) | (1 << TWEN)

static inline int i2c_start(const uint8_t address, const bool read)
{
	enum TWI_ERROR_STATUS err = TWI_OK;

	TWCR = (1 << TWSTA) | I2C_TX;
	err = i2c_wait();

	if (!err) {
		TWDR = (address << 1) | read;
		TWCR = I2C_TX;
		err  = i2c_wait();
	}

	return err;
}

static inline void i2c_stop()
{
	TWCR = I2C_TX | (1 << TWSTO);
}



void init_i2c() {
	TWBR = (F_CPU / 100000UL - 16) / 2;
	TWSR = 0;
	TWCR = 1 << TWEN;
}

#undef I2C_DEBUG

void i2c_send(uint8_t address, const size_t N, const uint8_t bytes[N])
{
	size_t i = 0;
	enum TWI_ERROR_STATUS err = TWI_OK;

#ifdef I2C_DEBUG
	printb("Sending %u: ", N);
	for (i = 0; (i < 4) && (i < N); i++)
		printb("%02hhx", bytes[i]);
	printb("\r\n%s\r\n", i < N? "...": "");
#endif /* I2C_DEBUG */

	err = i2c_start(address, false);
	if (!err)
		for (i = 0; i < N; i++) {
			TWDR = bytes[i];
			TWCR = I2C_TX;
			err = i2c_wait();
			if (err)
				break;
		}

	i2c_stop();
	if (err)
		i2c_dump_err();
}

#define I2C_DEBUG
uint8_t i2c_receive(uint8_t address, const uint8_t N, uint8_t bytes[N])
{
	size_t n = 0;
	enum TWI_ERROR_STATUS err = TWI_OK;

#ifdef I2C_DEBUG
	printb("I2C[%#hhx]: ", N);
#endif /* I2C_DEBUG */
	err = i2c_start(address, true);

	while (!err && n < N) {
		if (n < N-1)
			TWCR = I2C_TX | 1 << TWEA;
		else
			TWCR = I2C_TX;
		err = i2c_wait();
		if (!err) {
			bytes[n] = TWDR;
#ifdef I2C_DEBUG
			printb("%02hhx", bytes[n]);
#endif /* I2C_DEBUG */
		}
		n++;
	}

	i2c_stop();
	if (err)
		i2c_dump_err();

#ifdef I2C_DEBUG
	printb("\r\n");
#endif /* I2C_DEBUG */

	return n;
}

