#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_types.h"
#include "chess_engine.h"
#include "chess_logic.h"
#include "esp_eap_client.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led.h"
#include "nvs_flash.h"
#include "server.h"

#define WIFI_SSID_TXT              "__WIFI_SSID__"
#define EAP_IDENTITY_TXT           "__EAP_IDENTITY__"
#define EAP_USERNAME_TXT           "__EAP_USERNAME__"
#define EAP_PASSWORD_TXT           "__EAP_PASSWORD__"
#define SOFTAP_SSID_TXT            "XADREZ_ESP"
#define SOFTAP_PASS_TXT            "xadrez12345"
#define SOFTAP_CHANNEL             (1U)
#define SOFTAP_MAX_CONNECTIONS     (2U)
#define EAP_MAX_LEN                (127U)
#define STATE_TEXT_LEN             (32U)
#define MODE_TEXT_LEN              (32U)
#define LEGAL_TEXT_LEN             (256U)
#define HTTP_CHUNK_LEN             (768U)
#define GAME_COMMAND_QUEUE_LEN     (8U)
#define START_RAINBOW_MS           (1600U)

typedef enum
{
    GAME_MODE_SETUP = 0,
    GAME_MODE_RUNNING,
    GAME_MODE_LIFTED,
    GAME_MODE_PROMOTION_PENDING,
    GAME_MODE_INVALID_LOCK,
    GAME_MODE_SYNC_WAIT,
    GAME_MODE_CHECKMATE_LOCK
} game_mode_t;

typedef enum
{
    GAME_COMMAND_START = 0,
    GAME_COMMAND_PROMOTE
} game_command_type_t;

typedef struct
{
    game_command_type_t type;
    chess_piece_type_t promotion;
} game_command_t;

static const char * const TAG = "SERVER";

static QueueHandle_t sensorQueueRef = NULL;
static QueueHandle_t ledQueueRef = NULL;
static QueueHandle_t gameCommandQueue = NULL;
static SemaphoreHandle_t stateMutex = NULL;
static httpd_handle_t httpServer = NULL;
static uint8_t wifiConnected = 0U;

static StaticQueue_t gameCommandQueueControl;
static uint8_t gameCommandQueueStorage[GAME_COMMAND_QUEUE_LEN * sizeof(game_command_t)];

static chess_game_t game;
static game_mode_t gameMode = GAME_MODE_SETUP;
static uint8_t physicalPresence[APP_BOARD_RANK_COUNT] = {0U};
static uint8_t legalMap[APP_BOARD_RANK_COUNT] = {0U};
static uint8_t bestMap[APP_BOARD_RANK_COUNT] = {0U};
static uint8_t invalidMap[APP_BOARD_RANK_COUNT] = {0U};
static uint8_t checkMap[APP_BOARD_RANK_COUNT] = {0U};
static uint8_t selectedValid = 0U;
static char selectedSquare[APP_SQUARE_TEXT_LEN] = "";
static chess_move_t pendingPromotionMove;
static uint8_t pendingPromotionValid = 0U;
static char stateText[STATE_TEXT_LEN] = "SETUP";
static char stateLegal[LEGAL_TEXT_LEN] = "-";
static char stateBest[APP_MOVE_TEXT_LEN] = "-----";
static uint8_t winnerWhite = 0U;
static uint32_t ledSequence = 1U;

static size_t boundedStringLength(const char * text, size_t maxLen);
static void copyStringToU8(uint8_t * dst, size_t dstLen, const char * src);
static void setBit(uint8_t map[APP_BOARD_RANK_COUNT], const char square[APP_SQUARE_TEXT_LEN]);
static void clearMaps(void);
static void updatePhysical(const char square[APP_SQUARE_TEXT_LEN], uint8_t present);
static void rebuildOwnerMaps(led_command_t * command);
static void sendLedFrame(uint8_t rainbowActive);
static void setMode(game_mode_t mode, const char * text);
static bool physicalMatchesStart(void);
static bool physicalMatchesBoard(void);
static void startGame(void);
static void handleSensorEvent(const sensor_event_t * event);
static void handleGameCommand(const game_command_t * command);
static void processPendingCommands(void);
static void computeLiftHints(uint8_t row, uint8_t col);
static bool applySelectedMove(uint8_t toRow, uint8_t toCol, chess_piece_type_t promotion);
static chess_piece_type_t selectorPromotionPiece(const char square[APP_SQUARE_TEXT_LEN]);
static void updateCheckState(void);
static void updateLegalTextFromMap(void);
static const char * modeToText(game_mode_t mode);

static esp_err_t initNvs(void);
static esp_err_t initEnterpriseAuth(void);
static void eventHandler(void * arg, esp_event_base_t base, int32_t id, void * data);
static esp_err_t startHttpServer(void);
static esp_err_t rootHandler(httpd_req_t * req);
static esp_err_t apiStateHandler(httpd_req_t * req);
static esp_err_t apiStartHandler(httpd_req_t * req);
static esp_err_t apiPromoteHandler(httpd_req_t * req);

static const httpd_uri_t rootUri = { .uri = "/", .method = HTTP_GET, .handler = rootHandler, .user_ctx = NULL };
static const httpd_uri_t stateUri = { .uri = "/api/state", .method = HTTP_GET, .handler = apiStateHandler, .user_ctx = NULL };
static const httpd_uri_t startUri = { .uri = "/api/start", .method = HTTP_POST, .handler = apiStartHandler, .user_ctx = NULL };
static const httpd_uri_t promoteUri = { .uri = "/api/promote", .method = HTTP_POST, .handler = apiPromoteHandler, .user_ctx = NULL };

static size_t boundedStringLength(const char * text, size_t maxLen)
{
    size_t len = 0U;
    if (text != NULL)
    {
        while ((len < maxLen) && (text[len] != '\0'))
        {
            len++;
        }
    }
    return len;
}

static void copyStringToU8(uint8_t * dst, size_t dstLen, const char * src)
{
    size_t index = 0U;
    if ((dst != NULL) && (src != NULL) && (dstLen > 0U))
    {
        while ((src[index] != '\0') && (index < (dstLen - 1U)))
        {
            dst[index] = (uint8_t)src[index];
            index++;
        }
        dst[index] = (uint8_t)'\0';
    }
}

static void setMode(game_mode_t mode, const char * text)
{
    gameMode = mode;
    if (text != NULL)
    {
        (void)snprintf(stateText, sizeof(stateText), "%s", text);
    }
}

static const char * modeToText(game_mode_t mode)
{
    switch (mode)
    {
        case GAME_MODE_SETUP: return "SETUP";
        case GAME_MODE_RUNNING: return "RUNNING";
        case GAME_MODE_LIFTED: return "LIFTED";
        case GAME_MODE_PROMOTION_PENDING: return "PROMOTION";
        case GAME_MODE_INVALID_LOCK: return "INVALID_LOCK";
        case GAME_MODE_SYNC_WAIT: return "SYNC_WAIT";
        case GAME_MODE_CHECKMATE_LOCK: return "CHECKMATE";
        default: return "UNKNOWN";
    }
}

static void setBit(uint8_t map[APP_BOARD_RANK_COUNT], const char square[APP_SQUARE_TEXT_LEN])
{
    if ((map != NULL) && (square != NULL) &&
        (square[0] >= 'a') && (square[0] <= 'h') &&
        (square[1] >= '1') && (square[1] <= '8'))
    {
        uint32_t file = (uint32_t)(square[0] - 'a');
        uint32_t rank = (uint32_t)(square[1] - '1');
        map[rank] = (uint8_t)(map[rank] | ((uint8_t)(1U << file)));
    }
}

static void clearMaps(void)
{
    (void)memset(legalMap, 0, sizeof(legalMap));
    (void)memset(bestMap, 0, sizeof(bestMap));
    (void)memset(invalidMap, 0, sizeof(invalidMap));
    (void)memset(checkMap, 0, sizeof(checkMap));
    stateLegal[0] = '-';
    stateLegal[1] = '\0';
    (void)snprintf(stateBest, sizeof(stateBest), "-----");
}

static void updatePhysical(const char square[APP_SQUARE_TEXT_LEN], uint8_t present)
{
    if ((square != NULL) && (square[0] >= 'a') && (square[0] <= 'h') && (square[1] >= '1') && (square[1] <= '8'))
    {
        uint32_t file = (uint32_t)(square[0] - 'a');
        uint32_t rank = (uint32_t)(square[1] - '1');
        uint8_t mask = (uint8_t)(1U << file);

        if (present != 0U)
        {
            physicalPresence[rank] = (uint8_t)(physicalPresence[rank] | mask);
        }
        else
        {
            physicalPresence[rank] = (uint8_t)(physicalPresence[rank] & ((uint8_t)(~mask)));
        }
    }
}

static void rebuildOwnerMaps(led_command_t * command)
{
    if (command == NULL)
    {
        return;
    }

    for (uint8_t row = 0U; row < 8U; row++)
    {
        for (uint8_t col = 0U; col < 8U; col++)
        {
            chess_piece_t piece = chess_get_piece(&game, row, col);
            uint8_t rank = (uint8_t)(7U - row);
            uint8_t bit = (uint8_t)(1U << col);

            if (piece.color == CHESS_WHITE)
            {
                command->whitePieces[rank] = (uint8_t)(command->whitePieces[rank] | bit);
            }
            else if (piece.color == CHESS_BLACK)
            {
                command->blackPieces[rank] = (uint8_t)(command->blackPieces[rank] | bit);
            }
        }
    }
}

static void sendLedFrame(uint8_t rainbowActive)
{
    led_command_t command;

    (void)memset(&command, 0, sizeof(command));
    command.sequence = ledSequence++;
    command.gameMode = (uint8_t)gameMode;
    command.sideToMove = (game.side_to_move == CHESS_WHITE) ? LED_SIDE_WHITE : LED_SIDE_BLACK;
    command.rainbowActive = rainbowActive;
    command.blinkActive = selectedValid;
    command.mateActive = (gameMode == GAME_MODE_CHECKMATE_LOCK) ? 1U : 0U;
    command.winnerWhite = winnerWhite;

    (void)memcpy(command.physical, physicalPresence, sizeof(command.physical));
    (void)memcpy(command.legal, legalMap, sizeof(command.legal));
    (void)memcpy(command.best, bestMap, sizeof(command.best));
    (void)memcpy(command.invalid, invalidMap, sizeof(command.invalid));
    (void)memcpy(command.check, checkMap, sizeof(command.check));
    (void)snprintf(command.blinkSquare, sizeof(command.blinkSquare), "%s", selectedSquare);

    rebuildOwnerMaps(&command);

    if (ledQueueRef != NULL)
    {
        (void)xQueueSend(ledQueueRef, &command, portMAX_DELAY);
    }
}

static bool physicalMatchesStart(void)
{
    for (uint8_t rank = 0U; rank < 8U; rank++)
    {
        uint8_t expected = ((rank <= 1U) || (rank >= 6U)) ? 0xFFU : 0x00U;
        if (physicalPresence[rank] != expected)
        {
            return false;
        }
    }
    return true;
}

static bool physicalMatchesBoard(void)
{
    uint8_t expected[8] = {0U};

    for (uint8_t row = 0U; row < 8U; row++)
    {
        for (uint8_t col = 0U; col < 8U; col++)
        {
            if (game.board[row][col].type != CHESS_EMPTY)
            {
                uint8_t rank = (uint8_t)(7U - row);
                expected[rank] = (uint8_t)(expected[rank] | ((uint8_t)(1U << col)));
            }
        }
    }

    for (uint8_t rank = 0U; rank < 8U; rank++)
    {
        if (physicalPresence[rank] != expected[rank])
        {
            return false;
        }
    }

    return true;
}

static void updateLegalTextFromMap(void)
{
    size_t pos = 0U;
    bool any = false;

    for (uint8_t rank = 0U; rank < 8U; rank++)
    {
        for (uint8_t file = 0U; file < 8U; file++)
        {
            if ((legalMap[rank] & ((uint8_t)(1U << file))) != 0U)
            {
                if (pos < (sizeof(stateLegal) - 4U))
                {
                    pos += (size_t)snprintf(&stateLegal[pos], sizeof(stateLegal) - pos, "%c%c ", (char)('a' + file), (char)('1' + rank));
                    any = true;
                }
            }
        }
    }

    if (any == false)
    {
        (void)snprintf(stateLegal, sizeof(stateLegal), "-");
    }
}

static void updateCheckState(void)
{
    uint8_t row;
    uint8_t col;
    char square[APP_SQUARE_TEXT_LEN];

    (void)memset(checkMap, 0, sizeof(checkMap));

    if ((chess_is_check(&game, game.side_to_move) == true) &&
        (chess_find_king(&game, game.side_to_move, &row, &col) == true))
    {
        chess_index_to_square(row, col, square);
        setBit(checkMap, square);
    }

    if (chess_is_checkmate(&game, game.side_to_move) == true)
    {
        winnerWhite = (game.side_to_move == CHESS_BLACK) ? 1U : 0U;
        setMode(GAME_MODE_CHECKMATE_LOCK, "CHECKMATE");
    }
}

static void computeLiftHints(uint8_t row, uint8_t col)
{
    chess_move_t moves[CHESS_MAX_MOVES];
    chess_move_t best;
    uint32_t count;
    char square[APP_SQUARE_TEXT_LEN];

    clearMaps();
    count = chess_generate_legal_moves_from(&game, row, col, moves, CHESS_MAX_MOVES);

    for (uint32_t i = 0U; i < count; i++)
    {
        chess_index_to_square(moves[i].to_row, moves[i].to_col, square);
        setBit(legalMap, square);
    }

    if (chess_engine_best_for_origin(&game, row, col, &best) == true)
    {
        chess_index_to_square(best.to_row, best.to_col, square);
        setBit(bestMap, square);
        (void)snprintf(stateBest, sizeof(stateBest), "%c%c%c%c", (char)('a' + best.from_col), (char)('8' - best.from_row), (char)('a' + best.to_col), (char)('8' - best.to_row));
    }

    updateLegalTextFromMap();
}

static chess_piece_type_t selectorPromotionPiece(const char square[APP_SQUARE_TEXT_LEN])
{
    if (square == NULL)
    {
        return CHESS_EMPTY;
    }

    if ((square[1] == '1') || (square[1] == '8'))
    {
        if ((square[0] == 'a') || (square[0] == 'h')) return CHESS_ROOK;
        if ((square[0] == 'b') || (square[0] == 'g')) return CHESS_KNIGHT;
        if ((square[0] == 'c') || (square[0] == 'f')) return CHESS_BISHOP;
        if (square[0] == 'd') return CHESS_QUEEN;
    }

    return CHESS_EMPTY;
}

static bool applySelectedMove(uint8_t toRow, uint8_t toCol, chess_piece_type_t promotion)
{
    uint8_t fromRow;
    uint8_t fromCol;
    chess_move_t move;

    if ((selectedValid == 0U) || (chess_square_to_index(selectedSquare, &fromRow, &fromCol) == false))
    {
        return false;
    }

    move.from_row = fromRow;
    move.from_col = fromCol;
    move.to_row = toRow;
    move.to_col = toCol;
    move.promotion = promotion;

    if (chess_is_legal_move(&game, &move) == false)
    {
        return false;
    }

    if ((promotion == CHESS_EMPTY) && (chess_is_promotion_move(&game, &move) == true))
    {
        pendingPromotionMove = move;
        pendingPromotionValid = 1U;
        setMode(GAME_MODE_PROMOTION_PENDING, "PROMOTION_PENDING");
        clearMaps();
        chess_index_to_square(toRow, toCol, selectedSquare);
        selectedValid = 1U;
        return true;
    }

    if (promotion == CHESS_EMPTY)
    {
        move.promotion = CHESS_QUEEN;
    }

    if (chess_apply_move(&game, &move) == false)
    {
        return false;
    }

    selectedValid = 0U;
    selectedSquare[0] = '\0';
    pendingPromotionValid = 0U;
    clearMaps();
    updateCheckState();

    if (gameMode != GAME_MODE_CHECKMATE_LOCK)
    {
        if (physicalMatchesBoard() == true)
        {
            setMode(GAME_MODE_RUNNING, "RUNNING");
        }
        else
        {
            setMode(GAME_MODE_SYNC_WAIT, "SYNC_WAIT");
        }
    }

    return true;
}

static void startGame(void)
{
    clearMaps();
    selectedValid = 0U;
    selectedSquare[0] = '\0';
    pendingPromotionValid = 0U;
    winnerWhite = 0U;

    if (physicalMatchesStart() == false)
    {
        setMode(GAME_MODE_SETUP, "SETUP_NOT_READY");
        invalidMap[0] = 0xFFU;
        invalidMap[1] = 0xFFU;
        invalidMap[6] = 0xFFU;
        invalidMap[7] = 0xFFU;
        sendLedFrame(0U);
        return;
    }

    chess_reset(&game);
    setMode(GAME_MODE_RUNNING, "RUNNING");
    sendLedFrame(1U);
    vTaskDelay(pdMS_TO_TICKS(START_RAINBOW_MS));
    sendLedFrame(0U);
}

static void handleGameCommand(const game_command_t * command)
{
    if (command == NULL)
    {
        return;
    }

    if (command->type == GAME_COMMAND_START)
    {
        startGame();
    }
    else if ((command->type == GAME_COMMAND_PROMOTE) && (gameMode == GAME_MODE_PROMOTION_PENDING) && (pendingPromotionValid != 0U))
    {
        selectedValid = 1U;
        chess_index_to_square(pendingPromotionMove.from_row, pendingPromotionMove.from_col, selectedSquare);
        (void)applySelectedMove(pendingPromotionMove.to_row, pendingPromotionMove.to_col, command->promotion);
        sendLedFrame(0U);
    }
}

static void processPendingCommands(void)
{
    game_command_t command;

    while ((gameCommandQueue != NULL) && (xQueueReceive(gameCommandQueue, &command, 0) == pdPASS))
    {
        handleGameCommand(&command);
    }
}

static void handleSensorEvent(const sensor_event_t * event)
{
    uint8_t row;
    uint8_t col;
    chess_piece_t piece;
    chess_piece_type_t promotion;

    if ((event == NULL) || (chess_square_to_index(event->square, &row, &col) == false))
    {
        return;
    }

    updatePhysical(event->square, (event->state == SENSOR_STATE_PRESENT) ? 1U : 0U);

    if ((gameMode == GAME_MODE_SETUP) || (gameMode == GAME_MODE_CHECKMATE_LOCK))
    {
        sendLedFrame(0U);
        return;
    }

    if ((gameMode == GAME_MODE_INVALID_LOCK) || (gameMode == GAME_MODE_SYNC_WAIT))
    {
        if (physicalMatchesBoard() == true)
        {
            clearMaps();
            setMode(GAME_MODE_RUNNING, "RUNNING");
        }
        sendLedFrame(0U);
        return;
    }

    if (gameMode == GAME_MODE_PROMOTION_PENDING)
    {
        promotion = selectorPromotionPiece(event->square);
        if ((promotion != CHESS_EMPTY) && (event->state == SENSOR_STATE_PRESENT) && (pendingPromotionValid != 0U))
        {
            game_command_t command = { GAME_COMMAND_PROMOTE, promotion };
            handleGameCommand(&command);
        }
        sendLedFrame(0U);
        return;
    }

    if (event->state == SENSOR_STATE_LIFTED)
    {
        piece = chess_get_piece(&game, row, col);
        clearMaps();

        if ((piece.type != CHESS_EMPTY) && (piece.color == game.side_to_move))
        {
            (void)snprintf(selectedSquare, sizeof(selectedSquare), "%s", event->square);
            selectedValid = 1U;
            computeLiftHints(row, col);
            setMode(GAME_MODE_LIFTED, "PIECE_LIFTED");
        }
        else
        {
            setBit(invalidMap, event->square);
            setMode(GAME_MODE_INVALID_LOCK, "INVALID_LIFT");
        }
    }
    else if ((event->state == SENSOR_STATE_PRESENT) && (selectedValid != 0U))
    {
        if (strcmp(selectedSquare, event->square) == 0)
        {
            selectedValid = 0U;
            selectedSquare[0] = '\0';
            clearMaps();
            setMode(GAME_MODE_RUNNING, "RUNNING");
        }
        else if (applySelectedMove(row, col, CHESS_EMPTY) == false)
        {
            setBit(invalidMap, selectedSquare);
            setBit(invalidMap, event->square);
            setMode(GAME_MODE_INVALID_LOCK, "INVALID_MOVE");
        }
    }

    sendLedFrame(0U);
}

esp_err_t serverInit(QueueHandle_t sensorQueue, QueueHandle_t ledQueue)
{
    esp_err_t err = ESP_ERR_INVALID_ARG;

    if ((sensorQueue != NULL) && (ledQueue != NULL))
    {
        sensorQueueRef = sensorQueue;
        ledQueueRef = ledQueue;
        stateMutex = xSemaphoreCreateMutex();
        gameCommandQueue = xQueueCreateStatic(
            GAME_COMMAND_QUEUE_LEN,
            sizeof(game_command_t),
            gameCommandQueueStorage,
            &gameCommandQueueControl
        );

        if ((stateMutex != NULL) && (gameCommandQueue != NULL))
        {
            chess_reset(&game);
            setMode(GAME_MODE_SETUP, "SETUP");
            err = ESP_OK;
        }
    }

    return err;
}

static esp_err_t initNvs(void)
{
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        err = nvs_flash_erase();
        if (err == ESP_OK)
        {
            err = nvs_flash_init();
        }
    }
    return err;
}

static esp_err_t initEnterpriseAuth(void)
{
    size_t identity_len = boundedStringLength(EAP_IDENTITY_TXT, EAP_MAX_LEN + 1U);
    size_t username_len = boundedStringLength(EAP_USERNAME_TXT, EAP_MAX_LEN + 1U);
    size_t password_len = boundedStringLength(EAP_PASSWORD_TXT, EAP_MAX_LEN + 1U);
    esp_err_t err = ESP_ERR_INVALID_ARG;

    if ((identity_len > 0U) && (identity_len <= EAP_MAX_LEN) &&
        (username_len > 0U) && (username_len <= EAP_MAX_LEN) &&
        (password_len > 0U) && (password_len <= EAP_MAX_LEN))
    {
        err = esp_eap_client_set_identity((const unsigned char *)EAP_IDENTITY_TXT, (int)identity_len);
        if (err == ESP_OK) err = esp_eap_client_set_username((const unsigned char *)EAP_USERNAME_TXT, (int)username_len);
        if (err == ESP_OK) err = esp_eap_client_set_password((const unsigned char *)EAP_PASSWORD_TXT, (int)password_len);
        if (err == ESP_OK) err = esp_wifi_sta_enterprise_enable();
    }

    return err;
}

static void eventHandler(void * arg, esp_event_base_t base, int32_t id, void * data)
{
    esp_err_t err;
    (void)arg;

    if ((base == WIFI_EVENT) && (id == WIFI_EVENT_STA_START))
    {
        err = esp_wifi_connect();
        if (err != ESP_OK) ESP_LOGE(TAG, "WiFi connect error: %ld", (long)err);
    }
    else if ((base == WIFI_EVENT) && (id == WIFI_EVENT_STA_DISCONNECTED))
    {
        wifiConnected = 0U;
        if (data != NULL)
        {
            const wifi_event_sta_disconnected_t * const event = (const wifi_event_sta_disconnected_t *)data;
            ESP_LOGE(TAG, "WiFi disconnected. Reason: %u", (unsigned int)event->reason);
        }
        err = esp_wifi_connect();
        if (err != ESP_OK) ESP_LOGE(TAG, "WiFi reconnect error: %ld", (long)err);
    }
    else if ((base == IP_EVENT) && (id == IP_EVENT_STA_GOT_IP))
    {
        wifiConnected = 1U;
        if (data != NULL)
        {
            const ip_event_got_ip_t * const event = (const ip_event_got_ip_t *)data;
            ESP_LOGI(TAG, "ESP32 IP: " IPSTR, IP2STR(&event->ip_info.ip));
        }
    }
}

esp_err_t serverNetworkInit(void)
{
    esp_err_t err;
    esp_netif_t * staNetif;
    esp_netif_t * apNetif;
    wifi_init_config_t initCfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t staCfg = {0};
    wifi_config_t apCfg = {0};
    size_t apSsidLen;

    err = initNvs();
    if (err == ESP_OK) err = esp_netif_init();
    if (err == ESP_OK) err = esp_event_loop_create_default();

    if (err == ESP_OK)
    {
        staNetif = esp_netif_create_default_wifi_sta();
        apNetif = esp_netif_create_default_wifi_ap();
        if ((staNetif == NULL) || (apNetif == NULL)) err = ESP_FAIL;
    }

    if (err == ESP_OK) err = esp_wifi_init(&initCfg);
    if (err == ESP_OK) err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, eventHandler, NULL, NULL);
    if (err == ESP_OK) err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, eventHandler, NULL, NULL);

    if (err == ESP_OK)
    {
        copyStringToU8(staCfg.sta.ssid, sizeof(staCfg.sta.ssid), WIFI_SSID_TXT);
        copyStringToU8(apCfg.ap.ssid, sizeof(apCfg.ap.ssid), SOFTAP_SSID_TXT);
        copyStringToU8(apCfg.ap.password, sizeof(apCfg.ap.password), SOFTAP_PASS_TXT);
        apSsidLen = boundedStringLength(SOFTAP_SSID_TXT, sizeof(apCfg.ap.ssid));

        if (apSsidLen <= 32U)
        {
            apCfg.ap.ssid_len = (uint8_t)apSsidLen;
        }
        else
        {
            err = ESP_ERR_INVALID_ARG;
        }

        apCfg.ap.channel = (uint8_t)SOFTAP_CHANNEL;
        apCfg.ap.max_connection = (uint8_t)SOFTAP_MAX_CONNECTIONS;
        apCfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_STA, &staCfg);
    if (err == ESP_OK) err = initEnterpriseAuth();
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &apCfg);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK) err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err == ESP_OK) err = startHttpServer();

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "SoftAP SSID: %s", SOFTAP_SSID_TXT);
        ESP_LOGI(TAG, "SoftAP IP: 192.168.4.1");
    }

    return err;
}

static esp_err_t startHttpServer(void)
{
    esp_err_t err = ESP_OK;

    if (httpServer == NULL)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.stack_size = 12288U;
        config.lru_purge_enable = true;
        config.recv_wait_timeout = 5;
        config.send_wait_timeout = 5;

        err = httpd_start(&httpServer, &config);
        if (err == ESP_OK) err = httpd_register_uri_handler(httpServer, &rootUri);
        if (err == ESP_OK) err = httpd_register_uri_handler(httpServer, &stateUri);
        if (err == ESP_OK) err = httpd_register_uri_handler(httpServer, &startUri);
        if (err == ESP_OK) err = httpd_register_uri_handler(httpServer, &promoteUri);

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "HTTP server started at http://192.168.4.1/");
        }
    }

    return err;
}

static esp_err_t rootHandler(httpd_req_t * req)
{
    static const char html[] =
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Xadrez ESP32-S3</title><style>"
        "body{font-family:Arial,sans-serif;background:#10131a;color:#eee;margin:18px}"
        ".layout{display:flex;gap:24px;flex-wrap:wrap}.board{display:grid;grid-template-columns:repeat(8,58px);grid-template-rows:repeat(8,58px);border:4px solid #333}"
        ".sq{width:58px;height:58px;display:flex;align-items:center;justify-content:center;font-size:36px;box-sizing:border-box;position:relative}"
        ".light{background:#d9c09b;color:#111}.dark{background:#73543a;color:#111}.phys{box-shadow:inset 0 0 0 6px #285dff}.turn{box-shadow:inset 0 0 0 8px #4ba3ff}"
        ".legal{background:#d8c52f!important}.best{background:#0b9f39!important}.invalid,.check{background:#c01818!important;color:#fff}.winner{background:#11802f!important}.loser{background:#b00020!important;color:#fff}"
        ".sel{animation:blink .7s infinite}@keyframes blink{50%{filter:brightness(1.8)}}button{font-size:20px;margin:4px;padding:10px 18px}.box{background:#1d2430;padding:12px;margin:8px 0;border-radius:8px;max-width:760px}.fen{font-family:monospace;word-break:break-all;font-size:14px}"
        "</style></head><body><h1>Xadrez físico ESP32-S3</h1><div><button onclick=\"startGame()\">Start</button>"
        "<button onclick=\"promote('Q')\">Queen</button><button onclick=\"promote('R')\">Rook</button><button onclick=\"promote('B')\">Bishop</button><button onclick=\"promote('N')\">Knight</button></div>"
        "<div class=\"layout\"><div class=\"board\" id=\"board\"></div><div><div class=\"box\" id=\"info\"></div><div class=\"box fen\" id=\"fen\"></div><div class=\"box fen\" id=\"pgn\"></div></div></div>"
        "<script>const pcs={P:'♙',N:'♘',B:'♗',R:'♖',Q:'♕',K:'♔',p:'♟',n:'♞',b:'♝',r:'♜',q:'♛',k:'♚'};"
        "function bit(arr,sq){let f=sq.charCodeAt(0)-97,r=sq.charCodeAt(1)-49;return ((arr[r]>>f)&1)!==0;}"
        "async function startGame(){await fetch('/api/start',{method:'POST'});update();}async function promote(p){await fetch('/api/promote?p='+p,{method:'POST'});update();}"
        "function cellPiece(fen,sq){let rows=fen.split(' ')[0].split('/');let r=8-parseInt(sq[1]),f=sq.charCodeAt(0)-97,x=0;for(const ch of rows[r]){if(ch>='1'&&ch<='8'){x+=parseInt(ch);}else{if(x===f)return pcs[ch]||'';x++;}}return '';}"
        "async function update(){let s=await (await fetch('/api/state')).json();let b=document.getElementById('board');b.innerHTML='';for(let r=8;r>=1;r--){for(let f=0;f<8;f++){let sq=String.fromCharCode(97+f)+r;let c='sq '+(((r+f)%2)?'dark':'light');if(bit(s.physical,sq))c+=' phys';if(bit(s.turn,sq))c+=' turn';if(bit(s.legal,sq))c+=' legal';if(bit(s.best,sq))c+=' best';if(bit(s.invalid,sq))c+=' invalid';if(bit(s.check,sq))c+=' check';if(s.selected===sq)c+=' sel';let d=document.createElement('div');d.className=c;d.textContent=cellPiece(s.fen,sq);b.appendChild(d);}}document.getElementById('info').innerHTML='<b>Mode:</b> '+s.mode+'<br><b>State:</b> '+s.state+'<br><b>Turn:</b> '+s.turn_text+'<br><b>Selected:</b> '+s.selected+'<br><b>Legal:</b> '+s.legal_text+'<br><b>Best:</b> '+s.best;document.getElementById('fen').textContent=s.fen;document.getElementById('pgn').textContent=s.pgn;}setInterval(update,500);update();</script></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static void jsonArray8(char * dst, size_t len, const uint8_t map[8])
{
    (void)snprintf(dst, len, "[%u,%u,%u,%u,%u,%u,%u,%u]", map[0], map[1], map[2], map[3], map[4], map[5], map[6], map[7]);
}

static esp_err_t apiStateHandler(httpd_req_t * req)
{
    char chunk[HTTP_CHUNK_LEN];
    char physical[64];
    char turn[64];
    char legal[64];
    char best[64];
    char invalid[64];
    char check[64];
    uint8_t turnMap[8] = {0U};

    for (uint8_t rank = 0U; rank < 8U; rank++)
    {
        turnMap[rank] = (game.side_to_move == CHESS_WHITE) ? 0U : 0U;
    }

    for (uint8_t row = 0U; row < 8U; row++)
    {
        for (uint8_t col = 0U; col < 8U; col++)
        {
            chess_piece_t p = game.board[row][col];
            if ((p.type != CHESS_EMPTY) && (p.color == game.side_to_move))
            {
                uint8_t rank = (uint8_t)(7U - row);
                turnMap[rank] = (uint8_t)(turnMap[rank] | ((uint8_t)(1U << col)));
            }
        }
    }

    jsonArray8(physical, sizeof(physical), physicalPresence);
    jsonArray8(turn, sizeof(turn), turnMap);
    jsonArray8(legal, sizeof(legal), legalMap);
    jsonArray8(best, sizeof(best), bestMap);
    jsonArray8(invalid, sizeof(invalid), invalidMap);
    jsonArray8(check, sizeof(check), checkMap);

    httpd_resp_set_type(req, "application/json");
    (void)snprintf(
        chunk,
        sizeof(chunk),
        "{\"mode\":\"%s\",\"state\":\"%s\",\"turn_text\":\"%s\",\"fen\":\"%s\",\"pgn\":\"%s\",\"selected\":\"%s\",\"legal_text\":\"%s\",\"best\":\"%s\",\"physical\":%s,\"turn\":%s,\"legal\":%s,\"best_map\":%s,\"best\":\"%s\",\"invalid\":%s,\"check\":%s}",
        modeToText(gameMode),
        stateText,
        (game.side_to_move == CHESS_WHITE) ? "white" : "black",
        game.fen,
        game.pgn,
        (selectedValid != 0U) ? selectedSquare : "-",
        stateLegal,
        stateBest,
        physical,
        turn,
        legal,
        best,
        stateBest,
        invalid,
        check
    );

    return httpd_resp_sendstr(req, chunk);
}

static esp_err_t apiStartHandler(httpd_req_t * req)
{
    game_command_t command = { GAME_COMMAND_START, CHESS_EMPTY };
    if (gameCommandQueue != NULL)
    {
        (void)xQueueSend(gameCommandQueue, &command, 0);
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t apiPromoteHandler(httpd_req_t * req)
{
    char query[32];
    char pieceText[4];
    game_command_t command = { GAME_COMMAND_PROMOTE, CHESS_QUEEN };

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        if (httpd_query_key_value(query, "p", pieceText, sizeof(pieceText)) == ESP_OK)
        {
            if (pieceText[0] == 'R') command.promotion = CHESS_ROOK;
            else if (pieceText[0] == 'B') command.promotion = CHESS_BISHOP;
            else if (pieceText[0] == 'N') command.promotion = CHESS_KNIGHT;
            else command.promotion = CHESS_QUEEN;
        }
    }

    if (gameCommandQueue != NULL)
    {
        (void)xQueueSend(gameCommandQueue, &command, 0);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

void serverTask(void * parameters)
{
    sensor_event_t event;
    BaseType_t status;

    (void)parameters;
    ESP_LOGI(TAG, "Chess controller started in setup mode");
    sendLedFrame(0U);

    for (;;)
    {
        processPendingCommands();
        status = xQueueReceive(sensorQueueRef, &event, pdMS_TO_TICKS(40U));

        if (status == pdPASS)
        {
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(1000U)) == pdPASS)
            {
                handleSensorEvent(&event);
                (void)xSemaphoreGive(stateMutex);
            }
        }
    }
}
