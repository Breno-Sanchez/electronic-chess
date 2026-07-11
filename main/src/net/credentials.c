#include <stddef.h>
#include <string.h>

#include "credentials.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#define CREDENTIALS_NVS_NAMESPACE "wifi_enter"

static const char * const TAG = "CREDENTIALS";

static esp_err_t readString(nvs_handle_t handle, const char * key, char * dst, size_t dstLen, size_t maxLen)
{
    esp_err_t err;
    size_t requiredLen = dstLen;

    if ((key == NULL) || (dst == NULL) || (dstLen == 0U) || (maxLen >= dstLen))
    {
        return ESP_ERR_INVALID_ARG;
    }

    dst[0] = '\0';
    err = nvs_get_str(handle, key, dst, &requiredLen);

    if (err == ESP_OK)
    {
        if ((requiredLen == 0U) || (requiredLen > (maxLen + 1U)))
        {
            dst[0] = '\0';
            err = ESP_ERR_INVALID_SIZE;
        }
    }

    return err;
}

esp_err_t credentialsLoad(wifi_enterprise_credentials_t * credentials)
{
    nvs_handle_t handle = 0;
    esp_err_t err;
    uint8_t opened = 0U;

    if (credentials == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    (void)memset(credentials, 0, sizeof(*credentials));

    err = nvs_open(CREDENTIALS_NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err == ESP_OK)
    {
        opened = 1U;
    }

    if (err == ESP_OK)
    {
        err = readString(handle, "ssid", credentials->ssid, sizeof(credentials->ssid), WIFI_ENTERPRISE_SSID_MAX_LEN);
    }

    if (err == ESP_OK)
    {
        err = readString(handle, "identity", credentials->identity, sizeof(credentials->identity), WIFI_ENTERPRISE_EAP_MAX_LEN);
    }

    if (err == ESP_OK)
    {
        err = readString(handle, "username", credentials->username, sizeof(credentials->username), WIFI_ENTERPRISE_EAP_MAX_LEN);
    }

    if (err == ESP_OK)
    {
        err = readString(handle, "password", credentials->password, sizeof(credentials->password), WIFI_ENTERPRISE_EAP_MAX_LEN);
    }

    if (err == ESP_OK)
    {
        credentials->valid = 1U;
        ESP_LOGI(TAG, "WPA2 Enterprise credentials loaded from NVS");
    }
    else
    {
        (void)memset(credentials, 0, sizeof(*credentials));
        ESP_LOGW(TAG, "WPA2 Enterprise credentials are not provisioned: %ld", (long)err);
    }

    if (opened != 0U)
    {
        nvs_close(handle);
    }

    return err;
}
