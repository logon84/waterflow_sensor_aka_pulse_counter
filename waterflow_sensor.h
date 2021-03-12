
//OTA settings
#define OTA
const char* ota_password = "update_waterflow";

//Clock settings
#define TIME_ZONE TZ_Europe_Madrid

// wifi settings
const char* ssid     = "wifi_ssid_here";
const char* password = "wifi_password_here";

// mqtt server settings
const char* mqtt_broker   = "mqtt_broker_here";
const int mqtt_port       = 1883;
const char* mqtt_username = "mqtt_user_here";
const char* mqtt_password = "mqtt_password_here";

// mqtt client settings
const char* client_id     = "waterflow"; // Must be unique on the MQTT network
const char* dest_topic    = "waterflow";

// pinouts
const int blueLedPin = 1;
const int PULSE_PIN  = 3;

// sketch settings
const unsigned int DEBOUNCE_MS = 300;
const unsigned int LED_FLICKER_MS = 200;
const unsigned int MIN_TIME_BETWEEN_SENDS_MS = 0;
const float RISING_EDGE_LITERS = 0.7;
const float FALLING_EDGE_LITERS = 0.3;
