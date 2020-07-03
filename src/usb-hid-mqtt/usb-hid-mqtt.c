#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "usb-hid-mqtt.h"

#define UNUSED(x) (void)(x)

/**
 * MQTT variables
 */
#define MQTT_CLIENTID    "hid-mqtt-client"
#define MQTT_QOS         1

static MQTTAsync mqtt_client;
static const char *mqtt_topic;
static const char *mqtt_address;
static const char *mqtt_username;
static const char *mqtt_password;

/**
 * USB variables
 */
#define USB_CONFIG_INDEX        0
#define USB_INTERFACE_INDEX     2
#define USB_ALT_SETTING_INDEX   0
#define USB_ENDPOINT_INDEX      0
#define USB_VENDOR_ID           0x46d
#define USB_PRODUCT_ID          0xc52b

static libusb_context *usb_ctx;
static libusb_device_handle *usb_device_handle;
static const struct libusb_interface *usb_iface;
static const struct libusb_interface_descriptor *usb_iface_desc;
static libusb_hotplug_callback_handle usb_callback_handle;
struct libusb_transfer *usb_transfer_in = NULL;
static struct timeval usb_timeout = {
    .tv_sec = 10,
    .tv_usec = 0
};

static uint8_t usb_in_buffer[1024*8];

/**
 * MQTT
 */
static void mqtt_on_connection_lost(void *context, char *cause) {
    printf("[MQTT] connection lost, cause: %s\n", cause);

    printf("[MQTT] reconnecting...\n");

    int rc = MQTTAsync_reconnect((MQTTAsync) context);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] reconnect failed, rc: %d\n", rc);
    }
}

static void mqtt_on_connect_failed(void* context, MQTTAsync_failureData* response) {
    printf("[MQTT] connect failed, rc: %d\n", response ? response->code : 0);
}

static void mqtt_on_connected(void *context, char *cause) {
    printf("[MQTT] connected\n");
}

static int mqtt_message_arrived(void* context, char* topic_name, int topic_len, MQTTAsync_message* m) {
    return 1;
}

static int mqtt_setup(void) {
    printf("[MQTT] creating...\n");

    int rc = MQTTAsync_create(&mqtt_client, mqtt_address, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] create failed, rc: %d\n", rc);
        return rc;
    }

    rc = MQTTAsync_setCallbacks(mqtt_client, mqtt_client, mqtt_on_connection_lost, mqtt_message_arrived, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] setCallbacks failed, rc: %d\n", rc);
        return rc;
    }

    rc = MQTTAsync_setConnected(mqtt_client, mqtt_client, mqtt_on_connected);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] setConnected failed, rc: %d\n", rc);
        return rc;
    }

    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.context = mqtt_client;
    conn_opts.username = mqtt_username;
    conn_opts.password = mqtt_password;
    conn_opts.onFailure = mqtt_on_connect_failed;
    conn_opts.automaticReconnect = 1;

    printf("[MQTT] connecting...\n");
    rc = MQTTAsync_connect(mqtt_client, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] connect failed, rc: %d\n", rc);
        return rc;
    }

    return rc;
}

static int mqtt_teardown(void) {
    printf("[MQTT] disconnecting...\n");

    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    opts.context = mqtt_client;

    int rc = MQTTAsync_disconnect(mqtt_client, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] disconnect failed, rc: %d\n", rc);
        return rc;
    }

    MQTTAsync_destroy(&mqtt_client);

    return 0;
}

/**
 * USB 
 */
static void usb_callback(struct libusb_transfer *transfer) {
    if (transfer->actual_length > 0 && 
        transfer->buffer[0] == 0x20 &&
        (transfer->buffer[2] == 0x01 || transfer->buffer[2] == 0x03)) {
        printf("[USB] Frame ");
        for(int i = 0; i< transfer->actual_length; i++) {
            printf("%#x ", transfer->buffer[i]);
        }
        printf("\n");

        if (MQTTAsync_isConnected(mqtt_client)) {
            MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
            MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
            opts.context = mqtt_client;
            pubmsg.payload = transfer->buffer;
            pubmsg.payloadlen = transfer->actual_length;
            pubmsg.qos = MQTT_QOS;
            pubmsg.retained = 0;

            int rc = MQTTAsync_sendMessage(mqtt_client, mqtt_topic, &pubmsg, &opts);
            if (rc != MQTTASYNC_SUCCESS) {
                printf("[MQTT] sendMessage failed, rc: %d\n", rc);
            }
        } else {
            printf("[USB] Received frame, but MQTT is not connected\n");
        }
    }

    int rc = libusb_submit_transfer(transfer);

    if (rc != LIBUSB_SUCCESS) {
        printf("[USB] submit transfer failed, rc: %d\n", rc);
    }
}

static void usb_event_poll(void) {
    int rc = libusb_handle_events_timeout_completed(usb_ctx, &usb_timeout, NULL);
    if (rc != LIBUSB_SUCCESS) {
        printf("[USB] handle events failed, rc: %d\n", rc);
    }
}

static int usb_hotplug_callback(struct libusb_context *ctx, struct libusb_device *device,
                     libusb_hotplug_event event, void *user_data) {
    int rc;
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config;

    (void)libusb_get_device_descriptor(device, &desc);

    if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
        rc = libusb_open(device, &usb_device_handle);
        if (LIBUSB_SUCCESS != rc) {
            printf("[USB] could not open harmony device\n");
        } else {
            printf("[USB] device found\n");

            libusb_get_config_descriptor(device, USB_CONFIG_INDEX, &config);
            usb_iface = &config->interface[USB_INTERFACE_INDEX];
            usb_iface_desc = &usb_iface->altsetting[USB_ALT_SETTING_INDEX];
        
            // Detach & claim interface from kernel driver
            rc = libusb_detach_kernel_driver(usb_device_handle, USB_INTERFACE_INDEX);
            if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_NOT_FOUND) {
                printf("[USB] detach kernel driver failed, rc: %d\n", rc);
                return rc;
            }

            rc = libusb_claim_interface(usb_device_handle, USB_INTERFACE_INDEX);
            if (rc != LIBUSB_SUCCESS) {
                printf("[USB] claim interface failed, rc: %d\n", rc);
                return rc;
            }

            usb_transfer_in = libusb_alloc_transfer(0);
            libusb_fill_interrupt_transfer(usb_transfer_in, usb_device_handle, 
                usb_iface_desc->endpoint[USB_ENDPOINT_INDEX].bEndpointAddress,
                usb_in_buffer, sizeof(usb_in_buffer),
                usb_callback, NULL, 0);

            rc = libusb_submit_transfer(usb_transfer_in);
            if (rc != LIBUSB_SUCCESS) {
                printf("[USB] submit transfer failed, rc: %d\n", rc);
                return rc;
            }

            printf("[USB] transfer initialized\n");
        }
    } else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
        if (usb_device_handle) {
            printf("[USB] device removed\n");
            libusb_cancel_transfer(usb_transfer_in);
            libusb_free_transfer(usb_transfer_in);
            libusb_release_interface(usb_device_handle, USB_INTERFACE_INDEX);
            libusb_attach_kernel_driver(usb_device_handle, USB_INTERFACE_INDEX);
            libusb_close(usb_device_handle);
            usb_device_handle = NULL;
        }
    } else {
        printf("[USB] unhandled event %d\n", event);
    }

  return 0;
}

static int usb_setup(void) {
    int rc;
 
    printf("[USB] initializing...\n");
 
    libusb_init(&usb_ctx);
    libusb_set_option(usb_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
 
    rc = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 
                                        LIBUSB_HOTPLUG_ENUMERATE, USB_VENDOR_ID, USB_PRODUCT_ID, LIBUSB_HOTPLUG_MATCH_ANY, 
                                        usb_hotplug_callback, NULL, &usb_callback_handle);
    if (LIBUSB_SUCCESS != rc) {
        printf("[USB] hotplug register failed, rc: %d\n", rc);
        return rc;
    }

    return LIBUSB_SUCCESS;
}

static int usb_teardown(void) {
    printf("[USB] closing..\n");

    libusb_hotplug_deregister_callback(NULL, usb_callback_handle);

    libusb_exit(usb_ctx);

    return 0;
}

int main(int argc, char* argv[]) {
    int opt; 
      
    while((opt = getopt(argc, argv, "a:t:u:p:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'a':  
                mqtt_address = optarg;
                break;  
            case 't':  
                mqtt_topic = optarg;
                break;  
            case 'u':  
                mqtt_username = optarg;
                break;  
            case 'p':  
                mqtt_password = optarg;
                break;  
            case '?':  
                printf("unknown option: %c\n", optopt); 
                break;  
        }  
    }  

    int rc;

    rc = mqtt_setup();
    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] mqtt setup failed, rc: %d\n", rc);
        return rc;
    }

    rc = usb_setup();
    if (rc != LIBUSB_SUCCESS) {
        printf("[USB] usb setup failed, rc: %d\n", rc);
        return rc;
    }

    while (1) {
        usb_event_poll();
    }

    usb_teardown();
    mqtt_teardown();

    return rc;
}
