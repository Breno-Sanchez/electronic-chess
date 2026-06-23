#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "led.h"
#include "led_strip.h"

#define LED_STRIP_GPIO              (4)
#define LED_STRIP_LED_COUNT         (150U)
#define CHESSBOARD_LED_COUNT        (64U)

#define YELLOW_RED_VALUE            (32U)
#define YELLOW_GREEN_VALUE          (24U)
#define YELLOW_BLUE_VALUE           (0U)

#define GREEN_RED_VALUE             (0U)
#define GREEN_GREEN_VALUE           (48U)
#define GREEN_BLUE_VALUE            (0U)

#define BACKGROUND_RED_VALUE        (0U)
#define BACKGROUND_GREEN_VALUE      (0U)
#define BACKGROUND_BLUE_VALUE       (2U)

#define RMT_RESOLUTION_HZ           (10000000U)
#define RMT_MEM_BLOCK_SYMBOLS       (64U)

static const char * const TAG = "LED";

static QueueHandle_t ledQueue = NULL;
static led_strip_handle_t ledStrip = NULL;

static uint8_t squareToIndex(const char square[APP_SQUARE_TEXT_LEN], uint32_t * index);
static esp_err_t setSquareColor(const char square[APP_SQUARE_TEXT_LEN],
                                uint32_t red,
                                uint32_t green,
                                uint32_t blue);
static esp_err_t setBlueBackground(void);
static esp_err_t applyCommand(const led_command_t * command);

esp_err_t ledInit(QueueHandle_t queue)
{
    esp_err_t err;

    led_strip_config_t stripConfig = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .flags = {
            .invert_out = 0
        }
    };

    led_strip_rmt_config_t rmtConfig = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = RMT_MEM_BLOCK_SYMBOLS,
        .flags = {
            .with_dma = 0
        }
    };

    ledQueue = queue;

    err = led_strip_new_rmt_device(&stripConfig, &rmtConfig, &ledStrip);

    if (err == ESP_OK)
    {
        err = setBlueBackground();
    }

    if (err == ESP_OK)
    {
        err = led_strip_refresh(ledStrip);
    }

    return err;
}

static uint8_t squareToIndex(const char square[APP_SQUARE_TEXT_LEN], uint32_t * index)
{
    uint8_t valid = 0U;
    uint32_t file;
    uint32_t rank;

    if ((square != NULL) && (index != NULL))
    {
        if ((square[0] >= 'a') && (square[0] <= 'h') &&
            (square[1] >= '1') && (square[1] <= '8'))
        {
            file = (uint32_t)((uint8_t)square[0] - (uint8_t)'a');
            rank = (uint32_t)((uint8_t)square[1] - (uint8_t)'1');

            *index = (rank * 8U) + file;

            if (*index < CHESSBOARD_LED_COUNT)
            {
                valid = 1U;
            }
        }
    }

    return valid;
}

static esp_err_t setSquareColor(const char square[APP_SQUARE_TEXT_LEN],
                                uint32_t red,
                                uint32_t green,
                                uint32_t blue)
{
    uint32_t index;
    esp_err_t err = ESP_ERR_INVALID_ARG;

    if (squareToIndex(square, &index) != 0U)
    {
        err = led_strip_set_pixel(ledStrip, index, red, green, blue);
    }

    return err;
}

static esp_err_t setBlueBackground(void)
{
    esp_err_t err = ESP_OK;
    uint32_t index;

    for (index = 0U; index < LED_STRIP_LED_COUNT; index++)
    {
        err = led_strip_set_pixel(
            ledStrip,
            index,
            BACKGROUND_RED_VALUE,
            BACKGROUND_GREEN_VALUE,
            BACKGROUND_BLUE_VALUE
        );

        if (err != ESP_OK)
        {
            break;
        }
    }

    return err;
}

static esp_err_t applyCommand(const led_command_t * command)
{
    esp_err_t err = ESP_ERR_INVALID_ARG;
    uint32_t i;

    if (command != NULL)
    {
        err = setBlueBackground();

        if ((err == ESP_OK) && (command->clear == 0U))
        {
            for (i = 0U; i < command->legalCount; i++)
            {
                err = setSquareColor(
                    command->legal[i],
                    YELLOW_RED_VALUE,
                    YELLOW_GREEN_VALUE,
                    YELLOW_BLUE_VALUE
                );

                if (err != ESP_OK)
                {
                    break;
                }
            }

            if ((err == ESP_OK) && (command->bestValid != 0U))
            {
                err = setSquareColor(
                    command->bestFrom,
                    GREEN_RED_VALUE,
                    GREEN_GREEN_VALUE,
                    GREEN_BLUE_VALUE
                );
            }

            if ((err == ESP_OK) && (command->bestValid != 0U))
            {
                err = setSquareColor(
                    command->bestTo,
                    GREEN_RED_VALUE,
                    GREEN_GREEN_VALUE,
                    GREEN_BLUE_VALUE
                );
            }
        }

        if (err == ESP_OK)
        {
            err = led_strip_refresh(ledStrip);
        }

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "LED command applied, sequence %lu", (unsigned long)command->sequence);
        }
    }

    return err;
}

void ledTask(void * parameters)
{
    led_command_t command;
    BaseType_t status;
    esp_err_t err;

    (void)parameters;

    for (;;)
    {
        status = xQueueReceive(ledQueue, &command, portMAX_DELAY);

        if (status == pdPASS)
        {
            err = applyCommand(&command);

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "LED command failed: %ld", (long)err);
            }
        }
    }
}
