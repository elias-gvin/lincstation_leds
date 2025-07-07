TARGET = lincstation_leds
CC = gcc
CFLAGS = -O2
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
SYSTEMDDIR = /etc/systemd/system

all: lincstation_leds

lincstation_leds: lincstation_leds.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c

install: lincstation_leds
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/
	install -d $(SYSTEMDDIR)
	install -m 644 lincstation-leds.service $(SYSTEMDDIR)/
	systemctl daemon-reload
	systemctl enable lincstation-leds.service

uninstall:
	systemctl stop lincstation-leds.service || true
	systemctl disable lincstation-leds.service || true
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SYSTEMDDIR)/lincstation-leds.service
	systemctl daemon-reload

clean:
	-rm -f $(TARGET) *~

.PHONY: all install uninstall clean
