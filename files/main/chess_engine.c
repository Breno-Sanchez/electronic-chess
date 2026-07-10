#include "chess_engine.h"

#include <limits.h>
#include <stdlib.h>

static int piece_value(chess_piece_type_t type)
{
    switch (type)
    {
        case CHESS_PAWN:   return 100;
        case CHESS_KNIGHT: return 320;
        case CHESS_BISHOP: return 330;
        case CHESS_ROOK:   return 500;
        case CHESS_QUEEN:  return 900;
        case CHESS_KING:   return 20000;
        default:           return 0;
    }
}

static int evaluate_for_side(const chess_game_t * game, chess_color_t side)
{
    int score = 0;

    for (uint8_t row = 0U; row < 8U; row++)
    {
        for (uint8_t col = 0U; col < 8U; col++)
        {
            chess_piece_t piece = game->board[row][col];
            int value = piece_value(piece.type);

            if (piece.type == CHESS_EMPTY)
            {
                continue;
            }

            if ((row >= 2U) && (row <= 5U) && (col >= 2U) && (col <= 5U))
            {
                value += 8;
            }

            if (piece.color == side)
            {
                score += value;
            }
            else
            {
                score -= value;
            }
        }
    }

    if (chess_is_check(game, chess_opposite(side)) == true)
    {
        score += 45;
    }

    if (chess_is_check(game, side) == true)
    {
        score -= 60;
    }

    return score;
}

static int negamax(chess_game_t game, uint8_t depth, int alpha, int beta, chess_color_t root_side)
{
    chess_move_t moves[CHESS_MAX_MOVES];
    uint32_t count;
    int best = INT_MIN / 4;

    if (depth == 0U)
    {
        return evaluate_for_side(&game, root_side);
    }

    if (chess_is_checkmate(&game, game.side_to_move) == true)
    {
        return (game.side_to_move == root_side) ? -30000 : 30000;
    }

    count = chess_generate_all_legal_moves(&game, moves, CHESS_MAX_MOVES);

    if (count == 0U)
    {
        return evaluate_for_side(&game, root_side);
    }

    for (uint32_t i = 0U; i < count; i++)
    {
        chess_game_t next = game;
        int score;

        (void)chess_apply_move(&next, &moves[i]);
        score = -negamax(next, (uint8_t)(depth - 1U), -beta, -alpha, root_side);

        if (score > best)
        {
            best = score;
        }
        if (score > alpha)
        {
            alpha = score;
        }
        if (alpha >= beta)
        {
            break;
        }
    }

    return best;
}

static bool best_from_list(const chess_game_t * game, const chess_move_t moves[], uint32_t count, chess_move_t * best_move)
{
    bool found = false;
    int best_score = INT_MIN / 4;
    chess_color_t root_side;

    if ((game == NULL) || (moves == NULL) || (best_move == NULL))
    {
        return false;
    }

    root_side = game->side_to_move;

    for (uint32_t i = 0U; i < count; i++)
    {
        chess_game_t next = *game;
        int score;

        (void)chess_apply_move(&next, &moves[i]);
        score = -negamax(next, 1U, -30000, 30000, root_side);

        if ((found == false) || (score > best_score))
        {
            best_score = score;
            *best_move = moves[i];
            found = true;
        }
    }

    return found;
}

bool chess_engine_best_for_origin(const chess_game_t * game, uint8_t row, uint8_t col, chess_move_t * best_move)
{
    chess_move_t moves[CHESS_MAX_MOVES];
    uint32_t count = chess_generate_legal_moves_from(game, row, col, moves, CHESS_MAX_MOVES);
    return best_from_list(game, moves, count, best_move);
}

bool chess_engine_best_global(const chess_game_t * game, chess_move_t * best_move)
{
    chess_move_t moves[CHESS_MAX_MOVES];
    uint32_t count = chess_generate_all_legal_moves(game, moves, CHESS_MAX_MOVES);
    return best_from_list(game, moves, count, best_move);
}
