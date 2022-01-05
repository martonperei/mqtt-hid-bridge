ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

BIN := usb-hid-mqtt

DEPDIR := .deps
OBJDIR := .objs

SRCS := \
	usb-hid-mqtt.c			\

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

CC := gcc
LD := gcc

CFLAGS := -std=gnu99 -Wall -Werror

LDFLAGS :=
LDLIBS := -lusb-1.0 -lpthread -ludev -lpaho-mqtt3as
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
	install -d $(DESTDIR)$(PREFIX)/lib/mqtt-hid-bridge
	install -m 644 $(BIN).service /etc/systemd/system

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/%.o: %.c 
$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d | $(DEPDIR) $(OBJDIR)
	$(PRECOMPILE)
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<
	$(POSTCOMPILE)

$(DEPDIR): ; @mkdir -p $@
$(OBJDIR): ; @mkdir -p $@

.PRECIOUS: $(DEPDIR)/%.d
$(DEPDIR)/%.d: ;

-include $(DEPS)
