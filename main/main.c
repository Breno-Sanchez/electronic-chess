#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define ROWS 8
#define COLS 8

#define DISCHARGE_DELAY_MS 3
#define SETTLE_DELAY_MS    5
#define SAMPLE_COUNT       5
#define SAMPLE_DELAY_MS    1
#define REQUIRED_HIGH      3

/*
 * ESP32-S3
 *
 * Linhas 1 a 8.
 * Lado do catodo do diodo.
 * Entrada com pull-down.
 */
static const gpio_num_t row_pins[ROWS] = {
    GPIO_NUM_4,   // linha 1
    GPIO_NUM_5,   // linha 2
    GPIO_NUM_6,   // linha 3
    GPIO_NUM_7,   // linha 4
    GPIO_NUM_8,   // linha 5
    GPIO_NUM_9,   // linha 6
    GPIO_NUM_10,  // linha 7
    GPIO_NUM_11   // linha 8
};

/*
 * ESP32-S3
 *
 * Colunas A a H.
 * Lado do reed/anodo do diodo.
 * Saídas de varredura.
 */
static const gpio_num_t col_pins[COLS] = {
    GPIO_NUM_12,  // coluna A
    GPIO_NUM_13,  // coluna B
    GPIO_NUM_14,  // coluna C
    GPIO_NUM_15,  // coluna D
    GPIO_NUM_16,  // coluna E
    GPIO_NUM_17,  // coluna F
    GPIO_NUM_18,  // coluna G
    GPIO_NUM_21   // coluna H
};

static const char col_names[COLS] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

static void all_cols_low(void)
{
    for (int col = 0; col < COLS; col++) {
        gpio_set_level(col_pins[col], 0);
    }
}

static void configure_gpio(void)
{
    uint64_t row_mask = 0;
    uint64_t col_mask = 0;

    for (int row = 0; row < ROWS; row++) {
        row_mask |= 1ULL << row_pins[row];
    }

    for (int col = 0; col < COLS; col++) {
        col_mask |= 1ULL << col_pins[col];
    }

    gpio_config_t row_cfg = {
        .pin_bit_mask = row_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config_t col_cfg = {
        .pin_bit_mask = col_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&row_cfg);
    gpio_config(&col_cfg);

    all_cols_low();
}

static void scan_matrix(bool board[ROWS][COLS])
{
    memset(board, 0, sizeof(bool) * ROWS * COLS);

    for (int col = 0; col < COLS; col++) {
        all_cols_low();

        vTaskDelay(pdMS_TO_TICKS(DISCHARGE_DELAY_MS));

        gpio_set_level(col_pins[col], 1);

        vTaskDelay(pdMS_TO_TICKS(SETTLE_DELAY_MS));

        for (int row = 0; row < ROWS; row++) {
            int high_count = 0;

            for (int sample = 0; sample < SAMPLE_COUNT; sample++) {
                high_count += gpio_get_level(row_pins[row]);
                vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
            }

            board[row][col] = high_count >= REQUIRED_HIGH;
        }

        gpio_set_level(col_pins[col], 0);
    }

    all_cols_low();
}

static int count_pieces(bool board[ROWS][COLS])
{
    int count = 0;

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (board[row][col]) {
                count++;
            }
        }
    }

    return count;
}

static bool boards_are_equal(bool a[ROWS][COLS], bool b[ROWS][COLS])
{
    return memcmp(a, b, sizeof(bool) * ROWS * COLS) == 0;
}

static void print_board(bool board[ROWS][COLS])
{
    int pieces = count_pieces(board);

    printf("\033[2J\033[H");

    printf("Teste matriz reed switch - ESP32-S3\n");
    printf("Colunas A-H: saidas HIGH uma por vez\n");
    printf("Linhas  1-8: entradas com pull-down\n\n");

    printf("Mapeamento:\n");
    printf("Linhas : 1=GPIO4  2=GPIO5  3=GPIO6  4=GPIO7  5=GPIO8  6=GPIO9  7=GPIO10 8=GPIO11\n");
    printf("Colunas: A=GPIO12 B=GPIO13 C=GPIO14 D=GPIO15 E=GPIO16 F=GPIO17 G=GPIO18 H=GPIO21\n\n");

    printf("Pecas detectadas: %d\n\n", pieces);

    printf("      A B C D E F G H\n");
    printf("    +-----------------+\n");

    for (int rank = 8; rank >= 1; rank--) {
        int row = rank - 1;

        printf(" %d  |", rank);

        for (int col = 0; col < COLS; col++) {
            printf(" %c", board[row][col] ? 'X' : '.');
        }

        printf(" |  %d\n", rank);
    }

    printf("    +-----------------+\n");
    printf("      A B C D E F G H\n\n");

    printf("Casas ocupadas:");

    if (pieces == 0) {
        printf(" nenhuma");
    } else {
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                if (board[row][col]) {
                    printf(" %c%d", col_names[col], row + 1);
                }
            }
        }
    }

    printf("\n");

    fflush(stdout);
}

void app_main(void)
{
    bool board[ROWS][COLS];
    bool last_board[ROWS][COLS];

    memset(board, 0, sizeof(board));
    memset(last_board, 0xFF, sizeof(last_board));

    configure_gpio();

    while (1) {
        scan_matrix(board);

        if (!boards_are_equal(board, last_board)) {
            print_board(board);
            memcpy(last_board, board, sizeof(board));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
