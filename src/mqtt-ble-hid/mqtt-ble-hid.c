#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include "MQTTAsync.h"

#include "mqtt-ble-hid.h"

#include "btstack.h"
#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"

#include "hids_device_remote.h"
#include "mqtt-ble-hid-services.h"

/**
 * HID buffer
 */
static uint8_t storage_hid_reports[128];
static btstack_ring_buffer_t buffer_hid_reports;
static pthread_mutex_t buffer_lock;

#define HID_REPORT_KEYBOARD 0x01
#define HID_REPORT_CONSUMER 0x03
#define HID_REPORT_KEYBOARD_KEY_COUNT 6
#define HID_REPORT_CONSUMER_KEY_COUNT 4

/**
 * MQTT variables
 */
#define MQTT_CLIENTID    "mqtt-ble-hid-client"
#define MQTT_QOS         1

static MQTTAsync mqtt_client;
static const char *mqtt_topic;
static const char *mqtt_address;
static const char *mqtt_username;
static const char *mqtt_password;

/**
 * BLE HID variables
 */
#define BLE_HID_STATE_OFFLINE 0
#define BLE_HID_STATE_ONLINE 1
#define BLE_HID_STATE_CONNECTED 2
#define BLE_HID_STATE_IDLE 3
#define BLE_HID_STATE_BUSY 4

#define BLE_HID_BUFFER_POLL_INTERVAL 50

static hci_con_handle_t ble_con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t ble_hid_state = BLE_HID_STATE_OFFLINE;
static btstack_packet_callback_registration_t ble_hci_event_callback_registration;
static btstack_packet_callback_registration_t ble_sm_event_callback_registration;
static uint8_t ble_hid_protocol_mode = 1;
static btstack_timer_source_t ble_hid_buffer_poll_timer;

static const uint8_t ble_adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Name
    0x12, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'M','Q', 'T', 'T', ' ', 'H', 'I', 'D', ' ', 'B', 'r', 'i', 'd', 'g', 'e',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8,
    // Appearance (remote),
    0x03, 0x19, 0x80, 0x01,
};
static const uint8_t ble_adv_data_len = sizeof(ble_adv_data);

// from USB HID Specification 1.1, Appendix B.1
static const uint8_t ble_hid_descriptor_boot_mode[] = {
    /*
     Keyboard
     */
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xa1, 0x01,                    // Collection (Application)

    0x85, 0x01,                    // Report ID 1

    // Modifier byte

    0x75, 0x01,                    //     Report Size (1)
    0x95, 0x08,                    //     Report Count (8)
    0x05, 0x07,                    //     Usage Page (Key codes)
    0x19, 0xE0,                    //     Usage Minimum (Keyboard LeftControl)
    0x29, 0xE7,                    //     Usage Maxium (Keyboard Right GUI)
    0x15, 0x00,                    //     Logical Minimum (0)
    0x25, 0x01,                    //     Logical Maximum (1)
    0x81, 0x02,                    //     Input (Data, Variable, Absolute)

    // Reserved byte

    0x75, 0x01,                    //     Report Size (1)
    0x95, 0x08,                    //     Report Count (8)
    0x81, 0x03,                    //     Input (Constant, Variable, Absolute)

    // LED report + padding

    0x95, 0x05,                    //     Report Count (5)
    0x75, 0x01,                    //     Report Size (1)
    0x05, 0x08,                    //     Usage Page (LEDs)
    0x19, 0x01,                    //     Usage Minimum (Num Lock)
    0x29, 0x05,                    //     Usage Maxium (Kana)
    0x91, 0x02,                    //     Output (Data, Variable, Absolute)

    0x95, 0x01,                    //     Report Count (1)
    0x75, 0x03,                    //     Report Size (3)
    0x91, 0x03,                    //     Output (Constant, Variable, Absolute)

    // Keycodes
    0x95, 0x06,                    //     Report Count (6)
    0x75, 0x08,                    //     Report Size (8)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xff,                    //     Logical Maximum (1)
    0x05, 0x07,                    //     Usage Page (Key codes)
    0x19, 0x00,                    //     Usage Minimum (Reserved (no event indicated))
    0x29, 0xff,                    //     Usage Maxium (Reserved)
    0x81, 0x00,                    //     Input (Data, Array)
    0xc0,                          //  End collection
    0x5, 0x0c,
    0x09, 0x01,
    0xa1, 0x01,
    0x85, 0x03,
    0x75, 0x10,
    0x95, 0x02,
    0x15, 0x01,
    0x26, 0xff,
    0x02, 0x19,
    0x01, 0x2a,
    0xff, 0x02,
    0x81, 0x60,
    0xc0,

    // Consumer Control
    0x05, 0x0C,       // (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page
    0x09, 0x01,       // (LOCAL)  USAGE              0x000C0001 Consumer Control
    0xA1, 0x01,       // (MAIN)   COLLECTION         0x01 Application
    0x85, 0x02,       //   (GLOBAL) REPORT_ID          2
    0x19, 0x00,       //   (LOCAL)  USAGE_MINIMUM 
    0x2A, 0x9C, 0x02, //   (LOCAL)  USAGE_MAXIMUM
    0x15, 0x00,       //   (GLOBAL) LOGICAL_MINIMUM 
    0x26, 0x9C, 0x02, //   (GLOBAL) LOGICAL_MAXIMUM    0x029C (668)
    0x95, 0x01,       //   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields
    0x75, 0x10,       //   (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field
    0x81, 0x00,       //   (MAIN)   INPUT 
    0xC0              // (MAIN)   END_COLLECTION     Application
};

static const uint8_t ble_hid_descriptor_boot_mode_len = sizeof(ble_hid_descriptor_boot_mode);

/**
 * Setup method for the peripheral.
 */
static void ble_hid_setup(void) {
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // setup battery service
    battery_service_server_init(100);

    // setup device information service
    device_information_service_server_init();

    // setup HID Device service
    hids_device_init(0, ble_hid_descriptor_boot_mode, ble_hid_descriptor_boot_mode_len);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(ble_adv_data_len, (uint8_t *) ble_adv_data);
    gap_advertisements_enable(1);

    // register for HCI events
    ble_hci_event_callback_registration.callback = &ble_hid_packet_handler;
    hci_add_event_handler(&ble_hci_event_callback_registration);

    // register for SM events
    ble_sm_event_callback_registration.callback = &ble_hid_packet_handler;
    sm_add_event_handler(&ble_sm_event_callback_registration);

    // register for HIDS
    hids_device_register_packet_handler(ble_hid_packet_handler);

    ble_hid_buffer_poll_timer.process = &ble_hid_buffer_poll_handler;
    btstack_run_loop_set_timer(&ble_hid_buffer_poll_timer, BLE_HID_BUFFER_POLL_INTERVAL);
    btstack_run_loop_add_timer(&ble_hid_buffer_poll_timer);
}

/**
 * Changes the peripheral state and logs this change
 * @param new_state The new state the peripheral should take.
 */
static void ble_hid_change_state(int new_state) {
    if (new_state != ble_hid_state) {
        printf("[BLE HID] change state: '%d' to '%d'\n", ble_hid_state, new_state);
        ble_hid_state = new_state;
    }
}

static int ble_hid_is_connected() {
    return (ble_hid_state == BLE_HID_STATE_IDLE || ble_hid_state == BLE_HID_STATE_BUSY) && ble_con_handle != HCI_CON_HANDLE_INVALID;
}

/**
 * Handles received bluetooth packets (callback function).
 * @param packet_type The type of packet we received.
 * @param channel On which channeld did we receive it.
 * @param packet The actual packet.
 * @param size The size of the received packet.
 */
static void ble_hid_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    uint16_t conn_interval;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE: {
                    if ((btstack_event_state_get_state(packet) == HCI_STATE_WORKING) && (ble_hid_state == BLE_HID_STATE_OFFLINE)) {
                        printf("[BLE HID] online\n");

                        ble_hid_change_state(BLE_HID_STATE_ONLINE);

                        mqtt_setup();
                    }

                    if ((btstack_event_state_get_state(packet) == HCI_STATE_HALTING) && (ble_hid_state != BLE_HID_STATE_OFFLINE)) {
                        ble_hid_change_state(BLE_HID_STATE_OFFLINE);

                        mqtt_teardown();
                        
                        printf("[BLE HID] offline\n");
                    }
                    break;
                }
                case SM_EVENT_JUST_WORKS_REQUEST: {
                    printf("[BLE HID] pairing method 'Just Works' requested\n");
                    sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
                    break;
                }
                case SM_EVENT_NUMERIC_COMPARISON_REQUEST: {
                    printf("[BLE HID] confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
                    sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
                    break;
                }
                case SM_EVENT_PASSKEY_DISPLAY_NUMBER: {
                    printf("[BLE HID] passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
                    break;
                }
                case SM_EVENT_PAIRING_COMPLETE: {
                    switch (sm_event_pairing_complete_get_status(packet)) {
                        case ERROR_CODE_SUCCESS: {
                            printf("[BLE HID] pairing complete, success\n");
                            break;
                        }
                        case ERROR_CODE_CONNECTION_TIMEOUT: {
                            printf("[BLE HID] pairing failed, timeout\n");
                            break;
                        }
                        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION: {
                            printf("[BLE HID] pairing failed, disconnected\n");
                            break;
                        }
                        case ERROR_CODE_AUTHENTICATION_FAILURE: {
                            printf("[BLE HID] pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                            break;
                        }
                    }
                    break;
                }
                case HCI_EVENT_LE_META: {
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        // Required for connections after pairing completed (e.g. turn off/turn on device)
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                            ble_con_handle = hids_subevent_input_report_enable_get_con_handle(packet);

                            // print connection parameters (without using float operations)
                            conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                            printf("[BLE HID] client connection complete, interval: %u.%02u ms, latency: %u\n",
                                conn_interval * 125 / 100, 25 * (conn_interval & 3),
                                hci_subevent_le_connection_complete_get_conn_latency(packet));

                            ble_hid_change_state(BLE_HID_STATE_CONNECTED);

                            pthread_mutex_lock(&buffer_lock);
                            btstack_ring_buffer_init(&buffer_hid_reports, storage_hid_reports, sizeof(storage_hid_reports));
                            pthread_mutex_unlock(&buffer_lock);

                            // request min con interval 15 ms for iOS 11+
                            gap_request_connection_parameter_update(ble_con_handle, 12, 12, 0, 0x0048);
                            break;
                        }
                        case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE: {
                            // print connection parameters (without using float operations)
                            conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                            printf("[BLE HID] client connection update, interval: %u.%02u ms, latency: %u\n",
                                conn_interval * 125 / 100, 25 * (conn_interval & 3),
                                hci_subevent_le_connection_update_complete_get_conn_latency(packet));
                            break;
                        }
                    }
                    break;
                }
                case HCI_EVENT_HIDS_META: {
                    switch (hci_event_hids_meta_get_subevent_code(packet)) {
                        case HIDS_SUBEVENT_INPUT_REPORT_ENABLE: {
                            ble_con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
                            printf("[BLE HID] input report enabled: %u\n", hids_subevent_input_report_enable_get_enable(packet));

                            if (ble_hid_state == BLE_HID_STATE_CONNECTED) {
                                ble_hid_change_state(BLE_HID_STATE_IDLE);
                            }
                            break;
                        }
                        case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE: {
                            ble_con_handle = hids_subevent_boot_keyboard_input_report_enable_get_con_handle(packet);
                            printf("[BLE HID] boot keyboard input report enabled: %u\n", hids_subevent_boot_keyboard_input_report_enable_get_enable(packet));
                            break;
                        }
                        case HIDS_SUBEVENT_PROTOCOL_MODE: {
                            ble_hid_protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
                            printf("[BLE HID] protocol mode: %s\n", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
                            break;
                        }
                        case HIDS_SUBEVENT_CAN_SEND_NOW: {
                            printf("[BLE HID] can send now\n");
                            if (ble_hid_state == BLE_HID_STATE_BUSY) {
                                uint32_t num_bytes_read;
                                uint8_t report_id = 0;

                                pthread_mutex_lock(&buffer_lock);

                                btstack_ring_buffer_read(&buffer_hid_reports, &report_id, 1, &num_bytes_read);

                                // initialize the report array
                                uint8_t report_len = 0;
                                if (report_id == HID_REPORT_KEYBOARD) {
                                    report_len = HID_REPORT_KEYBOARD_KEY_COUNT + 3; // modifier + reserved + vendor
                                } else if (report_id == HID_REPORT_CONSUMER) {
                                    report_len = HID_REPORT_CONSUMER_KEY_COUNT;
                                }

                                uint8_t report[report_len];
                                memset(report, 0, report_len*sizeof(uint8_t));

                                // fill up the array
                                if (report_id == HID_REPORT_KEYBOARD) {
                                    //{ /*modifier*/0, /*reserved*/0, /*vendor*/0, /*keys*/0, 0, 0, 0, 0, 0 };
                                    btstack_ring_buffer_read(&buffer_hid_reports, &report[0], 1, &num_bytes_read);
                                    btstack_ring_buffer_read(&buffer_hid_reports, &report[3], HID_REPORT_KEYBOARD_KEY_COUNT, &num_bytes_read);
                                } else if (report_id == HID_REPORT_CONSUMER) {
                                    // { /*keys*/0, 0, 0, 0 };
                                    btstack_ring_buffer_read(&buffer_hid_reports, &report[0], HID_REPORT_CONSUMER_KEY_COUNT, &num_bytes_read);
                                }

                                printf("[BLE HID] frame ");
                                for(int i = 0; i< report_len; i++) {
                                    printf("%#x ", report[i]);
                                }
                                printf("\n");

                                pthread_mutex_unlock(&buffer_lock);

                                // send the report
                                if (report_id == HID_REPORT_KEYBOARD) {
                                    hids_device_send_keyboard_report(ble_con_handle, report, sizeof(report));
                                } else if (report_id == HID_REPORT_CONSUMER) {
                                    hids_device_send_consumer_report(ble_con_handle, report, sizeof(report));
                                }

                                if (!btstack_ring_buffer_empty(&buffer_hid_reports)) {
                                    hids_device_request_can_send_now_event(ble_con_handle);
                                } else {
                                    ble_hid_change_state(BLE_HID_STATE_IDLE);
                                }
                                break;
                            }
                            break;
                        }
                    }
                    break;
                }
                case HCI_EVENT_DISCONNECTION_COMPLETE: {
                    if (ble_hid_state > BLE_HID_STATE_OFFLINE) {
                        ble_con_handle = HCI_CON_HANDLE_INVALID;
                        printf("[BLE HID] disconnection complete\n");

                        ble_hid_change_state(BLE_HID_STATE_ONLINE);
                    }
                    break;
                }
            }
            break;
    }
}

static void ble_hid_buffer_poll_handler(btstack_timer_source_t * ts){
    if (ble_hid_state == BLE_HID_STATE_IDLE && 
        !btstack_ring_buffer_empty(&buffer_hid_reports)) {
        ble_hid_change_state(BLE_HID_STATE_BUSY);

        hids_device_request_can_send_now_event(ble_con_handle);
    }

    btstack_run_loop_set_timer(ts, BLE_HID_BUFFER_POLL_INTERVAL);
    btstack_run_loop_add_timer(ts);
}

/**
 * MQTT
 */
static int mqtt_message_arrived(void *context, char *topic_name, int topic_len, MQTTAsync_message *message) {
    UNUSED(ble_hid_is_connected);

    char* buffer = (char*) message->payload;

    pthread_mutex_lock(&buffer_lock);

    printf("[MQTT] frame ");
    for(int i = 0; i< message->payloadlen; i++) {
        printf("%#x ", buffer[i]);
    }
    printf("\n");

    uint8_t report_id = buffer[2];
    uint8_t report_len = 0;
    if (report_id == HID_REPORT_KEYBOARD) {
        report_len = HID_REPORT_KEYBOARD_KEY_COUNT + 2; // report_id + modifier
    } else if (report_id == HID_REPORT_CONSUMER) {
        report_len = HID_REPORT_CONSUMER_KEY_COUNT + 1; // report_id
    }
    
    btstack_ring_buffer_write(&buffer_hid_reports, (uint8_t*) &buffer[2], report_len); // [0] hid frame, [1] vendor, [2+] keys
    pthread_mutex_unlock(&buffer_lock);

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic_name);
    
    return 1;
}

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

    MQTTAsync client = (MQTTAsync)context;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = mqtt_on_subscribe;
    opts.onFailure = mqtt_on_subscribe_failed;
    opts.context = client;
    int rc = MQTTAsync_subscribe(client, mqtt_topic, MQTT_QOS, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("[MQTT] subscribe failed, rc: %d\n", rc);
    }
}

static void mqtt_on_subscribe(void* context, MQTTAsync_successData* response) {
    printf("[MQTT] subscribed\n");
}

static void mqtt_on_subscribe_failed(void* context, MQTTAsync_failureData* response) {
    printf("[MQTT] subscribe failed: %s, rc: %d\n", response->message, response->code);
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

int btstack_main(int argc, char * argv[]) {
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

    btstack_ring_buffer_init(&buffer_hid_reports, storage_hid_reports, sizeof(storage_hid_reports));

    int rc = pthread_mutex_init(&buffer_lock, NULL);
    if (rc != 0) { 
        printf("[MQTT] buffer mutex init has failed, rc: %d\n", rc); 
        return 1; 
    } 

    ble_hid_setup();

    hci_power_control(HCI_POWER_ON);

    return 0;
}
