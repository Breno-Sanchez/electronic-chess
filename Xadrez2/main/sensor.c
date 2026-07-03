#include <stdint.h>
#include <stdbool.h>

#include "app_types.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sensor.h"

#define NUM_LINHAS 8
#define NUM_COLUNAS 8

#define SENSOR_POLL_MS          (50U)
#define COLUMN_SETTLE_MS        (1U)

static const char * const TAG = "SENSOR";

static QueueHandle_t sensorQueue = NULL;

static const gpio_num_t pinosLinha[NUM_LINHAS] = {
    GPIO_NUM_4,   // Linha 1
    GPIO_NUM_5,   // Linha 2
    GPIO_NUM_6,   // Linha 3
    GPIO_NUM_7,   // Linha 4
    GPIO_NUM_8,   // Linha 5
    GPIO_NUM_9,   // Linha 6
    GPIO_NUM_10,  // Linha 7
    GPIO_NUM_11   // Linha 8
};

static const gpio_num_t pinosColuna[NUM_COLUNAS] = {
    GPIO_NUM_12,  // Coluna A
    GPIO_NUM_13,  // Coluna B
    GPIO_NUM_14,  // Coluna C
    GPIO_NUM_15,  // Coluna D
    GPIO_NUM_16,  // Coluna E
    GPIO_NUM_17,  // Coluna F
    GPIO_NUM_18,  // Coluna G
    GPIO_NUM_21   // Coluna H
};

static bool sensorAnterior[NUM_LINHAS][NUM_COLUNAS] = {false};
static uint32_t sequencia_global = 1U;

static void setSquare(char dst[APP_SQUARE_TEXT_LEN], char file, char rank)
{
    dst[0] = file;
    dst[1] = rank;
    dst[2] = '\0';
}

static void disableAllColumns(void)
{
    for (int c = 0; c < NUM_COLUNAS; c++)
    {
        gpio_set_level(pinosColuna[c], 0);
    }
}

static void scanMatrix(bool estado[NUM_LINHAS][NUM_COLUNAS])
{
    disableAllColumns();

    for (int c = 0; c < NUM_COLUNAS; c++)
    {
        gpio_set_level(pinosColuna[c], 1);
        vTaskDelay(pdMS_TO_TICKS(COLUMN_SETTLE_MS));

        for (int l = 0; l < NUM_LINHAS; l++)
        {
            estado[l][c] = (gpio_get_level(pinosLinha[l]) == 1);
        }

        gpio_set_level(pinosColuna[c], 0);
    }
}

esp_err_t sensorInit(QueueHandle_t queue)
{
    sensorQueue = queue;

    if (sensorQueue == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int l = 0; l < NUM_LINHAS; l++)
    {
        gpio_config_t configLin = {
            .pin_bit_mask = (1ULL << pinosLinha[l]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        gpio_config(&configLin);
    }

    for (int c = 0; c < NUM_COLUNAS; c++)
    {
        gpio_config_t configCol = {
            .pin_bit_mask = (1ULL << pinosColuna[c]),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        gpio_config(&configCol);
        gpio_set_level(pinosColuna[c], 0);
    }

    bool estadoInicial[NUM_LINHAS][NUM_COLUNAS] = {false};
    scanMatrix(estadoInicial);

    for (int l = 0; l < NUM_LINHAS; l++)
    {
        for (int c = 0; c < NUM_COLUNAS; c++)
        {
            sensorAnterior[l][c] = estadoInicial[l][c];
        }
    }

    ESP_LOGI(TAG, "Matriz 8x8 inicializada para ESP32-S3");
    ESP_LOGI(TAG, "Linhas: GPIO4 GPIO5 GPIO6 GPIO7 GPIO8 GPIO9 GPIO10 GPIO11");
    ESP_LOGI(TAG, "Colunas: GPIO12 GPIO13 GPIO14 GPIO15 GPIO16 GPIO17 GPIO18 GPIO21");

    return ESP_OK;
}

void sensorTask(void * parameters)
{
    (void)parameters;

    bool estadoAtual[NUM_LINHAS][NUM_COLUNAS] = {false};

    for (;;)
    {
        scanMatrix(estadoAtual);

        for (int l = 0; l < NUM_LINHAS; l++)
        {
            for (int c = 0; c < NUM_COLUNAS; c++)
            {
                if (estadoAtual[l][c] != sensorAnterior[l][c])
                {
                    sensorAnterior[l][c] = estadoAtual[l][c];

                    sensor_event_t event;
                    event.sequence = sequencia_global++;
                    event.state = estadoAtual[l][c] ? SENSOR_STATE_PRESENT : SENSOR_STATE_LIFTED;

                    setSquare(event.square, (char)('a' + c), (char)('8' - l));

                    xQueueSend(sensorQueue, &event, 0);

                    ESP_LOGI(
                        TAG,
                        "Casa %s: %s",
                        event.square,
                        estadoAtual[l][c] ? "PRESENTE" : "REMOVIDA"
                    );
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_MS));
    }
}
