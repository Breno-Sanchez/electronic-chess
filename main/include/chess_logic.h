#ifndef CHESS_LOGIC_H
#define CHESS_LOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHESS_BOARD_SIZE          (8U)
#define CHESS_MAX_MOVES           (128U)
#define CHESS_FEN_LEN             (128U)
#define CHESS_PGN_LEN             (2048U)

typedef enum
{
    CHESS_COLOR_NONE = 0,
    CHESS_WHITE = 1,
    CHESS_BLACK = 2
} chess_color_t;

typedef enum
{
    CHESS_EMPTY = 0,
    CHESS_PAWN,
    CHESS_ROOK,
    CHESS_KNIGHT,
    CHESS_BISHOP,
    CHESS_QUEEN,
    CHESS_KING
} chess_piece_type_t;

typedef struct
{
    chess_piece_type_t type;
    chess_color_t color;
    uint8_t moved;
} chess_piece_t;

typedef struct
{
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
    chess_piece_type_t promotion;
} chess_move_t;

typedef struct
{
    chess_piece_t board[CHESS_BOARD_SIZE][CHESS_BOARD_SIZE];
    chess_color_t side_to_move;
    int8_t en_passant_row;
    int8_t en_passant_col;
    uint16_t halfmove_clock;
    uint16_t fullmove_number;
    char fen[CHESS_FEN_LEN];
    char pgn[CHESS_PGN_LEN];
    size_t pgn_len;
} chess_game_t;

void chess_reset(chess_game_t * game);
void chess_generate_fen(chess_game_t * game);
bool chess_square_to_index(const char square[3], uint8_t * row, uint8_t * col);
void chess_index_to_square(uint8_t row, uint8_t col, char square[3]);
chess_piece_t chess_get_piece(const chess_game_t * game, uint8_t row, uint8_t col);
chess_color_t chess_opposite(chess_color_t color);
bool chess_is_legal_move(const chess_game_t * game, const chess_move_t * move);
bool chess_apply_move(chess_game_t * game, const chess_move_t * move);
uint32_t chess_generate_legal_moves_from(const chess_game_t * game, uint8_t row, uint8_t col, chess_move_t moves[], uint32_t max_moves);
uint32_t chess_generate_all_legal_moves(const chess_game_t * game, chess_move_t moves[], uint32_t max_moves);
bool chess_is_check(const chess_game_t * game, chess_color_t side);
bool chess_is_checkmate(const chess_game_t * game, chess_color_t side);
bool chess_find_king(const chess_game_t * game, chess_color_t side, uint8_t * row, uint8_t * col);
bool chess_is_promotion_move(const chess_game_t * game, const chess_move_t * move);

#ifdef __cplusplus
}
#endif

#endif
