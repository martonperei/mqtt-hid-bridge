BTSTACK_ROOT=./lib/btstack
BTSTACK_DIR=./lib/btstack/port/raspi

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

include ${BTSTACK_DIR}/Makefile

VPATH+= ${BTSTACK_DIR}

mqtt-ble-hid-src = $(wildcard src/mqtt-ble-hid/*.c) \
	$(wildcard src/mqtt-ble-hid/services/*.c)
mqtt-ble-hid-obj = $(mqtt-ble-hid-src:.c=.o)
mqtt-ble-hid-deps = ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} \
	battery_service_server.o device_information_service_server.o btstack_ring_buffer.o

mqtt-ble-hid-gatt-src = $(wildcard src/mqtt-ble-hid/services/*-services.gatt)
mqtt-ble-hid-gatt = $(mqtt-ble-hid-gatt-src:.gatt=.h)

usb-hid-mqtt-src = $(wildcard src/usb-hid-mqtt/*.c)
usb-hid-mqtt-obj = $(usb-hid-mqtt-src:.c=.o)

CFLAGS += -I${BTSTACK_DIR} -std=gnu99

CFLAGS:=$(filter-out -Wredundant-decls,${CFLAGS})
CFLAGS:=$(filter-out -Wshadow,${CFLAGS})
CFLAGS:=$(filter-out -g,${CFLAGS})
CFLAGS:=$(filter-out -Wmissing-prototypes,${CFLAGS})
CFLAGS:=$(filter-out -Werror=unused-but-set-variable,${CFLAGS})
CFLAGS:=$(filter-out -Werror=unused-function,${CFLAGS})

default_target: all

%.h: %.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py -Isrc/mqtt-ble-hid/services $< $@ 

out:
	mkdir -p $@

out/mqtt-ble-hid: $(mqtt-ble-hid-gatt) $(mqtt-ble-hid-obj) $(mqtt-ble-hid-deps)
	$(CC) -o $@ $^ $(CFLAGS) -lpaho-mqtt3as -lpthread 

out/usb-hid-mqtt: $(usb-hid-mqtt-obj)
	$(CC) -o $@ $^ $(CFLAGS) -lusb-1.0 -lpaho-mqtt3as 

.PHONY: clean
clean:
	rm -f $(obj) out/mqtt-ble-hid
	rm -f $(obj) out/usb-hid-mqtt

.PHONY: libbtstack
libbtstack:
	BTSTACK_ROOT=../.. && cd lib/btstack/port/raspi && $(MAKE)

all: libbtstack out/mqtt-ble-hid out/usb-hid-mqtt
	@echo "Done"

.PHONY: install
install: all
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 out/mqtt-ble-hid $(DESTDIR)$(PREFIX)/bin/
	install -m 755 out/usb-hid-mqtt $(DESTDIR)$(PREFIX)/bin/
	# install -d $(DESTDIR)$(PREFIX)/lib/hid-ble-bridge
	# install -m 644 src/mqtt-ble-hid/mqtt-ble-hid.service /etc/systemd/system
	# install -m 644 src/usb-hid-mqtt/usb-hid-mqtt.service /etc/systemd/system