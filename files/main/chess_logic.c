#include "chess_logic.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const chess_piece_t EMPTY_PIECE = { CHESS_EMPTY, CHESS_COLOR_NONE, 0U };

static bool is_inside(int32_t row, int32_t col)
{
    return ((row >= 0) && (row < 8) && (col >= 0) && (col < 8));
}

chess_color_t chess_opposite(chess_color_t color)
{
    return (color == CHESS_WHITE) ? CHESS_BLACK : ((color == CHESS_BLACK) ? CHESS_WHITE : CHESS_COLOR_NONE);
}

static char piece_to_fen(chess_piece_t piece)
{
    char value = '1';

    switch (piece.type)
    {
        case CHESS_PAWN:   value = 'p'; break;
        case CHESS_ROOK:   value = 'r'; break;
        case CHESS_KNIGHT: value = 'n'; break;
        case CHESS_BISHOP: value = 'b'; break;
        case CHESS_QUEEN:  value = 'q'; break;
        case CHESS_KING:   value = 'k'; break;
        default:           value = '1'; break;
    }

    if ((piece.color == CHESS_WHITE) && (value >= 'a') && (value <= 'z'))
    {
        value = (char)(value - ('a' - 'A'));
    }

    return value;
}

void chess_index_to_square(uint8_t row, uint8_t col, char square[3])
{
    if (square != NULL)
    {
        square[0] = (char)('a' + (char)col);
        square[1] = (char)('8' - (char)row);
        square[2] = '\0';
    }
}

bool chess_square_to_index(const char square[3], uint8_t * row, uint8_t * col)
{
    bool ok = false;

    if ((square != NULL) && (row != NULL) && (col != NULL) &&
        (square[0] >= 'a') && (square[0] <= 'h') &&
        (square[1] >= '1') && (square[1] <= '8'))
    {
        *col = (uint8_t)(square[0] - 'a');
        *row = (uint8_t)('8' - square[1]);
        ok = true;
    }

    return ok;
}

chess_piece_t chess_get_piece(const chess_game_t * game, uint8_t row, uint8_t col)
{
    if ((game == NULL) || (row >= 8U) || (col >= 8U))
    {
        return EMPTY_PIECE;
    }

    return game->board[row][col];
}

void chess_generate_fen(chess_game_t * game)
{
    size_t pos = 0U;
    char castling[5];
    uint8_t castling_pos = 0U;

    if (game == NULL)
    {
        return;
    }

    for (uint8_t row = 0U; row < 8U; row++)
    {
        uint8_t empty_count = 0U;

        for (uint8_t col = 0U; col < 8U; col++)
        {
            chess_piece_t piece = game->board[row][col];

            if (piece.type == CHESS_EMPTY)
            {
                empty_count++;
            }
            else
            {
                if (empty_count > 0U)
                {
                    pos += (size_t)snprintf(&game->fen[pos], CHESS_FEN_LEN - pos, "%u", (unsigned int)empty_count);
                    empty_count = 0U;
                }

                if (pos < (CHESS_FEN_LEN - 1U))
                {
                    game->fen[pos++] = piece_to_fen(piece);
                }
            }
        }

        if (empty_count > 0U)
        {
            pos += (size_t)snprintf(&game->fen[pos], CHESS_FEN_LEN - pos, "%u", (unsigned int)empty_count);
        }

        if ((row < 7U) && (pos < (CHESS_FEN_LEN - 1U)))
        {
            game->fen[pos++] = '/';
        }
    }

    if ((game->board[7][4].type == CHESS_KING) && (game->board[7][4].color == CHESS_WHITE) && (game->board[7][4].moved == 0U))
    {
        if ((game->board[7][7].type == CHESS_ROOK) && (game->board[7][7].color == CHESS_WHITE) && (game->board[7][7].moved == 0U))
        {
            castling[castling_pos++] = 'K';
        }
        if ((game->board[7][0].type == CHESS_ROOK) && (game->board[7][0].color == CHESS_WHITE) && (game->board[7][0].moved == 0U))
        {
            castling[castling_pos++] = 'Q';
        }
    }

    if ((game->board[0][4].type == CHESS_KING) && (game->board[0][4].color == CHESS_BLACK) && (game->board[0][4].moved == 0U))
    {
        if ((game->board[0][7].type == CHESS_ROOK) && (game->board[0][7].color == CHESS_BLACK) && (game->board[0][7].moved == 0U))
        {
            castling[castling_pos++] = 'k';
        }
        if ((game->board[0][0].type == CHESS_ROOK) && (game->board[0][0].color == CHESS_BLACK) && (game->board[0][0].moved == 0U))
        {
            castling[castling_pos++] = 'q';
        }
    }

    castling[castling_pos] = '\0';

    pos += (size_t)snprintf(
        &game->fen[pos],
        CHESS_FEN_LEN - pos,
        " %c %s ",
        (game->side_to_move == CHESS_WHITE) ? 'w' : 'b',
        (castling_pos == 0U) ? "-" : castling
    );

    if ((game->en_passant_row >= 0) && (game->en_passant_col >= 0))
    {
        char ep[3];
        chess_index_to_square((uint8_t)game->en_passant_row, (uint8_t)game->en_passant_col, ep);
        pos += (size_t)snprintf(&game->fen[pos], CHESS_FEN_LEN - pos, "%s", ep);
    }
    else
    {
        pos += (size_t)snprintf(&game->fen[pos], CHESS_FEN_LEN - pos, "-");
    }

    (void)snprintf(&game->fen[pos], CHESS_FEN_LEN - pos, " %u %u", (unsigned int)game->halfmove_clock, (unsigned int)game->fullmove_number);
}

void chess_reset(chess_game_t * game)
{
    if (game == NULL)
    {
        return;
    }

    (void)memset(game, 0, sizeof(*game));

    for (uint8_t row = 0U; row < 8U; row++)
    {
        for (uint8_t col = 0U; col < 8U; col++)
        {
            game->board[row][col] = EMPTY_PIECE;
        }
    }

    for (uint8_t col = 0U; col < 8U; col++)
    {
        game->board[1][col] = (chess_piece_t){ CHESS_PAWN, CHESS_BLACK, 0U };
        game->board[6][col] = (chess_piece_t){ CHESS_PAWN, CHESS_WHITE, 0U };
    }

    game->board[0][0] = (chess_piece_t){ CHESS_ROOK, CHESS_BLACK, 0U };
    game->board[0][7] = (chess_piece_t){ CHESS_ROOK, CHESS_BLACK, 0U };
    game->board[7][0] = (chess_piece_t){ CHESS_ROOK, CHESS_WHITE, 0U };
    game->board[7][7] = (chess_piece_t){ CHESS_ROOK, CHESS_WHITE, 0U };
    game->board[0][1] = (chess_piece_t){ CHESS_KNIGHT, CHESS_BLACK, 0U };
    game->board[0][6] = (chess_piece_t){ CHESS_KNIGHT, CHESS_BLACK, 0U };
    game->board[7][1] = (chess_piece_t){ CHESS_KNIGHT, CHESS_WHITE, 0U };
    game->board[7][6] = (chess_piece_t){ CHESS_KNIGHT, CHESS_WHITE, 0U };
    game->board[0][2] = (chess_piece_t){ CHESS_BISHOP, CHESS_BLACK, 0U };
    game->board[0][5] = (chess_piece_t){ CHESS_BISHOP, CHESS_BLACK, 0U };
    game->board[7][2] = (chess_piece_t){ CHESS_BISHOP, CHESS_WHITE, 0U };
    game->board[7][5] = (chess_piece_t){ CHESS_BISHOP, CHESS_WHITE, 0U };
    game->board[0][3] = (chess_piece_t){ CHESS_QUEEN, CHESS_BLACK, 0U };
    game->board[0][4] = (chess_piece_t){ CHESS_KING, CHESS_BLACK, 0U };
    game->board[7][3] = (chess_piece_t){ CHESS_QUEEN, CHESS_WHITE, 0U };
    game->board[7][4] = (chess_piece_t){ CHESS_KING, CHESS_WHITE, 0U };

    game->side_to_move = CHESS_WHITE;
    game->en_passant_row = -1;
    game->en_passant_col = -1;
    game->fullmove_number = 1U;
    game->halfmove_clock = 0U;
    game->pgn_len = 0U;
    game->pgn[0] = '\0';

    chess_generate_fen(game);
}

static bool path_clear(const chess_game_t * game, uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)
{
    int32_t dr = (to_row > from_row) ? 1 : ((to_row < from_row) ? -1 : 0);
    int32_t dc = (to_col > from_col) ? 1 : ((to_col < from_col) ? -1 : 0);
    int32_t r = (int32_t)from_row + dr;
    int32_t c = (int32_t)from_col + dc;

    while ((r != (int32_t)to_row) || (c != (int32_t)to_col))
    {
        if (game->board[r][c].type != CHESS_EMPTY)
        {
            return false;
        }
        r += dr;
        c += dc;
    }

    return true;
}

static bool square_attacked(const chess_game_t * game, uint8_t row, uint8_t col, chess_color_t attacker)
{
    static const int8_t knight_offsets[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    static const int8_t king_offsets[8][2] = {
        {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}
    };
    static const int8_t line_dirs[8][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };

    int32_t pawn_dir = (attacker == CHESS_WHITE) ? -1 : 1;
    int32_t pawn_row = (int32_t)row - pawn_dir;

    for (int32_t dc = -1; dc <= 1; dc += 2)
    {
        int32_t pc = (int32_t)col + dc;
        if (is_inside(pawn_row, pc))
        {
            chess_piece_t p = game->board[pawn_row][pc];
            if ((p.type == CHESS_PAWN) && (p.color == attacker))
            {
                return true;
            }
        }
    }

    for (uint8_t i = 0U; i < 8U; i++)
    {
        int32_t r = (int32_t)row + knight_offsets[i][0];
        int32_t c = (int32_t)col + knight_offsets[i][1];
        if (is_inside(r, c))
        {
            chess_piece_t p = game->board[r][c];
            if ((p.type == CHESS_KNIGHT) && (p.color == attacker))
            {
                return true;
            }
        }
    }

    for (uint8_t i = 0U; i < 8U; i++)
    {
        int32_t r = (int32_t)row + king_offsets[i][0];
        int32_t c = (int32_t)col + king_offsets[i][1];
        if (is_inside(r, c))
        {
            chess_piece_t p = game->board[r][c];
            if ((p.type == CHESS_KING) && (p.color == attacker))
            {
                return true;
            }
        }
    }

    for (uint8_t i = 0U; i < 8U; i++)
    {
        int32_t r = (int32_t)row + line_dirs[i][0];
        int32_t c = (int32_t)col + line_dirs[i][1];

        while (is_inside(r, c))
        {
            chess_piece_t p = game->board[r][c];
            if (p.type != CHESS_EMPTY)
            {
                if (p.color == attacker)
                {
                    bool diagonal = (line_dirs[i][0] != 0) && (line_dirs[i][1] != 0);
                    if ((p.type == CHESS_QUEEN) ||
                        ((p.type == CHESS_ROOK) && (diagonal == false)) ||
                        ((p.type == CHESS_BISHOP) && (diagonal == true)))
                    {
                        return true;
                    }
                }
                break;
            }
            r += line_dirs[i][0];
            c += line_dirs[i][1];
        }
    }

    return false;
}

bool chess_find_king(const chess_game_t * game, chess_color_t side, uint8_t * row, uint8_t * col)
{
    if ((game == NULL) || (row == NULL) || (col == NULL))
    {
        return false;
    }

    for (uint8_t r = 0U; r < 8U; r++)
    {
        for (uint8_t c = 0U; c < 8U; c++)
        {
            chess_piece_t p = game->board[r][c];
            if ((p.type == CHESS_KING) && (p.color == side))
            {
                *row = r;
                *col = c;
                return true;
            }
        }
    }

    return false;
}

bool chess_is_check(const chess_game_t * game, chess_color_t side)
{
    uint8_t row;
    uint8_t col;

    if ((game == NULL) || (chess_find_king(game, side, &row, &col) == false))
    {
        return false;
    }

    return square_attacked(game, row, col, chess_opposite(side));
}

static bool pseudo_legal(const chess_game_t * game, const chess_move_t * move)
{
    chess_piece_t src;
    chess_piece_t dst;
    int32_t dr;
    int32_t dc;
    int32_t abs_dr;
    int32_t abs_dc;

    if ((game == NULL) || (move == NULL) ||
        (move->from_row >= 8U) || (move->from_col >= 8U) ||
        (move->to_row >= 8U) || (move->to_col >= 8U))
    {
        return false;
    }

    src = game->board[move->from_row][move->from_col];
    dst = game->board[move->to_row][move->to_col];

    if ((src.type == CHESS_EMPTY) || (src.color != game->side_to_move) ||
        ((dst.type != CHESS_EMPTY) && (dst.color == src.color)))
    {
        return false;
    }

    dr = (int32_t)move->to_row - (int32_t)move->from_row;
    dc = (int32_t)move->to_col - (int32_t)move->from_col;
    abs_dr = abs(dr);
    abs_dc = abs(dc);

    switch (src.type)
    {
        case CHESS_KNIGHT:
            return (((abs_dr == 2) && (abs_dc == 1)) || ((abs_dr == 1) && (abs_dc == 2)));

        case CHESS_BISHOP:
            return ((abs_dr == abs_dc) && path_clear(game, move->from_row, move->from_col, move->to_row, move->to_col));

        case CHESS_ROOK:
            return (((abs_dr == 0) || (abs_dc == 0)) && ((abs_dr + abs_dc) > 0) && path_clear(game, move->from_row, move->from_col, move->to_row, move->to_col));

        case CHESS_QUEEN:
            return ((((abs_dr == abs_dc) || (abs_dr == 0) || (abs_dc == 0)) && ((abs_dr + abs_dc) > 0)) && path_clear(game, move->from_row, move->from_col, move->to_row, move->to_col));

        case CHESS_KING:
            if ((abs_dr <= 1) && (abs_dc <= 1))
            {
                return (square_attacked(game, move->to_row, move->to_col, chess_opposite(src.color)) == false);
            }
            if ((abs_dr == 0) && (abs_dc == 2) && (src.moved == 0U) && (chess_is_check(game, src.color) == false))
            {
                uint8_t rook_col = (dc > 0) ? 7U : 0U;
                uint8_t through_col = (dc > 0) ? 5U : 3U;
                uint8_t dest_col = (dc > 0) ? 6U : 2U;
                chess_piece_t rook = game->board[move->from_row][rook_col];
                if ((rook.type != CHESS_ROOK) || (rook.color != src.color) || (rook.moved != 0U))
                {
                    return false;
                }
                if (dc > 0)
                {
                    if ((game->board[move->from_row][5].type != CHESS_EMPTY) || (game->board[move->from_row][6].type != CHESS_EMPTY))
                    {
                        return false;
                    }
                }
                else
                {
                    if ((game->board[move->from_row][1].type != CHESS_EMPTY) || (game->board[move->from_row][2].type != CHESS_EMPTY) || (game->board[move->from_row][3].type != CHESS_EMPTY))
                    {
                        return false;
                    }
                }
                return ((square_attacked(game, move->from_row, through_col, chess_opposite(src.color)) == false) &&
                        (square_attacked(game, move->from_row, dest_col, chess_opposite(src.color)) == false));
            }
            return false;

        case CHESS_PAWN:
        {
            int32_t dir = (src.color == CHESS_WHITE) ? -1 : 1;
            uint8_t start_row = (src.color == CHESS_WHITE) ? 6U : 1U;

            if ((dc == 0) && (dr == dir) && (dst.type == CHESS_EMPTY))
            {
                return true;
            }
            if ((dc == 0) && (dr == (2 * dir)) && (move->from_row == start_row) && (dst.type == CHESS_EMPTY) &&
                (game->board[(uint8_t)((int32_t)move->from_row + dir)][move->from_col].type == CHESS_EMPTY))
            {
                return true;
            }
            if ((abs_dc == 1) && (dr == dir) && (dst.type != CHESS_EMPTY) && (dst.color != src.color))
            {
                return true;
            }
            if ((abs_dc == 1) && (dr == dir) && (dst.type == CHESS_EMPTY) &&
                ((int8_t)move->to_row == game->en_passant_row) && ((int8_t)move->to_col == game->en_passant_col))
            {
                return true;
            }
            return false;
        }

        default:
            return false;
    }
}

static void apply_unchecked(chess_game_t * game, const chess_move_t * move, bool update_log)
{
    chess_piece_t src = game->board[move->from_row][move->from_col];
    chess_piece_t dst = game->board[move->to_row][move->to_col];
    bool pawn_move = (src.type == CHESS_PAWN);
    bool capture = (dst.type != CHESS_EMPTY);
    bool en_passant_capture = false;
    char from[3];
    char to[3];

    chess_index_to_square(move->from_row, move->from_col, from);
    chess_index_to_square(move->to_row, move->to_col, to);

    if ((src.type == CHESS_PAWN) && (move->from_col != move->to_col) && (dst.type == CHESS_EMPTY))
    {
        uint8_t captured_row = move->from_row;
        game->board[captured_row][move->to_col] = EMPTY_PIECE;
        en_passant_capture = true;
        capture = true;
    }

    if ((src.type == CHESS_KING) && (abs((int32_t)move->to_col - (int32_t)move->from_col) == 2))
    {
        if (move->to_col > move->from_col)
        {
            game->board[move->from_row][5] = game->board[move->from_row][7];
            game->board[move->from_row][5].moved = 1U;
            game->board[move->from_row][7] = EMPTY_PIECE;
        }
        else
        {
            game->board[move->from_row][3] = game->board[move->from_row][0];
            game->board[move->from_row][3].moved = 1U;
            game->board[move->from_row][0] = EMPTY_PIECE;
        }
    }

    game->board[move->to_row][move->to_col] = src;
    game->board[move->to_row][move->to_col].moved = 1U;
    game->board[move->from_row][move->from_col] = EMPTY_PIECE;

    if ((src.type == CHESS_PAWN) && ((move->to_row == 0U) || (move->to_row == 7U)))
    {
        chess_piece_type_t promo = move->promotion;
        if ((promo != CHESS_ROOK) && (promo != CHESS_BISHOP) && (promo != CHESS_KNIGHT) && (promo != CHESS_QUEEN))
        {
            promo = CHESS_QUEEN;
        }
        game->board[move->to_row][move->to_col].type = promo;
    }

    game->en_passant_row = -1;
    game->en_passant_col = -1;

    if ((src.type == CHESS_PAWN) && (abs((int32_t)move->to_row - (int32_t)move->from_row) == 2))
    {
        game->en_passant_row = (int8_t)(((int32_t)move->from_row + (int32_t)move->to_row) / 2);
        game->en_passant_col = (int8_t)move->from_col;
    }

    if ((pawn_move == true) || (capture == true))
    {
        game->halfmove_clock = 0U;
    }
    else
    {
        game->halfmove_clock++;
    }

    if (game->side_to_move == CHESS_BLACK)
    {
        game->fullmove_number++;
    }

    if ((update_log == true) && (game->pgn_len < (CHESS_PGN_LEN - 16U)))
    {
        if (src.color == CHESS_WHITE)
        {
            game->pgn_len += (size_t)snprintf(&game->pgn[game->pgn_len], CHESS_PGN_LEN - game->pgn_len, "%u. ", (unsigned int)game->fullmove_number);
        }
        game->pgn_len += (size_t)snprintf(&game->pgn[game->pgn_len], CHESS_PGN_LEN - game->pgn_len, "%s%s%s ", from, (capture || en_passant_capture) ? "x" : "-", to);
    }

    game->side_to_move = chess_opposite(game->side_to_move);
    chess_generate_fen(game);
}

bool chess_is_legal_move(const chess_game_t * game, const chess_move_t * move)
{
    chess_game_t copy;
    chess_color_t moving_side;

    if ((game == NULL) || (move == NULL) || (pseudo_legal(game, move) == false))
    {
        return false;
    }

    moving_side = game->side_to_move;
    copy = *game;
    apply_unchecked(&copy, move, false);

    return (chess_is_check(&copy, moving_side) == false);
}

bool chess_apply_move(chess_game_t * game, const chess_move_t * move)
{
    if ((game == NULL) || (move == NULL) || (chess_is_legal_move(game, move) == false))
    {
        return false;
    }

    apply_unchecked(game, move, true);
    return true;
}

bool chess_is_promotion_move(const chess_game_t * game, const chess_move_t * move)
{
    chess_piece_t piece;

    if ((game == NULL) || (move == NULL) || (move->from_row >= 8U) || (move->from_col >= 8U))
    {
        return false;
    }

    piece = game->board[move->from_row][move->from_col];
    return ((piece.type == CHESS_PAWN) && ((move->to_row == 0U) || (move->to_row == 7U)));
}

uint32_t chess_generate_legal_moves_from(const chess_game_t * game, uint8_t row, uint8_t col, chess_move_t moves[], uint32_t max_moves)
{
    uint32_t count = 0U;

    if ((game == NULL) || (moves == NULL) || (row >= 8U) || (col >= 8U))
    {
        return 0U;
    }

    if ((game->board[row][col].type == CHESS_EMPTY) || (game->board[row][col].color != game->side_to_move))
    {
        return 0U;
    }

    for (uint8_t to_row = 0U; to_row < 8U; to_row++)
    {
        for (uint8_t to_col = 0U; to_col < 8U; to_col++)
        {
            chess_move_t move = { row, col, to_row, to_col, CHESS_QUEEN };
            if (chess_is_legal_move(game, &move) == true)
            {
                if (count < max_moves)
                {
                    moves[count] = move;
                    count++;
                }
            }
        }
    }

    return count;
}

uint32_t chess_generate_all_legal_moves(const chess_game_t * game, chess_move_t moves[], uint32_t max_moves)
{
    uint32_t count = 0U;

    if ((game == NULL) || (moves == NULL))
    {
        return 0U;
    }

    for (uint8_t row = 0U; row < 8U; row++)
    {
        for (uint8_t col = 0U; col < 8U; col++)
        {
            chess_move_t local[CHESS_MAX_MOVES];
            uint32_t local_count = chess_generate_legal_moves_from(game, row, col, local, CHESS_MAX_MOVES);

            for (uint32_t i = 0U; (i < local_count) && (count < max_moves); i++)
            {
                moves[count] = local[i];
                count++;
            }
        }
    }

    return count;
}

bool chess_is_checkmate(const chess_game_t * game, chess_color_t side)
{
    chess_game_t copy;
    chess_move_t moves[CHESS_MAX_MOVES];

    if ((game == NULL) || (chess_is_check(game, side) == false))
    {
        return false;
    }

    copy = *game;
    copy.side_to_move = side;

    return (chess_generate_all_legal_moves(&copy, moves, CHESS_MAX_MOVES) == 0U);
}
