#pragma once

void wifi_init(void);
int wifi_connect(char *ssid, char *psk);
int wifi_get_status(void);
