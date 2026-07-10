#include "chess_engine.h"

#include <limits.h>
#include <stdint.h>

static int piece_value(chess_piece_type_t type)
{
    int value = 0;

    switch (type)
    {
        case CHESS_PAWN:   value = 100; break;
        case CHESS_KNIGHT: value = 320; break;
        case CHESS_BISHOP: value = 330; break;
        case CHESS_ROOK:   value = 500; break;
        case CHESS_QUEEN:  value = 900; break;
        case CHESS_KING:   value = 20000; break;
        default:           value = 0; break;
    }

    return value;
}

static int abs_i32(int32_t value)
{
    return (value < 0) ? (int)(-value) : (int)value;
}

static int center_score(uint8_t row, uint8_t col)
{
    int score = 0;

    if ((row >= 2U) && (row <= 5U) && (col >= 2U) && (col <= 5U))
    {
        score += 12;
    }

    if ((row >= 3U) && (row <= 4U) && (col >= 3U) && (col <= 4U))
    {
        score += 8;
    }

    return score;
}

static int development_score(const chess_game_t * game, const chess_move_t * move)
{
    int score = 0;
    chess_piece_t src;

    if ((game == NULL) || (move == NULL))
    {
        return 0;
    }

    src = game->board[move->from_row][move->from_col];

    if ((src.type == CHESS_KNIGHT) || (src.type == CHESS_BISHOP))
    {
        if (((src.color == CHESS_WHITE) && (move->from_row == 7U)) ||
            ((src.color == CHESS_BLACK) && (move->from_row == 0U)))
        {
            score += 14;
        }
    }

    if (src.type == CHESS_PAWN)
    {
        int32_t delta = (int32_t)move->to_row - (int32_t)move->from_row;

        if (abs_i32(delta) == 2)
        {
            score += 6;
        }
    }

    return score;
}

static int move_score(const chess_game_t * game, const chess_move_t * move)
{
    int score = 0;
    chess_piece_t src;
    chess_piece_t dst;

    if ((game == NULL) || (move == NULL) ||
        (move->from_row >= 8U) || (move->from_col >= 8U) ||
        (move->to_row >= 8U) || (move->to_col >= 8U))
    {
        return INT_MIN / 4;
    }

    src = game->board[move->from_row][move->from_col];
    dst = game->board[move->to_row][move->to_col];

    if ((src.type == CHESS_EMPTY) || (src.color != game->side_to_move))
    {
        return INT_MIN / 4;
    }

    score += piece_value(dst.type) * 10;
    score += center_score(move->to_row, move->to_col);
    score += development_score(game, move);

    if ((src.type == CHESS_PAWN) && ((move->to_row == 0U) || (move->to_row == 7U)))
    {
        chess_piece_type_t promoted = move->promotion;

        if ((promoted != CHESS_QUEEN) &&
            (promoted != CHESS_ROOK) &&
            (promoted != CHESS_BISHOP) &&
            (promoted != CHESS_KNIGHT))
        {
            promoted = CHESS_QUEEN;
        }

        score += piece_value(promoted);
    }

    if (src.type == CHESS_KING)
    {
        int32_t castle_delta = (int32_t)move->to_col - (int32_t)move->from_col;

        if (abs_i32(castle_delta) == 2)
        {
            score += 25;
        }
    }

    return score;
}

static bool best_from_list(const chess_game_t * game,
                           const chess_move_t moves[],
                           uint32_t count,
                           chess_move_t * best_move)
{
    bool found = false;
    int best_score = INT_MIN / 4;

    if ((game == NULL) || (moves == NULL) || (best_move == NULL))
    {
        return false;
    }

    for (uint32_t index = 0U; index < count; index++)
    {
        int score = move_score(game, &moves[index]);

        if ((found == false) || (score > best_score))
        {
            best_score = score;
            *best_move = moves[index];
            found = true;
        }
    }

    return found;
}

bool chess_engine_best_for_origin(const chess_game_t * game,
                                  uint8_t row,
                                  uint8_t col,
                                  chess_move_t * best_move)
{
    chess_move_t moves[CHESS_MAX_MOVES];
    uint32_t count;

    if ((game == NULL) || (best_move == NULL))
    {
        return false;
    }

    count = chess_generate_legal_moves_from(game, row, col, moves, CHESS_MAX_MOVES);
    return best_from_list(game, moves, count, best_move);
}

bool chess_engine_best_global(const chess_game_t * game, chess_move_t * best_move)
{
    chess_move_t moves[CHESS_MAX_MOVES];
    uint32_t count;

    if ((game == NULL) || (best_move == NULL))
    {
        return false;
    }

    count = chess_generate_all_legal_moves(game, moves, CHESS_MAX_MOVES);
    return best_from_list(game, moves, count, best_move);
}
