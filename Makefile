ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

mqtt-ble-hid-src = $(wildcard src/mqtt-ble-hid/*.c) \
	$(wildcard src/mqtt-ble-hid/services/*.c)
mqtt-ble-hid-obj = $(mqtt-ble-hid-src:.c=.o)

mqtt-ble-hid-gatt-src = $(wildcard src/mqtt-ble-hid/services/*-services.gatt)
mqtt-ble-hid-gatt = $(mqtt-ble-hid-gatt-src:.gatt=.h)

usb-hid-mqtt-src = $(wildcard src/usb-hid-mqtt/*.c)
usb-hid-mqtt-obj = $(usb-hid-mqtt-src:.c=.o)

CFLAGS += -std=gnu99 -Wall -Werror -fPIC -O2 -g

CFLAGS:=$(filter-out -Wredundant-decls,${CFLAGS})
CFLAGS:=$(filter-out -Wshadow,${CFLAGS})
CFLAGS:=$(filter-out -g,${CFLAGS})
CFLAGS:=$(filter-out -Wmissing-prototypes,${CFLAGS})
CFLAGS:=$(filter-out -Werror=unused-but-set-variable,${CFLAGS})
CFLAGS:=$(filter-out -Werror=unused-function,${CFLAGS})

default_target: all

%.h: %.gatt
	python lib/btstack/tool/compile_gatt.py -Isrc/mqtt-ble-hid/services $< $@ 

out:
	mkdir -p $@

out/mqtt-ble-hid: $(out) $(mqtt-ble-hid-gatt) $(mqtt-ble-hid-obj)
	$(CC) -o $@ $^ $(CFLAGS) -lpaho-mqtt3as -lpthread -lrt -lm -lbtstack -lbtstack_raspi -Ilib/btstack/src

out/usb-hid-mqtt: $(out) $(usb-hid-mqtt-obj)
	$(CC) -o $@ $^ $(CFLAGS) -lusb-1.0 -lpthread -ludev -lpaho-mqtt3as

.PHONY: clean
clean:
	rm -f $(obj) out/mqtt-ble-hid
	rm -f $(obj) out/usb-hid-mqtt

all: out/mqtt-ble-hid out/usb-hid-mqtt
	@echo "Done"

.PHONY: install
install: all
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 out/mqtt-ble-hid $(DESTDIR)$(PREFIX)/bin/
	install -m 755 out/usb-hid-mqtt $(DESTDIR)$(PREFIX)/bin/
	# install -d $(DESTDIR)$(PREFIX)/lib/hid-ble-bridge
	# install -m 644 src/mqtt-ble-hid/mqtt-ble-hid.service /etc/systemd/system
	# install -m 644 src/usb-hid-mqtt/usb-hid-mqtt.service /etc/systemd/system