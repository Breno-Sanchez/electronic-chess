#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_types.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sensor.h"

#define MATRIX_RANK_COUNT          (8U)
#define MATRIX_FILE_COUNT          (8U)

#define SENSOR_POLL_MS             (80U)
#define DISCHARGE_DELAY_MS         (3U)
#define COLUMN_SETTLE_MS           (5U)
#define SAMPLE_COUNT               (5U)
#define SAMPLE_DELAY_MS            (1U)
#define REQUIRED_HIGH_COUNT        (3U)

static const char * const TAG = "SENSOR";

static QueueHandle_t sensorQueue = NULL;
static bool previousState[MATRIX_RANK_COUNT][MATRIX_FILE_COUNT] = {false};
static uint32_t globalSequence = 1U;
static bool initialEventsSent = false;

static const gpio_num_t rowPins[MATRIX_RANK_COUNT] = {
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_8,
    GPIO_NUM_9,
    GPIO_NUM_10,
    GPIO_NUM_11
};

static const gpio_num_t columnPins[MATRIX_FILE_COUNT] = {
    GPIO_NUM_12,
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_15,
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
    GPIO_NUM_21
};

static void setSquare(char dst[APP_SQUARE_TEXT_LEN], uint32_t row, uint32_t column)
{
    if (dst != NULL)
    {
        dst[0] = (char)('a' + (char)column);
        dst[1] = (char)('1' + (char)row);
        dst[2] = '\0';
    }
}

static void disableAllColumns(void)
{
    for (uint32_t column = 0U; column < MATRIX_FILE_COUNT; column++)
    {
        (void)gpio_set_level(columnPins[column], 0);
    }
}

static bool readRowFiltered(uint32_t row)
{
    uint32_t highCount = 0U;

    for (uint32_t sample = 0U; sample < SAMPLE_COUNT; sample++)
    {
        if (gpio_get_level(rowPins[row]) != 0)
        {
            highCount++;
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }

    return (highCount >= REQUIRED_HIGH_COUNT);
}

static void scanMatrix(bool state[MATRIX_RANK_COUNT][MATRIX_FILE_COUNT])
{
    (void)memset(state, 0, sizeof(bool) * MATRIX_RANK_COUNT * MATRIX_FILE_COUNT);

    for (uint32_t column = 0U; column < MATRIX_FILE_COUNT; column++)
    {
        disableAllColumns();
        vTaskDelay(pdMS_TO_TICKS(DISCHARGE_DELAY_MS));

        (void)gpio_set_level(columnPins[column], 1);
        vTaskDelay(pdMS_TO_TICKS(COLUMN_SETTLE_MS));

        for (uint32_t row = 0U; row < MATRIX_RANK_COUNT; row++)
        {
            state[row][column] = readRowFiltered(row);
        }

        (void)gpio_set_level(columnPins[column], 0);
    }

    disableAllColumns();
}

static void sendSensorEvent(uint32_t row, uint32_t column, bool present)
{
    sensor_event_t event;

    event.sequence = globalSequence++;
    event.state = present ? SENSOR_STATE_PRESENT : SENSOR_STATE_LIFTED;
    setSquare(event.square, row, column);

    (void)xQueueSend(sensorQueue, &event, portMAX_DELAY);

    ESP_LOGI(
        TAG,
        "Square %s: %s",
        event.square,
        present ? "PRESENT" : "REMOVED"
    );
}

esp_err_t sensorInit(QueueHandle_t queue)
{
    esp_err_t err = ESP_OK;

    sensorQueue = queue;

    if (sensorQueue == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t row = 0U; row < MATRIX_RANK_COUNT; row++)
    {
        gpio_config_t rowConfig = {
            .pin_bit_mask = (1ULL << rowPins[row]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        err = gpio_config(&rowConfig);

        if (err != ESP_OK)
        {
            return err;
        }
    }

    for (uint32_t column = 0U; column < MATRIX_FILE_COUNT; column++)
    {
        gpio_config_t columnConfig = {
            .pin_bit_mask = (1ULL << columnPins[column]),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        err = gpio_config(&columnConfig);

        if (err != ESP_OK)
        {
            return err;
        }

        (void)gpio_set_level(columnPins[column], 0);
    }

    (void)memset(previousState, 0, sizeof(previousState));

    ESP_LOGI(TAG, "8x8 reed matrix initialized for ESP32-S3");
    ESP_LOGI(TAG, "Rows 1..8: GPIO4 GPIO5 GPIO6 GPIO7 GPIO8 GPIO9 GPIO10 GPIO11");
    ESP_LOGI(TAG, "Columns A..H: GPIO12 GPIO13 GPIO14 GPIO15 GPIO16 GPIO17 GPIO18 GPIO21");
    ESP_LOGI(TAG, "Mapping: row 1 -> rank 1, row 8 -> rank 8");

    return ESP_OK;
}

void sensorTask(void * parameters)
{
    bool currentState[MATRIX_RANK_COUNT][MATRIX_FILE_COUNT] = {false};

    (void)parameters;

    for (;;)
    {
        scanMatrix(currentState);

        if (initialEventsSent == false)
        {
            for (uint32_t row = 0U; row < MATRIX_RANK_COUNT; row++)
            {
                for (uint32_t column = 0U; column < MATRIX_FILE_COUNT; column++)
                {
                    previousState[row][column] = currentState[row][column];

                    if (currentState[row][column] == true)
                    {
                        sendSensorEvent(row, column, true);
                    }
                }
            }

            initialEventsSent = true;
            vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_MS));
            continue;
        }

        for (uint32_t row = 0U; row < MATRIX_RANK_COUNT; row++)
        {
            for (uint32_t column = 0U; column < MATRIX_FILE_COUNT; column++)
            {
                if (currentState[row][column] != previousState[row][column])
                {
                    previousState[row][column] = currentState[row][column];
                    sendSensorEvent(row, column, currentState[row][column]);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_MS));
    }
}
