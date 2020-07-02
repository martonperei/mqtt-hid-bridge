#include <stdint.h>
#include <btstack.h>

/**
 * Setup method for the peripheral.
 */
static void ble_hid_setup(void);

/**
 * Changes the peripheral state and logs this change
 * @param new_state The new state the peripheral should take.
 */
static void ble_hid_change_state(int new_state);


/**
 * Handles received bluetooth packets (callback function).
 * @param packet_type The type of packet we received.
 * @param channel On which channeld did we receive it.
 * @param packet The actual packet.
 * @param size The size of the received packet.
 */
static void ble_hid_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void ble_hid_buffer_poll_handler(btstack_timer_source_t * ts);

static int mqtt_setup(void);
static int mqtt_teardown(void);

static int mqtt_message_arrived(void *context, char *topic_name, int topic_len, MQTTAsync_message *message);
static void mqtt_on_connection_lost(void *context, char *cause);
static void mqtt_on_connect_failed(void* context, MQTTAsync_failureData* response);
static void mqtt_on_connected(void *context, char *cause);

static void mqtt_on_subscribe(void* context, MQTTAsync_successData* response);
static void mqtt_on_subscribe_failed(void* context, MQTTAsync_failureData* response);