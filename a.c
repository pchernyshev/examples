#include <stdio.h>
#include <stdint.h>

#define PROGMEM
#include "Hello_world_img.h"

struct screen {
	uint8_t d; /* holder for data byte for the first row */
	uint8_t b[8][128];
} __attribute__((packed));

int main()
{
	uint8_t b[1024] = { 0 };
	int i = 0, j = 0, k = 0;
	for (i = 0; i < 8*1024; i += 1024)
		for (j = 0; j < 128; j++)
			b[k++] =
				(header_data[i + j]) |
				(header_data[i + j + (1 << 7)] << 1) |
				(header_data[i + j + (2 << 7)] << 2) |
				(header_data[i + j + (3 << 7)] << 3) |
				(header_data[i + j + (4 << 7)] << 4) |
				(header_data[i + j + (5 << 7)] << 5) |
				(header_data[i + j + (6 << 7)] << 6) |
				(header_data[i + j + (7 << 7)] << 7);

	printf("#include <avr/pgmspace.h>\n\n");
	printf("static const uint8_t PROGMEM header_data[] = {\n");
	for (i = 0; i < k; i++)
		printf("%#4hhx,%s", b[i], (i+1) % 16? "": "\n");
	printf("};\n");
	return 0;
}
