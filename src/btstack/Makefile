ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

LDCONFIG ?= /sbin/ldconfig

BTSTACK_ROOT ?= ../../lib/btstack

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/chipset/bcm
VPATH += ${BTSTACK_ROOT}/port/raspi

BTSTACK_RASPI_SOURCES = \
	btstack_control_raspi.c \
	raspi_get_model.c \
	main.c

BTSTACK_SOURCES = \
	btstack_chipset_bcm.c \
	btstack_chipset_bcm_download_firmware.c \
	btstack_link_key_db_tlv.c \
	btstack_run_loop_posix.c \
	btstack_tlv.c 		\
	btstack_util.c 		\
	btstack_tlv_posix.c \
	btstack_uart_block_posix.c \
	btstack_slip.c \
	hci_transport_h4.c \
	hci_transport_h5.c \
	le_device_db_tlv.c \
	rijndael.c \
	btstack_memory.c            \
	btstack_linked_list.c	    \
	btstack_memory_pool.c       \
	btstack_run_loop.c		    \
	ad_parser.c                 \
	hci.c			            \
	hci_cmd.c		            \
	hci_dump.c		            \
	l2cap.c			            \
	l2cap_signaling.c	        \
	btstack_audio.c             \
	btstack_tlv.c               \
	btstack_crypto.c            \
	uECC.c                      \
	sm.c                        \
	hci.c			            \
	device_id_server.c          \
	ad_parser.c                 \
	hci_cmd.c		            \
	hci_dump.c		            \
	l2cap.c			            \
	l2cap_signaling.c	        \
	rfcomm.c                    \
	btstack_run_loop_posix.c 	\
	btstack_stdin_posix.c 	\
	btstack_link_key_db_tlv.c 	\
	le_device_db_tlv.c 			\
	wav_util.c 					\
	sdp_util.c	                \
	sdp_server.c		        \
	sdp_client.c 				\
	sdp_client_rfcomm.c 		\
	sm.c 						\
	uECC.c 					    \
	spp_server.c 				\
	uECC.c						\
	att_dispatch.c       	    \
	att_db.c 				 	\
	att_server.c        	    \
	sm.c 				 	    \
	battery_service_server.c    \
	device_information_service_server.c \
	btstack_ring_buffer.c

BTSTACK_OBJECTS = $(BTSTACK_SOURCES:.c=.o)
BTSTACK_RASPI_OBJECTS = $(BTSTACK_RASPI_SOURCES:.c=.o)

CFLAGS += \
	-I${BTSTACK_ROOT}/port/raspi \
	-I${BTSTACK_ROOT}/src/ble \
	-I${BTSTACK_ROOT}/src \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/bcm 
CFLAGS += -Wall -Werror

CC = arm-linux-gnueabihf-gcc

default_target: all

libbtstack.so: $(BTSTACK_OBJECTS)
	gcc -o $@ $^ -shared

tlv_path:
	sed -i 's/^#define TLV_DB_PATH_PREFIX .*/#define TLV_DB_PATH_PREFIX ".\/btstack_"/g' ${BTSTACK_ROOT}/port/raspi/main.c

libbtstack_raspi.a: $ $(BTSTACK_RASPI_OBJECTS)
	ar rcs $@ $^

all: tlv_path libbtstack.so libbtstack_raspi.a

.PHONY: clean
clean:
	rm -f *.o
	rm -f *.a
	rm -f *.so

.PHONY: install
install: all
	install -m 644 libbtstack.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 libbtstack_raspi.a $(DESTDIR)$(PREFIX)/lib/
	$(LDCONFIG) $(DESTDIR)${libdir}
