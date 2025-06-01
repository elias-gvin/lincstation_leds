TARGET = lincstation_leds
CC = gcc
CFLAGS = -O2

all: lincstation_leds

lincstation_leds: lincstation_leds.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c

clean:
	-rm -f $(TARGET) *~ 
