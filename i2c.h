#ifndef _I2C_H
#define _I2C_H

#include <stdint.h>

enum TWI_ERROR_STATUS {
	TWI_OK,
	TWI_ERROR,
	TWI_FATAL,
	TWI_UNKNOWN
};

void init_i2c();
void i2c_send(uint8_t address, const size_t N, const uint8_t bytes[N]);
uint8_t i2c_receive(uint8_t address, const uint8_t N, uint8_t bytes[N]);

#endif /* _I2C_H */
