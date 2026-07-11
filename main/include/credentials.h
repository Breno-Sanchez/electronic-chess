#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include <stdint.h>

#include "esp_err.h"

#define WIFI_ENTERPRISE_SSID_MAX_LEN      (32U)
#define WIFI_ENTERPRISE_EAP_MAX_LEN       (127U)
#define WIFI_ENTERPRISE_FIELD_STORAGE_LEN (WIFI_ENTERPRISE_EAP_MAX_LEN + 1U)

typedef struct
{
    char ssid[WIFI_ENTERPRISE_SSID_MAX_LEN + 1U];
    char identity[WIFI_ENTERPRISE_FIELD_STORAGE_LEN];
    char username[WIFI_ENTERPRISE_FIELD_STORAGE_LEN];
    char password[WIFI_ENTERPRISE_FIELD_STORAGE_LEN];
    uint8_t valid;
} wifi_enterprise_credentials_t;

esp_err_t credentialsLoad(wifi_enterprise_credentials_t * credentials);

#endif
