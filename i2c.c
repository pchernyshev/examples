#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>

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
	TWI_M_RDATA_NACK = 0x58,
};

enum TWI_ERROR_STATUS {
	TWI_OK,
	TWI_ERROR,
	TWI_FATAL,
	TWI_UNKNOWN
};

static const char* twi_error_strings[] = {
	[ 0 ] = "Unknown status",
	[ TWI_M_START ]            = "START condition has been transmitted",
	[ TWI_M_START_REPEAT ]     = "A repeated START condition has been transmitted",
	[ TWI_M_ARBITRATION_LOST ] = "Arbitration lost",
	[ TWI_M_SLAW_ACK ]         = "SLA+W has been transmitted, got ACK",
	[ TWI_M_SLAW_NACK ]        = "SLA+W has been transmitted, got NACK",
	[ TWI_M_WDATA_ACK ]        = "Data has been transmitted, got ACK",
	[ TWI_M_WDATA_NACK ]       = "Data has been transmitted, got NACK",
	[ TWI_M_SLAR_ACK ]         = "SLA+R has been transmitted, got ACK",
	[ TWI_M_SLAR_NACK ]        = "SLA+R has been transmitted, got NACK",
	[ TWI_M_RDATA_ACK ]        = "Data has been received, sent ACK",
	[ TWI_M_RDATA_NACK ]       = "Data has been received, sent NACK",
};

static const char *i2c_last_error;


static int i2c_check_status()
{
	uint8_t twsr = TWSR;
	enum TWI_ERROR_STATUS err = TWI_OK;

	switch (twsr) {
		case TWI_M_SLAW_NACK:
		case TWI_M_SLAR_NACK:
		case TWI_M_WDATA_NACK:
		case TWI_M_RDATA_NACK:
			err = TWI_ERROR;
			break;

		case TWI_M_START:
		case TWI_M_SLAW_ACK:
		case TWI_M_WDATA_ACK:
		case TWI_M_SLAR_ACK:
		case TWI_M_RDATA_ACK:
			err = TWI_OK;
			break;

		case TWI_M_START_REPEAT:
		case TWI_M_ARBITRATION_LOST:
			err = TWI_FATAL;
			break;

		default:
			err = TWI_FATAL;
			twsr = 0;
			break;
	}

	i2c_last_error = twi_error_strings[twsr];
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

static inline void i2c_dump_err()
{
	print_polling("Error: ");
	print_polling(i2c_last_error);
	print_polling("\r\n");
}



void init_i2c() {
	TWBR = (F_CPU / 100000UL - 16) / 2;
	TWSR = 0;
	TWCR = 1 << TWEN;
}

void i2c_send(uint8_t address, const size_t N, const uint8_t bytes[N])
{
	size_t i = 0;
	enum TWI_ERROR_STATUS err = TWI_OK;

	char dbg[sizeof("Sending 000 bytes\r\n")];
	sprintf(dbg, "Sending %u bytes\r\n", N);
	print_polling(dbg);

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

uint8_t i2c_receive(uint8_t address, const uint8_t N, uint8_t bytes[N])
{
	size_t n = 0;
	bool exit = false;
	enum TWI_ERROR_STATUS err = TWI_OK;

	err = i2c_start(address, true);

	while (!err && n < N) {
		TWCR = I2C_TX | (1 << TWEA);
		err = i2c_wait();
		if (!err)
			bytes[n++] = TWDR;
	}

	i2c_stop();
	if (err)
		i2c_dump_err();

	if (n) {
		size_t i = 0;
		char str[32];
		sprintf(str, "I2C[%#hhx]: ", n);
		print_polling(str);
		for (i = 0; i < n; i++) {
			sprintf(str, "%02hhx ", bytes[i]);
			print_polling(str);
		}
		print_polling("\r\n");
	}

	return n;
}

