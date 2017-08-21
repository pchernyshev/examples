DEVICE := atmega328p
F_CPU  := 16000000

PROGRAMMER := arduino
PORT   := /dev/ttyACM1
SPEED  := 115200

CC      := colorgcc
#AVRDUDE := avrdude -v -p$(DEVICE) -c$(PROGRAMMER) -D -V
AVRDUDE := avrdude -v -p$(DEVICE) -c$(PROGRAMMER) -P$(PORT) -b$(SPEED) -D -V

CFLAGS  += -Wall -O3 -DF_CPU=$(F_CPU) -DMCU=$(DEVICE) -mmcu=$(DEVICE) \
		   -Wl,-u,vfprintf -lprintf_flt -lm \
		   -Wdouble-promotion \
		   -Wunsafe-loop-optimizations -Wcast-align \
		   -Wpadded \
		   -fmerge-all-constants \
		   -ffast-math -funroll-loops \
		   -fstack-check --std=gnu99
#LDFLAGS := -fwhole-program

OBJECTS := main.o uart.o i2c.o
TMPOUT  := main.elf
OUT     := main.hex

$(OUT):
.PHONY: $(OUT) all flash clean

flash: $(OUT)
	$(AVRDUDE) -U flash:w:$^:i

clean:
	-rm -f $(OUT) $(TMPOUT) $(OBJECTS)

$(OUT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TMPOUT) $^ $(LDFLAGS) 
	avr-objcopy -j .text -j .data -j .bss  -O ihex $(TMPOUT) $@
	avr-size -t --format=avr --mcu=$(DEVICE) $(TMPOUT)

