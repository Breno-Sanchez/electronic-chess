#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led.h"
#include "sensor.h"
#include "server.h"

#define SENSOR_QUEUE_LEN              (4U)
#define LED_QUEUE_LEN                 (4U)

#define SENSOR_TASK_STACK_WORDS       (3072U)
#define SERVER_TASK_STACK_WORDS       (8192U)
#define LED_TASK_STACK_WORDS          (4096U)

#define SENSOR_TASK_PRIORITY          (5U)
#define SERVER_TASK_PRIORITY          (6U)
#define LED_TASK_PRIORITY             (5U)

static const char * const TAG = "MAIN";

static QueueHandle_t sensorQueue = NULL;
static QueueHandle_t ledQueue = NULL;

void app_main(void)
{
    esp_err_t err;
    BaseType_t status;

    sensorQueue = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(sensor_event_t));
    ledQueue = xQueueCreate(LED_QUEUE_LEN, sizeof(led_command_t));

    if ((sensorQueue == NULL) || (ledQueue == NULL))
    {
        ESP_LOGE(TAG, "Queue creation failed");
    }
    else
    {
        err = ledInit(ledQueue);

        if (err == ESP_OK)
        {
            err = sensorInit(sensorQueue);
        }

        if (err == ESP_OK)
        {
            err = serverInit(sensorQueue, ledQueue);
        }

        if (err == ESP_OK)
        {
            err = serverNetworkInit();
        }

        if (err == ESP_OK)
        {
            status = xTaskCreate(
                ledTask,
                "led_task",
                LED_TASK_STACK_WORDS,
                NULL,
                LED_TASK_PRIORITY,
                NULL
            );

            if (status != pdPASS)
            {
                err = ESP_FAIL;
            }
        }

        if (err == ESP_OK)
        {
            status = xTaskCreate(
                serverTask,
                "server_task",
                SERVER_TASK_STACK_WORDS,
                NULL,
                SERVER_TASK_PRIORITY,
                NULL
            );

            if (status != pdPASS)
            {
                err = ESP_FAIL;
            }
        }

        if (err == ESP_OK)
        {
            status = xTaskCreate(
                sensorTask,
                "sensor_task",
                SENSOR_TASK_STACK_WORDS,
                NULL,
                SENSOR_TASK_PRIORITY,
                NULL
            );

            if (status != pdPASS)
            {
                err = ESP_FAIL;
            }
        }

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "System started");
        }
        else
        {
            ESP_LOGE(TAG, "Startup failed: %ld", (long)err);
        }
    }
}
