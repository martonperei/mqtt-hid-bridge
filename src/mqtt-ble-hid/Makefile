ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

BTSTACK_ROOT ?= ../../lib/btstack

BIN := mqtt-ble-hid

DEPDIR := .deps
OBJDIR := .objs

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/chipset/bcm
VPATH += ${BTSTACK_ROOT}/port/raspi
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

SRCS := \
	btstack_chipset_bcm.c \
	btstack_chipset_bcm_download_firmware.c \
	btstack_run_loop_posix.c \
	btstack_tlv_posix.c \
	btstack_link_key_db_tlv.c \
	btstack_uart_posix.c \
	btstack_signal.c \
	btstack_slip.c \
	hci_transport_h4.c \
	hci_transport_h5.c \
	le_device_db_tlv.c \
	wav_util.c \
	btstack_stdin_posix.c \
	rijndael.c \
	btstack_memory.c \
	btstack_linked_list.c \
	btstack_memory_pool.c \
	btstack_run_loop.c \
	btstack_util.c \
	ad_parser.c \
	hci.c \
	hci_cmd.c \
	hci_dump.c \
        hci_dump_posix_fs.c \
	l2cap.c \
	l2cap_signaling.c \
	btstack_audio.c \
	btstack_tlv.c \
	btstack_crypto.c \
	uECC.c \
	sm.c \
	att_dispatch.c \
	att_db.c \
	att_server.c \
	battery_service_server.c	\
	device_information_service_server.c	\
	btstack_ring_buffer.c	\
	btstack_control_raspi.c	\
	raspi_get_model.c		\
	main.c					\
	hids_device_remote.c 	\
	mqtt-ble-hid.c			\

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

SERVICES_SRCS = mqtt-ble-hid-services.gatt
SERVICES_GATT = $(SERVICES_SRCS:%.gatt=%.h)

CC := gcc
LD := gcc

CFLAGS := -std=gnu99 -Wall -Werror 			\
	-I${BTSTACK_ROOT}/src					\
	-I${BTSTACK_ROOT}/src/ble				\
	-I${BTSTACK_ROOT}/src/ble/gatt-service	\
	-I${BTSTACK_ROOT}/src/classic			\
	-I$(BTSTACK_ROOT)/platform/posix		\
	-I$(BTSTACK_ROOT)/chipset/bcm			\
	-I${BTSTACK_ROOT}/port/raspi			\
	-I${BTSTACK_ROOT}/3rd-party/rijndael	\
	-I${BTSTACK_ROOT}/3rd-party/micro-ecc	\
	-I${MQTT_ROOT}/src

LDFLAGS :=
LDLIBS := -lpaho-mqtt3as -lpthread -lrt -lm -ldl -llua5.3
DEPFLAGS = -MT $@ -MD -MP -MF $(DEPDIR)/$*.Td

PRECOMPILE =
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

all: $(BIN)

.PHONY: compile
compile: $(OBJS)

.PHONY: clean
clean:
	$(RM) -r $(OBJDIR) $(DEPDIR) $(SERVICES_GATT)

.PHONY: install
install: all
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/
	# install -d $(DESTDIR)$(PREFIX)/lib/hid-ble-bridge
	# install -m 644 $(BIN).service /etc/systemd/system

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/%.o: %.c 
$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d | $(DEPDIR) $(OBJDIR) PREPROCESS_BTSTACK $(SERVICES_GATT)
	$(PRECOMPILE)
	$(CC) $(DEPFLAGS) $(CFLAGS)  -c -o $@ $<
	$(POSTCOMPILE)

$(DEPDIR): ; @mkdir -p $@
$(OBJDIR): ; @mkdir -p $@

.PHONY: PREPROCESS_BTSTACK
PREPROCESS_BTSTACK: 
	sed -i 's/^#define TLV_DB_PATH_PREFIX .*/#define TLV_DB_PATH_PREFIX ".\/btstack_"/g' ${BTSTACK_ROOT}/port/raspi/main.c
	sed -i 's/^\/\/ #define ENABLE_CONTROLLER_WARM_BOOT/#define ENABLE_CONTROLLER_WARM_BOOT/g' ${BTSTACK_ROOT}/port/raspi/btstack_config.h

%.h: %.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py -I. $< $@ 

.PRECIOUS: $(DEPDIR)/%.d \
	$(SERVICES_GATT)
$(DEPDIR)/%.d: ;

-include $(DEPS)
