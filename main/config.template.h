#pragma once

#define MQTT_URI "scheme://username:password@hostname:port/path"
#define MQTT_CLIENT_ID "xecut-lock"
#define MQTT_RECONNECT_DELAY_SEC 10

// Uncomment this to disable ethernet module and use wifi.
// #define USE_WIFI

#ifdef USE_WIFI
#define WIFI_SSID "SSID"
#define WIFI_PSK  "PASSWORD"
#define WIFI_RECONNECT_DELAY 10
#endif
