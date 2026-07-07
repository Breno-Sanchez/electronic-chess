#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led.h"
#include "led_map_generated.h"
#include "led_strip.h"

#define LED_STRIP_GPIO              (38)
#define LED_STRIP_LED_COUNT         (150U)
#define INVALID_LED_INDEX           (0xFFFFU)

#define RMT_RESOLUTION_HZ           (10000000U)
#define RMT_MEM_BLOCK_SYMBOLS       (64U)

#define BLUE_R                      (0U)
#define BLUE_G                      (18U)
#define BLUE_B                      (55U)

#define YELLOW_R                    (90U)
#define YELLOW_G                    (68U)
#define YELLOW_B                    (0U)

#define GREEN_R                     (0U)
#define GREEN_G                     (90U)
#define GREEN_B                     (0U)

#define RED_R                       (100U)
#define RED_G                       (0U)
#define RED_B                       (0U)

#define BLINK_MS                    (300U)

static const char * const TAG = "LED";

static QueueHandle_t ledQueue = NULL;
static led_strip_handle_t ledStrip = NULL;

static uint8_t ledIntensity = 80U;
static led_command_t currentCommand;
static uint8_t hasCommand = 0U;
static uint8_t blinkPhase = 1U;


static uint8_t applyIntensity(uint8_t value);
static uint8_t squareToLedIndex(const char square[APP_SQUARE_TEXT_LEN], uint32_t * index);
static uint8_t isPhysicalPresent(const led_command_t * command, const char square[APP_SQUARE_TEXT_LEN]);
static esp_err_t clearAll(void);
static esp_err_t setLedSquare(const char square[APP_SQUARE_TEXT_LEN], uint8_t r, uint8_t g, uint8_t b);
static void setSquareText(char square[APP_SQUARE_TEXT_LEN], uint32_t file, uint32_t rank);
static void renderCheckX(void);
static void renderMateRank(char rank, uint8_t r, uint8_t g, uint8_t b);
static void renderMateSides(uint8_t winnerWhite);
static esp_err_t render(void);

void led_atualizar_config(uint8_t intensidade, uint8_t r, uint8_t g, uint8_t b)
{
    (void)r;
    (void)g;
    (void)b;

    if (intensidade > 100U)
    {
        intensidade = 100U;
    }

    ledIntensity = intensidade;
}

void led_set_erro(const char * casa_origem, const char * casa_destino)
{
    currentCommand.invalidActive = 1U;

    if (casa_origem != NULL)
    {
        (void)strncpy(currentCommand.invalidFrom, casa_origem, APP_SQUARE_TEXT_LEN - 1U);
        currentCommand.invalidFrom[APP_SQUARE_TEXT_LEN - 1U] = '\0';
    }

    if (casa_destino != NULL)
    {
        (void)strncpy(currentCommand.invalidTo, casa_destino, APP_SQUARE_TEXT_LEN - 1U);
        currentCommand.invalidTo[APP_SQUARE_TEXT_LEN - 1U] = '\0';
    }

    hasCommand = 1U;
    (void)render();
}

void led_limpar_erro(void)
{
    currentCommand.invalidActive = 0U;
    currentCommand.invalidFrom[0] = '\0';
    currentCommand.invalidTo[0] = '\0';
    (void)render();
}

esp_err_t ledInit(QueueHandle_t queue)
{
    esp_err_t err;

    led_strip_config_t stripConfig = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
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

    memset(&currentCommand, 0, sizeof(currentCommand));
    ledQueue = queue;

    err = led_strip_new_rmt_device(&stripConfig, &rmtConfig, &ledStrip);

    if (err == ESP_OK)
    {
        err = clearAll();
    }

    if (err == ESP_OK)
    {
        err = led_strip_refresh(ledStrip);
    }

    ESP_LOGI(TAG, "LED strip GPIO %d, count %u", LED_STRIP_GPIO, (unsigned int)LED_STRIP_LED_COUNT);

    return err;
}

static uint8_t applyIntensity(uint8_t value)
{
    return (uint8_t)(((uint32_t)value * (uint32_t)ledIntensity) / 100U);
}

static uint8_t squareToLedIndex(const char square[APP_SQUARE_TEXT_LEN], uint32_t * index)
{
    uint32_t file;
    uint32_t rank;
    uint16_t mapped;

    if ((square == NULL) || (index == NULL))
    {
        return 0U;
    }

    if ((square[0] < 'a') || (square[0] > 'h') ||
        (square[1] < '1') || (square[1] > '8'))
    {
        return 0U;
    }

    file = (uint32_t)(square[0] - 'a');
    rank = (uint32_t)(square[1] - '1');
    mapped = ledMapGenerated[rank][file];

    if ((mapped == INVALID_LED_INDEX) || (mapped >= LED_STRIP_LED_COUNT))
    {
        return 0U;
    }

    *index = (uint32_t)mapped;
    return 1U;
}

static uint8_t isPhysicalPresent(const led_command_t * command, const char square[APP_SQUARE_TEXT_LEN])
{
    uint8_t present = 0U;

    if ((command != NULL) && (square != NULL) &&
        (square[0] >= 'a') && (square[0] <= 'h') &&
        (square[1] >= '1') && (square[1] <= '8'))
    {
        uint32_t file = (uint32_t)(square[0] - 'a');
        uint32_t rank = (uint32_t)(square[1] - '1');
        uint8_t mask = (uint8_t)(1U << file);

        if ((command->physical[rank] & mask) != 0U)
        {
            present = 1U;
        }
    }

    return present;
}

static esp_err_t clearAll(void)
{
    esp_err_t err = ESP_OK;

    for (uint32_t i = 0U; i < LED_STRIP_LED_COUNT; i++)
    {
        err = led_strip_set_pixel(ledStrip, i, 0U, 0U, 0U);

        if (err != ESP_OK)
        {
            break;
        }
    }

    return err;
}

static esp_err_t setLedSquare(const char square[APP_SQUARE_TEXT_LEN], uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t index;

    if (squareToLedIndex(square, &index) == 0U)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return led_strip_set_pixel(
        ledStrip,
        index,
        applyIntensity(r),
        applyIntensity(g),
        applyIntensity(b)
    );
}

static void setSquareText(char square[APP_SQUARE_TEXT_LEN], uint32_t file, uint32_t rank)
{
    if (square != NULL)
    {
        square[0] = (char)('a' + (char)file);
        square[1] = (char)('1' + (char)rank);
        square[2] = '\0';
    }
}

static void renderCheckX(void)
{
    static const char * const diagA[8] = {"a1", "b2", "c3", "d4", "e5", "f6", "g7", "h8"};
    static const char * const diagB[8] = {"a8", "b7", "c6", "d5", "e4", "f3", "g2", "h1"};

    for (uint32_t i = 0U; i < 8U; i++)
    {
        (void)setLedSquare(diagA[i], RED_R, RED_G, RED_B);
        (void)setLedSquare(diagB[i], RED_R, RED_G, RED_B);
    }
}

static void renderMateRank(char rank, uint8_t r, uint8_t g, uint8_t b)
{
    char square[APP_SQUARE_TEXT_LEN];

    for (char file = 'a'; file <= 'h'; file++)
    {
        square[0] = file;
        square[1] = rank;
        square[2] = '\0';
        (void)setLedSquare(square, r, g, b);
    }
}

static void renderMateSides(uint8_t winnerWhite)
{
    if (blinkPhase == 0U)
    {
        return;
    }

    if (winnerWhite != 0U)
    {
        renderMateRank('1', GREEN_R, GREEN_G, GREEN_B);
        renderMateRank('2', GREEN_R, GREEN_G, GREEN_B);
        renderMateRank('7', RED_R, RED_G, RED_B);
        renderMateRank('8', RED_R, RED_G, RED_B);
    }
    else
    {
        renderMateRank('7', GREEN_R, GREEN_G, GREEN_B);
        renderMateRank('8', GREEN_R, GREEN_G, GREEN_B);
        renderMateRank('1', RED_R, RED_G, RED_B);
        renderMateRank('2', RED_R, RED_G, RED_B);
    }
}

static esp_err_t render(void)
{
    char square[APP_SQUARE_TEXT_LEN];
    esp_err_t err;

    if (ledStrip == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = clearAll();

    if (err != ESP_OK)
    {
        return err;
    }

    if (hasCommand != 0U)
    {
        for (uint32_t rank = 0U; rank < APP_BOARD_RANK_COUNT; rank++)
        {
            for (uint32_t file = 0U; file < APP_BOARD_FILE_COUNT; file++)
            {
                setSquareText(square, file, rank);

                if (isPhysicalPresent(&currentCommand, square) != 0U)
                {
                    (void)setLedSquare(square, BLUE_R, BLUE_G, BLUE_B);
                }
            }
        }
    }

    if ((hasCommand != 0U) && (currentCommand.helpEnabled != 0U))
    {
        for (uint32_t i = 0U; i < currentCommand.legalCount; i++)
        {
            (void)setLedSquare(currentCommand.legal[i], YELLOW_R, YELLOW_G, YELLOW_B);
        }

        if (currentCommand.bestValid != 0U)
        {
            (void)setLedSquare(currentCommand.bestFrom, GREEN_R, GREEN_G, GREEN_B);
            (void)setLedSquare(currentCommand.bestTo, GREEN_R, GREEN_G, GREEN_B);
        }
    }

    if ((hasCommand != 0U) && (currentCommand.blinkActive != 0U))
    {
        if (blinkPhase != 0U)
        {
            (void)setLedSquare(currentCommand.blinkSquare, BLUE_R, BLUE_G, BLUE_B);
        }
        else
        {
            (void)setLedSquare(currentCommand.blinkSquare, 0U, 0U, 0U);
        }
    }

    if ((hasCommand != 0U) && (currentCommand.checkActive != 0U))
    {
        renderCheckX();
    }

    if ((hasCommand != 0U) && (currentCommand.invalidActive != 0U))
    {
        (void)setLedSquare(currentCommand.invalidFrom, RED_R, RED_G, RED_B);
        (void)setLedSquare(currentCommand.invalidTo, RED_R, RED_G, RED_B);
    }

    if ((hasCommand != 0U) && (currentCommand.mateActive != 0U))
    {
        renderMateSides(currentCommand.winnerWhite);
    }

    return led_strip_refresh(ledStrip);
}

void ledTask(void * parameters)
{
    led_command_t command;
    BaseType_t status;

    (void)parameters;

    for (;;)
    {
        status = xQueueReceive(ledQueue, &command, pdMS_TO_TICKS(BLINK_MS));

        if (status == pdPASS)
        {
            currentCommand = command;
            hasCommand = 1U;
            blinkPhase = 1U;

            if (render() == ESP_OK)
            {
                ESP_LOGI(TAG, "LED command applied, sequence %lu", (unsigned long)command.sequence);
            }
            else
            {
                ESP_LOGE(TAG, "LED command failed");
            }
        }
        else
        {
            blinkPhase = (blinkPhase == 0U) ? 1U : 0U;

            if ((hasCommand != 0U) &&
                ((currentCommand.blinkActive != 0U) || (currentCommand.mateActive != 0U)))
            {
                (void)render();
            }
        }
    }
}
