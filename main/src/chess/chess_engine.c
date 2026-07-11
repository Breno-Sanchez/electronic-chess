#include "chess_engine.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define ENGINE_MATE_SCORE          (30000)
#define ENGINE_INVALID_SCORE       (-30000)
#define ENGINE_CENTER_BONUS        (12)
#define ENGINE_INNER_CENTER_BONUS  (8)
#define ENGINE_CHECK_BONUS         (45)
#define ENGINE_MOBILITY_WEIGHT     (2)

static int piece_value_cp(chess_piece_type_t type)
{
    int value = 0;

    switch (type)
    {
        case CHESS_PAWN:
            value = 100;
            break;

        case CHESS_KNIGHT:
            value = 300;
            break;

        case CHESS_BISHOP:
            value = 300;
            break;

        case CHESS_ROOK:
            value = 500;
            break;

        case CHESS_QUEEN:
            value = 900;
            break;

        case CHESS_KING:
        case CHESS_EMPTY:
        default:
            value = 0;
            break;
    }

    return value;
}

static chess_move_t normalized_move(const chess_game_t * game, const chess_move_t * move)
{
    chess_move_t normalized = *move;
    chess_piece_t moving;

    if ((game != NULL) &&
        (move != NULL) &&
        (move->from_row < CHESS_BOARD_SIZE) &&
        (move->from_col < CHESS_BOARD_SIZE))
    {
        moving = game->board[move->from_row][move->from_col];

        if ((moving.type == CHESS_PAWN) &&
            ((move->to_row == 0U) || (move->to_row == 7U)) &&
            ((move->promotion != CHESS_QUEEN) &&
             (move->promotion != CHESS_ROOK) &&
             (move->promotion != CHESS_BISHOP) &&
             (move->promotion != CHESS_KNIGHT)))
        {
            normalized.promotion = CHESS_QUEEN;
        }
    }

    return normalized;
}

static int center_score(uint8_t row, uint8_t col)
{
    int score = 0;

    if ((row >= 2U) && (row <= 5U) && (col >= 2U) && (col <= 5U))
    {
        score += ENGINE_CENTER_BONUS;
    }

    if ((row >= 3U) && (row <= 4U) && (col >= 3U) && (col <= 4U))
    {
        score += ENGINE_INNER_CENTER_BONUS;
    }

    return score;
}

static int development_score(const chess_game_t * game, const chess_move_t * move)
{
    int score = 0;
    chess_piece_t src;

    if ((game == NULL) || (move == NULL) ||
        (move->from_row >= CHESS_BOARD_SIZE) ||
        (move->from_col >= CHESS_BOARD_SIZE))
    {
        return 0;
    }

    src = game->board[move->from_row][move->from_col];

    if ((src.type == CHESS_KNIGHT) || (src.type == CHESS_BISHOP))
    {
        if (((src.color == CHESS_WHITE) && (move->from_row == 0U)) ||
            ((src.color == CHESS_BLACK) && (move->from_row == 7U)))
        {
            score += 18;
        }
    }

    if ((src.type == CHESS_KING) &&
        (((move->from_col + 2U) == move->to_col) ||
         ((move->to_col + 2U) == move->from_col)))
    {
        score += 35;
    }

    return score;
}

static int evaluate_position(const chess_game_t * game, chess_color_t root_side)
{
    int score = 0;
    chess_color_t opponent;
    chess_move_t moves[CHESS_MAX_MOVES];
    uint32_t mobility;

    if (game == NULL)
    {
        return ENGINE_INVALID_SCORE;
    }

    opponent = chess_opposite(root_side);

    if (chess_is_checkmate(game, root_side) == true)
    {
        return -ENGINE_MATE_SCORE;
    }

    if (chess_is_checkmate(game, opponent) == true)
    {
        return ENGINE_MATE_SCORE;
    }

    for (uint8_t row = 0U; row < CHESS_BOARD_SIZE; row++)
    {
        for (uint8_t col = 0U; col < CHESS_BOARD_SIZE; col++)
        {
            chess_piece_t piece = game->board[row][col];

            if (piece.type != CHESS_EMPTY)
            {
                int value = piece_value_cp(piece.type) + center_score(row, col);

                if (piece.color == root_side)
                {
                    score += value;
                }
                else if (piece.color == opponent)
                {
                    score -= value;
                }
            }
        }
    }

    if (chess_is_check(game, opponent) == true)
    {
        score += ENGINE_CHECK_BONUS;
    }

    if (chess_is_check(game, root_side) == true)
    {
        score -= ENGINE_CHECK_BONUS;
    }

    {
        chess_game_t temp = *game;

        temp.side_to_move = root_side;
        mobility = chess_generate_all_legal_moves(&temp, moves, CHESS_MAX_MOVES);
        score += (int)mobility * ENGINE_MOBILITY_WEIGHT;

        temp = *game;
        temp.side_to_move = opponent;
        mobility = chess_generate_all_legal_moves(&temp, moves, CHESS_MAX_MOVES);
        score -= (int)mobility * ENGINE_MOBILITY_WEIGHT;
    }

    return score;
}

static int score_after_opponent_reply(const chess_game_t * game, const chess_move_t * move)
{
    chess_game_t after_move;
    chess_color_t root_side;
    chess_color_t opponent;
    chess_move_t normalized;
    chess_move_t replies[CHESS_MAX_MOVES];
    uint32_t reply_count;
    int worst_score = INT_MAX;

    if ((game == NULL) || (move == NULL))
    {
        return ENGINE_INVALID_SCORE;
    }

    root_side = game->side_to_move;
    opponent = chess_opposite(root_side);
    normalized = normalized_move(game, move);
    after_move = *game;

    if (chess_apply_move(&after_move, &normalized) == false)
    {
        return ENGINE_INVALID_SCORE;
    }

    if (chess_is_checkmate(&after_move, opponent) == true)
    {
        return ENGINE_MATE_SCORE - 1;
    }

    reply_count = chess_generate_all_legal_moves(&after_move, replies, CHESS_MAX_MOVES);

    if (reply_count == 0U)
    {
        return evaluate_position(&after_move, root_side);
    }

    for (uint32_t index = 0U; index < reply_count; index++)
    {
        chess_game_t after_reply = after_move;
        chess_move_t reply = normalized_move(&after_move, &replies[index]);
        int score;

        if (chess_apply_move(&after_reply, &reply) == false)
        {
            continue;
        }

        score = evaluate_position(&after_reply, root_side);

        if (score < worst_score)
        {
            worst_score = score;
        }
    }

    if (worst_score == INT_MAX)
    {
        worst_score = evaluate_position(&after_move, root_side);
    }

    worst_score += development_score(game, &normalized);
    worst_score += center_score(normalized.to_row, normalized.to_col);

    return worst_score;
}

static bool best_from_list(const chess_game_t * game,
                           const chess_move_t moves[],
                           uint32_t count,
                           chess_move_t * best_move)
{
    bool found = false;
    int best_score = ENGINE_INVALID_SCORE;

    if ((game == NULL) || (moves == NULL) || (best_move == NULL))
    {
        return false;
    }

    for (uint32_t index = 0U; index < count; index++)
    {
        int score = score_after_opponent_reply(game, &moves[index]);

        if ((found == false) || (score > best_score))
        {
            best_score = score;
            *best_move = normalized_move(game, &moves[index]);
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
