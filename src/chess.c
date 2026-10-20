#include "chess.h"

#include <string.h>

static int abs_i(int value) { return value < 0 ? -value : value; }

ChessColor chess_piece_color(int piece)
{
    return piece > 0 ? CHESS_RED : (piece < 0 ? CHESS_BLACK : CHESS_NONE);
}

ChessPieceType chess_piece_type(int piece)
{
    int value = abs_i(piece);
    return value >= CHESS_KING && value <= CHESS_PAWN ? (ChessPieceType)value : CHESS_EMPTY;
}

int chess_in_bounds(int x, int y)
{
    return x >= 0 && x < CHESS_COLS && y >= 0 && y < CHESS_ROWS;
}

static int sign_i(int value) { return value > 0 ? 1 : (value < 0 ? -1 : 0); }

static int count_between(const ChessGame *game, int x1, int y1, int x2, int y2)
{
    int count = 0;
    int x = x1 + sign_i(x2 - x1);
    int y = y1 + sign_i(y2 - y1);
    while (x != x2 || y != y2) {
        if (game->board[y][x] != CHESS_EMPTY) count++;
        x += sign_i(x2 - x1);
        y += sign_i(y2 - y1);
    }
    return count;
}

static int in_palace(ChessColor color, int x, int y)
{
    if (x < 3 || x > 5) return 0;
    return color == CHESS_RED ? (y >= 7 && y <= 9) : (y >= 0 && y <= 2);
}

static int elephant_side_ok(ChessColor color, int y)
{
    return color == CHESS_RED ? y >= 5 : y <= 4;
}

static int pawn_can_attack(ChessColor color, int from_x, int from_y, int to_x, int to_y)
{
    int forward = color == CHESS_RED ? -1 : 1;
    if (to_y - from_y == forward && to_x == from_x) return 1;
    if (to_y == from_y && abs_i(to_x - from_x) == 1) {
        int crossed = color == CHESS_RED ? from_y <= 4 : from_y >= 5;
        return crossed;
    }
    return 0;
}

static int attacks_square(const ChessGame *game, int from_x, int from_y, int to_x, int to_y)
{
    int piece = game->board[from_y][from_x];
    ChessColor color = chess_piece_color(piece);
    ChessPieceType type = chess_piece_type(piece);
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    int adx = abs_i(dx);
    int ady = abs_i(dy);
    if (color == CHESS_NONE) return 0;

    switch (type) {
    case CHESS_KING:
        if (adx + ady == 1 && in_palace(color, to_x, to_y)) return 1;
        return from_x == to_x && count_between(game, from_x, from_y, to_x, to_y) == 0;
    case CHESS_ADVISOR:
        return adx == 1 && ady == 1 && in_palace(color, to_x, to_y);
    case CHESS_ELEPHANT:
        return adx == 2 && ady == 2 && elephant_side_ok(color, to_y) &&
               game->board[from_y + dy / 2][from_x + dx / 2] == CHESS_EMPTY;
    case CHESS_HORSE:
        if (!((adx == 1 && ady == 2) || (adx == 2 && ady == 1))) return 0;
        if (adx == 1) return game->board[from_y + dy / 2][from_x] == CHESS_EMPTY;
        return game->board[from_y][from_x + dx / 2] == CHESS_EMPTY;
    case CHESS_ROOK:
        return (dx == 0 || dy == 0) && count_between(game, from_x, from_y, to_x, to_y) == 0;
    case CHESS_CANNON:
        return (dx == 0 || dy == 0) && count_between(game, from_x, from_y, to_x, to_y) == 1;
    case CHESS_PAWN:
        return pawn_can_attack(color, from_x, from_y, to_x, to_y);
    default:
        return 0;
    }
}

static int find_king(const ChessGame *game, ChessColor color, int *out_x, int *out_y)
{
    int y;
    for (y = 0; y < CHESS_ROWS; y++) {
        int x;
        for (x = 0; x < CHESS_COLS; x++) {
            if (game->board[y][x] == (color == CHESS_RED ? CHESS_KING : -CHESS_KING)) {
                *out_x = x;
                *out_y = y;
                return 1;
            }
        }
    }
    return 0;
}

int chess_is_in_check(const ChessGame *game, ChessColor color)
{
    int king_x;
    int king_y;
    int y;
    if (!find_king(game, color, &king_x, &king_y)) return 1;
    for (y = 0; y < CHESS_ROWS; y++) {
        int x;
        for (x = 0; x < CHESS_COLS; x++) {
            if (chess_piece_color(game->board[y][x]) == (ChessColor)-color &&
                attacks_square(game, x, y, king_x, king_y)) {
                return 1;
            }
        }
    }
    return 0;
}

static int pseudo_legal(const ChessGame *game, int from_x, int from_y, int to_x, int to_y)
{
    int piece;
    ChessColor color;
    ChessPieceType type;
    int dx;
    int dy;
    int adx;
    int ady;
    if (!chess_in_bounds(from_x, from_y) || !chess_in_bounds(to_x, to_y)) return 0;
    piece = game->board[from_y][from_x];
    color = chess_piece_color(piece);
    type = chess_piece_type(piece);
    if (color == CHESS_NONE || (from_x == to_x && from_y == to_y) ||
        chess_piece_color(game->board[to_y][to_x]) == color) return 0;
    dx = to_x - from_x;
    dy = to_y - from_y;
    adx = abs_i(dx);
    ady = abs_i(dy);

    switch (type) {
    case CHESS_KING:
        return adx + ady == 1 && in_palace(color, to_x, to_y);
    case CHESS_ADVISOR:
        return adx == 1 && ady == 1 && in_palace(color, to_x, to_y);
    case CHESS_ELEPHANT:
        return adx == 2 && ady == 2 && elephant_side_ok(color, to_y) &&
               game->board[from_y + dy / 2][from_x + dx / 2] == CHESS_EMPTY;
    case CHESS_HORSE:
        if (!((adx == 1 && ady == 2) || (adx == 2 && ady == 1))) return 0;
        if (adx == 1) return game->board[from_y + dy / 2][from_x] == CHESS_EMPTY;
        return game->board[from_y][from_x + dx / 2] == CHESS_EMPTY;
    case CHESS_ROOK:
        return (dx == 0 || dy == 0) && count_between(game, from_x, from_y, to_x, to_y) == 0;
    case CHESS_CANNON:
        if (dx != 0 && dy != 0) return 0;
        return count_between(game, from_x, from_y, to_x, to_y) ==
               (game->board[to_y][to_x] == CHESS_EMPTY ? 0 : 1);
    case CHESS_PAWN:
        return pawn_can_attack(color, from_x, from_y, to_x, to_y);
    default:
        return 0;
    }
}

int chess_is_legal_move(const ChessGame *game, int from_x, int from_y, int to_x, int to_y)
{
    ChessGame copy;
    int piece;
    if (!game || game->result != CHESS_ONGOING || !pseudo_legal(game, from_x, from_y, to_x, to_y)) {
        return 0;
    }
    piece = game->board[from_y][from_x];
    if (chess_piece_color(piece) != game->turn) return 0;
    if (chess_piece_type(game->board[to_y][to_x]) == CHESS_KING) return 0;
    copy = *game;
    copy.board[to_y][to_x] = copy.board[from_y][from_x];
    copy.board[from_y][from_x] = CHESS_EMPTY;
    return !chess_is_in_check(&copy, game->turn);
}

int chess_has_legal_move(const ChessGame *game, ChessColor color)
{
    ChessGame copy;
    int y;
    if (!game || color == CHESS_NONE) return 0;
    copy = *game;
    copy.turn = color;
    for (y = 0; y < CHESS_ROWS; y++) {
        int x;
        for (x = 0; x < CHESS_COLS; x++) {
            int ty;
            if (chess_piece_color(copy.board[y][x]) != color) continue;
            for (ty = 0; ty < CHESS_ROWS; ty++) {
                int tx;
                for (tx = 0; tx < CHESS_COLS; tx++) {
                    if (chess_is_legal_move(&copy, x, y, tx, ty)) return 1;
                }
            }
        }
    }
    return 0;
}

int chess_apply_move(ChessGame *game, int from_x, int from_y, int to_x, int to_y)
{
    ChessColor moving;
    if (!game || !chess_is_legal_move(game, from_x, from_y, to_x, to_y)) return 0;
    moving = game->turn;
    game->board[to_y][to_x] = game->board[from_y][from_x];
    game->board[from_y][from_x] = CHESS_EMPTY;
    game->move_count++;
    game->turn = (ChessColor)-moving;
    if (!chess_has_legal_move(game, game->turn)) {
        /* A side with no legal move loses both when checkmated and when stalemated. */
        game->result = moving == CHESS_RED ? CHESS_RED_WIN : CHESS_BLACK_WIN;
    }
    return 1;
}

size_t chess_legal_destinations(const ChessGame *game, int from_x, int from_y,
                                int out_x[CHESS_MAX_LEGAL_MOVES],
                                int out_y[CHESS_MAX_LEGAL_MOVES])
{
    size_t count = 0;
    int y;
    if (!game || !out_x || !out_y) return 0;
    for (y = 0; y < CHESS_ROWS && count < CHESS_MAX_LEGAL_MOVES; y++) {
        int x;
        for (x = 0; x < CHESS_COLS && count < CHESS_MAX_LEGAL_MOVES; x++) {
            if (chess_is_legal_move(game, from_x, from_y, x, y)) {
                out_x[count] = x;
                out_y[count] = y;
                count++;
            }
        }
    }
    return count;
}

void chess_init(ChessGame *game)
{
    static const int back_rank[CHESS_COLS] = {
        CHESS_ROOK, CHESS_HORSE, CHESS_ELEPHANT, CHESS_ADVISOR, CHESS_KING,
        CHESS_ADVISOR, CHESS_ELEPHANT, CHESS_HORSE, CHESS_ROOK
    };
    int x;
    if (!game) return;
    memset(game, 0, sizeof(*game));
    for (x = 0; x < CHESS_COLS; x++) {
        game->board[0][x] = -back_rank[x];
        game->board[9][x] = back_rank[x];
    }
    game->board[2][1] = -CHESS_CANNON;
    game->board[2][7] = -CHESS_CANNON;
    game->board[7][1] = CHESS_CANNON;
    game->board[7][7] = CHESS_CANNON;
    for (x = 0; x < CHESS_COLS; x += 2) {
        game->board[3][x] = -CHESS_PAWN;
        game->board[6][x] = CHESS_PAWN;
    }
    game->turn = CHESS_RED;
    game->result = CHESS_ONGOING;
}

const char *chess_result_text(ChessResult result)
{
    switch (result) {
    case CHESS_RED_WIN: return "红方胜";
    case CHESS_BLACK_WIN: return "黑方胜";
    case CHESS_DRAW: return "和棋";
    default: return "对局进行中";
    }
}
