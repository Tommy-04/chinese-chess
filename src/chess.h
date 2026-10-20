#ifndef CHINESE_CHESS_H
#define CHINESE_CHESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHESS_ROWS 10
#define CHESS_COLS 9
#define CHESS_MAX_LEGAL_MOVES 90

typedef enum {
    CHESS_EMPTY = 0,
    CHESS_KING = 1,
    CHESS_ADVISOR = 2,
    CHESS_ELEPHANT = 3,
    CHESS_HORSE = 4,
    CHESS_ROOK = 5,
    CHESS_CANNON = 6,
    CHESS_PAWN = 7
} ChessPieceType;

typedef enum {
    CHESS_NONE = 0,
    CHESS_RED = 1,
    CHESS_BLACK = -1
} ChessColor;

typedef enum {
    CHESS_ONGOING = 0,
    CHESS_RED_WIN,
    CHESS_BLACK_WIN,
    CHESS_DRAW
} ChessResult;

typedef struct {
    int board[CHESS_ROWS][CHESS_COLS];
    ChessColor turn;
    ChessResult result;
    unsigned move_count;
} ChessGame;

void chess_init(ChessGame *game);
int chess_in_bounds(int x, int y);
ChessColor chess_piece_color(int piece);
ChessPieceType chess_piece_type(int piece);
int chess_is_legal_move(const ChessGame *game, int from_x, int from_y, int to_x, int to_y);
int chess_apply_move(ChessGame *game, int from_x, int from_y, int to_x, int to_y);
int chess_is_in_check(const ChessGame *game, ChessColor color);
size_t chess_legal_destinations(const ChessGame *game, int from_x, int from_y,
                                int out_x[CHESS_MAX_LEGAL_MOVES],
                                int out_y[CHESS_MAX_LEGAL_MOVES]);
int chess_has_legal_move(const ChessGame *game, ChessColor color);
const char *chess_result_text(ChessResult result);

#ifdef __cplusplus
}
#endif

#endif
