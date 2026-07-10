#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

#define APP_SQUARE_TEXT_LEN       (3U)
#define APP_MOVE_TEXT_LEN         (6U)
#define APP_MAX_LEGAL_MOVES       (64U)
#define APP_BOARD_RANK_COUNT      (8U)
#define APP_BOARD_FILE_COUNT      (8U)

typedef enum
{
    SENSOR_STATE_PRESENT = 0,
    SENSOR_STATE_LIFTED = 1
} sensor_piece_state_t;

typedef struct
{
    uint32_t sequence;
    sensor_piece_state_t state;
    char square[APP_SQUARE_TEXT_LEN];
} sensor_event_t;

typedef enum
{
    LED_GAME_MODE_SETUP = 0,
    LED_GAME_MODE_RUNNING = 1,
    LED_GAME_MODE_LIFTED = 2,
    LED_GAME_MODE_PROMOTION = 3,
    LED_GAME_MODE_INVALID = 4,
    LED_GAME_MODE_SYNC_WAIT = 5,
    LED_GAME_MODE_CHECKMATE = 6
} led_game_mode_t;

typedef enum
{
    LED_SIDE_NONE = 0,
    LED_SIDE_WHITE = 1,
    LED_SIDE_BLACK = 2
} led_side_t;

typedef struct
{
    uint32_t sequence;

    uint8_t clear;
    uint8_t gameMode;
    uint8_t sideToMove;

    uint8_t physical[APP_BOARD_RANK_COUNT];
    uint8_t whitePieces[APP_BOARD_RANK_COUNT];
    uint8_t blackPieces[APP_BOARD_RANK_COUNT];
    uint8_t legal[APP_BOARD_RANK_COUNT];
    uint8_t best[APP_BOARD_RANK_COUNT];
    uint8_t invalid[APP_BOARD_RANK_COUNT];
    uint8_t check[APP_BOARD_RANK_COUNT];

    uint8_t blinkActive;
    uint8_t rainbowActive;
    uint8_t mateActive;
    uint8_t winnerWhite;

    char blinkSquare[APP_SQUARE_TEXT_LEN];
} led_command_t;

#endif
