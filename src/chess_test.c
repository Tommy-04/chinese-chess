#include "chess.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ChessGame game;
    chess_init(&game);
    assert(game.turn == CHESS_RED);
    assert(game.board[9][4] == CHESS_KING);
    assert(game.board[0][4] == -CHESS_KING);

    /* Horse leg: place a blocker at b8, then remove it and retry. */
    game.board[8][1] = CHESS_PAWN;
    assert(!chess_is_legal_move(&game, 1, 9, 2, 7));
    game.board[8][1] = CHESS_EMPTY;
    assert(chess_is_legal_move(&game, 1, 9, 2, 7));

    /* Red cannon captures across exactly one screen piece. */
    game.board[7][1] = CHESS_CANNON;
    game.board[5][1] = CHESS_PAWN;
    game.board[2][1] = -CHESS_CANNON;
    assert(chess_is_legal_move(&game, 1, 7, 1, 2));

    /* A rook cannot expose its own king to the opposing rook. */
    chess_init(&game);
    game.board[8][4] = CHESS_EMPTY;
    game.board[1][4] = CHESS_EMPTY;
    game.board[7][4] = -CHESS_ROOK;
    assert(!chess_is_legal_move(&game, 4, 9, 4, 8));

    /* A stalemated side loses in Chinese chess, even without a check. */
    memset(&game, 0, sizeof(game));
    game.board[0][4] = -CHESS_KING;
    game.board[9][4] = CHESS_KING;
    game.board[2][3] = CHESS_ROOK;
    game.board[2][5] = CHESS_ROOK;
    game.board[1][0] = CHESS_ROOK;
    game.board[5][4] = CHESS_PAWN;
    game.board[6][0] = CHESS_PAWN;
    game.turn = CHESS_RED;
    game.result = CHESS_ONGOING;
    assert(!chess_is_in_check(&game, CHESS_BLACK));
    assert(chess_apply_move(&game, 0, 6, 0, 5));
    assert(game.result == CHESS_RED_WIN);

    puts("chess core tests passed");
    return 0;
}
