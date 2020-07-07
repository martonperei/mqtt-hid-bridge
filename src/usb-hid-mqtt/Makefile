ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

MQTT_ROOT ?= ../../lib/mqtt

BIN := usb-hid-mqtt

DEPDIR := .deps
OBJDIR := .objs

SRCS := \
	usb-hid-mqtt.c			\

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

CC := gcc
LD := gcc

CFLAGS := -std=gnu99 -Wall -Werror \
	-I${MQTT_ROOT}/src

LDFLAGS :=
LDLIBS :=  -lusb-1.0 -lpthread -ludev -lpaho-mqtt3as
DEPFLAGS = -MT $@ -MD -MP -MF $(DEPDIR)/$*.Td

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@
LINK.o = $(LD) $(LDFLAGS) $(LDLIBS) -o $@
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
	$(LINK.o) $^

$(OBJDIR)/%.o: %.c 
$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d | $(DEPDIR) $(OBJDIR)
	$(PRECOMPILE)
	$(COMPILE.c) $<
	$(POSTCOMPILE)

$(DEPDIR): ; @mkdir -p $@
$(OBJDIR): ; @mkdir -p $@

.PRECIOUS: $(DEPDIR)/%.d
$(DEPDIR)/%.d: ;

-include $(DEPS)