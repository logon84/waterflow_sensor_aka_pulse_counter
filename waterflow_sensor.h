
//OTA settings
#define OTA
const char* ota_password = "update_waterflow";

// wifi settings
const char* ssid     = "wifi_ssid_here";
const char* password = "wifi_password_here";

// mqtt server settings
const char* mqtt_broker   = "mqtt_broker_here";
const int mqtt_port       = 1883;
const char* mqtt_username = "mqtt_user_here";
const char* mqtt_password = "mqtt_password_here";

// mqtt client settings
// Note PubSubClient.h has a MQTT_MAX_PACKET_SIZE of 128 defined, so either raise it to 256 or use short topics
const char* client_id     = "waterflow"; // Must be unique on the MQTT network
const char* dest_topic    = "waterflow";

// input pinouts
const int PULSE_PIN  = 3;

// sketch settings
const unsigned int DEBOUNCE_MS = 100;
const unsigned int LED_FLICKER_MS = 200;
const float WATERMETER_RESOLUTION_M3 = 0.0001;
