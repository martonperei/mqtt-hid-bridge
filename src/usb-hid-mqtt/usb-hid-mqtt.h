#include <stdint.h>
#include <libusb-1.0/libusb.h>
#include <MQTTAsync.h>

static int usb_setup(void);
static int usb_teardown(void);

static void usb_callback(struct libusb_transfer *transfer);
static void usb_event_poll(void);
static int usb_hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                                libusb_hotplug_event event, void *user_data);

static int mqtt_setup(void);
static int mqtt_teardown(void);

static int mqtt_message_arrived(void *context, char *topic_name, int topic_len, MQTTAsync_message *message);
static void mqtt_on_connection_lost(void *context, char *cause);
static void mqtt_on_connect_failed(void* context, MQTTAsync_failureData* response);
static void mqtt_on_connected(void *context, char *cause);