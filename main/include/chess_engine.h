#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include <stdbool.h>
#include "chess_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

bool chess_engine_best_for_origin(const chess_game_t * game, uint8_t row, uint8_t col, chess_move_t * best_move);
bool chess_engine_best_global(const chess_game_t * game, chess_move_t * best_move);

#ifdef __cplusplus
}
#endif

#endif
