#include "ui.h"

#include "chess.h"
#include "net.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 800
#define SCREEN_H 480
#define BOARD_X 80
#define BOARD_Y 12
#define BOARD_W 640
#define BOARD_H 456
#define BOARD_LEFT 36
#define BOARD_TOP 24
#define BOARD_GAP_X 71
#define BOARD_GAP_Y 40
#define BOARD_RIVER_GAP 32
#define CELL_HIT_W 68
#define CELL_HIT_H 36
#define PIECE_SIZE 44
#define BOARD_POINT_X(x) (BOARD_LEFT + (x) * BOARD_GAP_X)
#define BOARD_POINT_Y(y) \
    (BOARD_TOP + ((y) <= 4 ? (y) * BOARD_GAP_Y : \
                  5 * BOARD_GAP_Y + BOARD_RIVER_GAP + ((y) - 5) * BOARD_GAP_Y))
#define SIDE_PANEL_X 445
#define SIDE_PANEL_Y 18
#define SIDE_PANEL_W 345
#define SIDE_PANEL_H 444
#define SIDE_TOGGLE_X 750
#define SIDE_TOGGLE_Y 18
#define SIDE_TOGGLE_W 42
#define SIDE_TOGGLE_H 56
#define DEFAULT_DEBUG_IP "8.163.43.95"

typedef enum {
    UI_MENU = 0,
    UI_ONLINE_MENU,
    UI_HOST_WAIT,
    UI_JOIN_FORM,
    UI_BOARD
} UiPage;

typedef enum {
    UI_LOCAL = 0,
    UI_ONLINE
} UiMode;

typedef struct {
    int x;
    int y;
} UiCellData;

typedef struct {
    ChessGame game;
    NetContext net;
    UiPage page;
    UiMode mode;
    ChessColor local_color;
    int selected_x;
    int selected_y;
    int legal[CHESS_ROWS][CHESS_COLS];
    UiCellData cell_data[CHESS_ROWS][CHESS_COLS];
    lv_obj_t *cell_objects[CHESS_ROWS][CHESS_COLS];
    lv_obj_t *piece_objects[CHESS_ROWS][CHESS_COLS];
    lv_obj_t *piece_labels[CHESS_ROWS][CHESS_COLS];
    lv_obj_t *root;
    lv_obj_t *board_panel;
    lv_obj_t *side_panel;
    lv_obj_t *side_toggle;
    lv_obj_t *status_label;
    lv_obj_t *turn_label;
    lv_obj_t *role_label;
    lv_obj_t *ip_textarea;
    lv_obj_t *port_textarea;
    lv_obj_t *room_textarea;
    lv_obj_t *form_status_label;
    lv_obj_t *keyboard;
    lv_obj_t *keyboard_target;
    lv_obj_t *keyboard_letter_labels[26];
    lv_timer_t *timer;
    int keyboard_letter_count;
    int keyboard_mode;
    int keyboard_uppercase;
    int result_shown;
    int side_open;
} UiState;

static UiState ui;
static lv_style_t style_screen;
static lv_style_t style_panel;
static lv_style_t style_button;
static lv_style_t style_button_primary;
static lv_style_t style_board_cell;
static lv_style_t style_board_selected;
static lv_style_t style_board_hint;
static lv_style_t style_board_line;
static lv_style_t style_board_mark;
static lv_style_t style_river;
static lv_style_t style_piece_red;
static lv_style_t style_piece_black;
static lv_style_t style_title;
static lv_style_t style_muted;
static lv_style_t style_status;
static lv_point_t board_horizontal_points[CHESS_ROWS][2];
static lv_point_t board_vertical_top_points[CHESS_COLS][2];
static lv_point_t board_vertical_bottom_points[CHESS_COLS][2];
static lv_point_t board_palace_points[4][2];

static void ui_show_menu(void);
static void ui_show_online_menu(void);
static void ui_show_join_form(void);
static void ui_network_timer(lv_timer_t *timer);
static void ui_result_event(lv_event_t *event);
static void ui_resign_event(lv_event_t *event);
static void ui_draw_event(lv_event_t *event);
static void ui_refresh_board(void);
static void ui_clear_selection(void);
static void ui_textarea_trigger_event(lv_event_t *event);
static void ui_add_textarea_trigger(lv_obj_t *parent, lv_obj_t *textarea);
static void ui_show_keyboard(lv_obj_t *textarea);
static void ui_keyboard_button_event(lv_event_t *event);
static void ui_toggle_side_event(lv_event_t *event);

static const lv_color_t COLOR_BG = LV_COLOR_MAKE(19, 27, 37);
static const lv_color_t COLOR_BG_BOTTOM = LV_COLOR_MAKE(68, 45, 31);
static const lv_color_t COLOR_PANEL = LV_COLOR_MAKE(30, 41, 54);
static const lv_color_t COLOR_PANEL_2 = LV_COLOR_MAKE(37, 50, 65);
static const lv_color_t COLOR_ACCENT = LV_COLOR_MAKE(224, 153, 61);
static const lv_color_t COLOR_BOARD_TOP = LV_COLOR_MAKE(239, 205, 143);
static const lv_color_t COLOR_BOARD_BOTTOM = LV_COLOR_MAKE(194, 142, 77);
static const lv_color_t COLOR_BOARD_FRAME = LV_COLOR_MAKE(91, 54, 29);
static const lv_color_t COLOR_BOARD_GRAIN = LV_COLOR_MAKE(169, 111, 56);
static const lv_color_t COLOR_RIVER = LV_COLOR_MAKE(211, 162, 94);
static const lv_color_t COLOR_GRID = LV_COLOR_MAKE(103, 72, 39);
static const lv_color_t COLOR_RED = LV_COLOR_MAKE(188, 53, 49);
static const lv_color_t COLOR_BLACK = LV_COLOR_MAKE(28, 35, 44);
static const lv_color_t COLOR_HINT = LV_COLOR_MAKE(109, 173, 104);
static const lv_color_t COLOR_PIECE_FACE = LV_COLOR_MAKE(250, 229, 178);
static const lv_color_t COLOR_PIECE_RED_EDGE = LV_COLOR_MAKE(151, 44, 36);
static const lv_color_t COLOR_PIECE_BLACK_EDGE = LV_COLOR_MAKE(61, 48, 34);
static const lv_color_t COLOR_MENU_WARM = LV_COLOR_MAKE(154, 83, 45);
static const lv_color_t COLOR_MENU_WARM_DARK = LV_COLOR_MAKE(112, 55, 34);
static const lv_color_t COLOR_MENU_TEAL = LV_COLOR_MAKE(39, 72, 82);
static const lv_color_t COLOR_MENU_TEAL_DARK = LV_COLOR_MAKE(27, 48, 59);
static const lv_color_t COLOR_MENU_LINE = LV_COLOR_MAKE(224, 153, 61);

#if LV_FONT_SIMSUN_16_CJK
LV_FONT_DECLARE(lv_font_simsun_16_cjk);
#define UI_FONT (&lv_font_simsun_16_cjk)
#else
#define UI_FONT LV_FONT_DEFAULT
#endif

static void ui_init_styles(void)
{
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, COLOR_BG);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&style_screen, COLOR_BG_BOTTOM);
    lv_style_set_bg_grad_dir(&style_screen, LV_GRAD_DIR_VER);

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, COLOR_PANEL);
    lv_style_set_bg_opa(&style_panel, LV_OPA_COVER);
    lv_style_set_radius(&style_panel, 8);
    lv_style_set_pad_all(&style_panel, 18);
    lv_style_set_border_width(&style_panel, 1);
    lv_style_set_border_color(&style_panel, COLOR_PANEL_2);

    lv_style_init(&style_button);
    lv_style_set_bg_color(&style_button, COLOR_PANEL_2);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_text_color(&style_button, lv_color_white());
    lv_style_set_text_font(&style_button, UI_FONT);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_border_width(&style_button, 1);
    lv_style_set_border_color(&style_button, lv_color_hex(0x435363));
    lv_style_set_pad_all(&style_button, 10);

    lv_style_init(&style_button_primary);
    lv_style_set_bg_color(&style_button_primary, COLOR_ACCENT);
    lv_style_set_bg_opa(&style_button_primary, LV_OPA_COVER);
    lv_style_set_text_color(&style_button_primary, COLOR_BG);
    lv_style_set_text_font(&style_button_primary, UI_FONT);
    lv_style_set_radius(&style_button_primary, 6);
    lv_style_set_pad_all(&style_button_primary, 10);

    lv_style_init(&style_board_cell);
    lv_style_set_bg_opa(&style_board_cell, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_board_cell, 0);
    lv_style_set_radius(&style_board_cell, 0);
    lv_style_set_pad_all(&style_board_cell, 0);
    lv_style_set_text_font(&style_board_cell, UI_FONT);
    lv_style_set_text_align(&style_board_cell, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_board_line);
    lv_style_set_line_color(&style_board_line, COLOR_GRID);
    lv_style_set_line_width(&style_board_line, 2);
    lv_style_set_line_rounded(&style_board_line, true);

    lv_style_init(&style_board_mark);
    lv_style_set_bg_color(&style_board_mark, COLOR_GRID);
    lv_style_set_bg_opa(&style_board_mark, LV_OPA_COVER);
    lv_style_set_radius(&style_board_mark, LV_RADIUS_CIRCLE);

    lv_style_init(&style_river);
    lv_style_set_text_color(&style_river, COLOR_GRID);
    lv_style_set_text_font(&style_river, UI_FONT);
    lv_style_set_text_align(&style_river, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_piece_red);
    lv_style_set_bg_color(&style_piece_red, COLOR_PIECE_FACE);
    lv_style_set_bg_opa(&style_piece_red, LV_OPA_COVER);
    lv_style_set_radius(&style_piece_red, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_piece_red, 3);
    lv_style_set_border_color(&style_piece_red, COLOR_PIECE_RED_EDGE);
    lv_style_set_shadow_width(&style_piece_red, 5);
    lv_style_set_shadow_ofs_y(&style_piece_red, 2);
    lv_style_set_shadow_color(&style_piece_red, COLOR_BOARD_FRAME);
    lv_style_set_shadow_opa(&style_piece_red, LV_OPA_40);
    lv_style_set_pad_all(&style_piece_red, 0);

    lv_style_init(&style_piece_black);
    lv_style_set_bg_color(&style_piece_black, COLOR_PIECE_FACE);
    lv_style_set_bg_opa(&style_piece_black, LV_OPA_COVER);
    lv_style_set_radius(&style_piece_black, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_piece_black, 3);
    lv_style_set_border_color(&style_piece_black, COLOR_PIECE_BLACK_EDGE);
    lv_style_set_shadow_width(&style_piece_black, 5);
    lv_style_set_shadow_ofs_y(&style_piece_black, 2);
    lv_style_set_shadow_color(&style_piece_black, COLOR_BOARD_FRAME);
    lv_style_set_shadow_opa(&style_piece_black, LV_OPA_40);
    lv_style_set_pad_all(&style_piece_black, 0);

    lv_style_init(&style_board_selected);
    lv_style_set_bg_color(&style_board_selected, lv_color_hex(0xF1D379));
    lv_style_set_bg_opa(&style_board_selected, LV_OPA_COVER);
    lv_style_set_border_width(&style_board_selected, 3);
    lv_style_set_border_color(&style_board_selected, COLOR_ACCENT);

    lv_style_init(&style_board_hint);
    lv_style_set_bg_color(&style_board_hint, COLOR_HINT);
    lv_style_set_bg_opa(&style_board_hint, LV_OPA_70);
    lv_style_set_border_width(&style_board_hint, 2);
    lv_style_set_border_color(&style_board_hint, COLOR_HINT);

    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_white());
    lv_style_set_text_font(&style_title, UI_FONT);
    lv_style_set_text_letter_space(&style_title, 1);

    lv_style_init(&style_muted);
    lv_style_set_text_color(&style_muted, lv_color_hex(0xA4B1BE));
    lv_style_set_text_font(&style_muted, UI_FONT);

    lv_style_init(&style_status);
    lv_style_set_text_color(&style_status, COLOR_ACCENT);
    lv_style_set_text_font(&style_status, UI_FONT);
}

static void ui_clear_root(int close_network)
{
    if (ui.timer) {
        lv_timer_del(ui.timer);
        ui.timer = NULL;
    }
    if (ui.keyboard) {
        lv_obj_del(ui.keyboard);
        ui.keyboard = NULL;
    }
    ui.keyboard_target = NULL;
    ui.keyboard_letter_count = 0;
    ui.keyboard_mode = -1;
    ui.keyboard_uppercase = 0;
    if (close_network) net_close(&ui.net);
    if (ui.root) lv_obj_del(ui.root);
    ui.board_panel = NULL;
    ui.side_panel = NULL;
    ui.side_toggle = NULL;
    ui.status_label = NULL;
    ui.turn_label = NULL;
    ui.role_label = NULL;
    ui.ip_textarea = NULL;
    ui.port_textarea = NULL;
    ui.room_textarea = NULL;
    ui.form_status_label = NULL;
    ui.root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(ui.root);
    lv_obj_add_style(ui.root, &style_screen, LV_PART_MAIN);
    lv_obj_set_size(ui.root, SCREEN_W, SCREEN_H);
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);
    ui.side_open = 0;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                            lv_coord_t width, lv_coord_t height, lv_style_t *style)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    if (style) lv_obj_add_style(label, style, LV_PART_MAIN);
    return label;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                             lv_coord_t width, lv_coord_t height, lv_event_cb_t callback, int primary)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_add_style(button, primary ? &style_button_primary : &style_button, LV_PART_MAIN);
    if (callback) lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *make_menu_piece_badge(lv_obj_t *parent, const char *piece, int red,
                                       lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_t *label;
    lv_obj_remove_style_all(badge);
    lv_obj_set_pos(badge, x, y);
    lv_obj_set_size(badge, 68, 68);
    lv_obj_set_style_bg_color(badge, COLOR_PIECE_FACE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(badge, red ? COLOR_PIECE_RED_EDGE : COLOR_PIECE_BLACK_EDGE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(badge, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(badge, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(badge, COLOR_BOARD_FRAME, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(badge, LV_OPA_50, LV_PART_MAIN);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    label = lv_label_create(badge);
    lv_label_set_text(label, piece);
    lv_obj_set_style_text_font(label, UI_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, red ? COLOR_RED : COLOR_BLACK, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return badge;
}

static lv_obj_t *make_menu_card(lv_obj_t *parent, const char *title, const char *subtitle,
                                const char *detail, const char *piece, int red,
                                lv_coord_t x, lv_event_cb_t callback)
{
    lv_obj_t *card = lv_btn_create(parent);
    lv_obj_t *title_label;
    lv_obj_t *subtitle_label;
    lv_obj_t *detail_label;
    lv_obj_t *arrow_label;
    lv_color_t background = red ? COLOR_MENU_WARM : COLOR_MENU_TEAL;
    lv_color_t pressed = red ? COLOR_MENU_WARM_DARK : COLOR_MENU_TEAL_DARK;
    lv_color_t text_color = red ? COLOR_PIECE_FACE : lv_color_white();
    lv_color_t muted_color = red ? lv_color_hex(0xF1D9B0) : lv_color_hex(0xB7D0D6);

    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, x, 148);
    lv_obj_set_size(card, 330, 196);
    lv_obj_set_style_bg_color(card, background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, red ? COLOR_MENU_LINE : lv_color_hex(0x7DB3B8), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_MENU_LINE, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(card, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(card, COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card, LV_OPA_40, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    if (callback) lv_obj_add_event_cb(card, callback, LV_EVENT_CLICKED, NULL);

    make_menu_piece_badge(card, piece, red, 24, 26);
    title_label = make_label(card, title, 108, 28, 190, 32, NULL);
    lv_obj_set_style_text_font(title_label, UI_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, text_color, LV_PART_MAIN);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    subtitle_label = make_label(card, subtitle, 24, 108, 280, 28, NULL);
    lv_obj_set_style_text_font(subtitle_label, UI_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(subtitle_label, muted_color, LV_PART_MAIN);
    detail_label = make_label(card, detail, 24, 143, 250, 28, NULL);
    lv_obj_set_style_text_font(detail_label, UI_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(detail_label, muted_color, LV_PART_MAIN);
    arrow_label = make_label(card, ">", 282, 143, 28, 28, NULL);
    lv_obj_set_style_text_font(arrow_label, UI_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(arrow_label, text_color, LV_PART_MAIN);
    lv_obj_set_style_text_align(arrow_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_clear_flag(title_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(subtitle_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(detail_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(arrow_label, LV_OBJ_FLAG_CLICKABLE);
    return card;
}

static const char *piece_name(int piece)
{
    static const char *red_names[] = {"", "帅", "仕", "相", "马", "车", "炮", "兵"};
    static const char *black_names[] = {"", "将", "士", "象", "馬", "车", "炮", "卒"};
    ChessPieceType type = chess_piece_type(piece);
    if (type == CHESS_EMPTY) return "";
    return piece > 0 ? red_names[type] : black_names[type];
}

static void ui_set_status(const char *text)
{
    if (ui.status_label) lv_label_set_text(ui.status_label, text);
    else if (ui.form_status_label) lv_label_set_text(ui.form_status_label, text);
}

static void ui_update_turn_label(void)
{
    char text[96];
    if (!ui.turn_label) return;
    if (ui.game.result != CHESS_ONGOING) {
        lv_label_set_text(ui.turn_label, chess_result_text(ui.game.result));
        return;
    }
    snprintf(text, sizeof(text), "当前回合  %s", ui.game.turn == CHESS_RED ? "红方" : "黑方");
    lv_label_set_text(ui.turn_label, text);
}

static const char *ui_result_title(void)
{
    if (ui.game.result == CHESS_DRAW) return "和棋";
    if (ui.mode == UI_ONLINE && ui.local_color != CHESS_NONE) {
        int won = (ui.game.result == CHESS_RED_WIN && ui.local_color == CHESS_RED) ||
                  (ui.game.result == CHESS_BLACK_WIN && ui.local_color == CHESS_BLACK);
        return won ? "胜利" : "失败";
    }
    return ui.game.result == CHESS_RED_WIN ? "红方胜利" : "黑方胜利";
}

static void ui_show_result(void)
{
    static const char *buttons[] = {"返回主菜单", ""};
    lv_obj_t *message;
    lv_obj_t *title;
    lv_obj_t *text;
    lv_obj_t *close_btn;
    lv_obj_t *close_label;
    lv_obj_t *button_matrix;
    if (ui.result_shown) return;
    message = lv_msgbox_create(NULL, ui_result_title(), chess_result_text(ui.game.result), buttons, true);
    if (!message) return;
    ui.result_shown = 1;
    lv_obj_set_width(message, 360);
    lv_obj_center(message);
    lv_obj_set_style_bg_color(message, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(message, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(message, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(message, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_radius(message, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(message, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(message, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(message, COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(message, LV_OPA_50, LV_PART_MAIN);

    title = lv_msgbox_get_title(message);
    text = lv_msgbox_get_text(message);
    close_btn = lv_msgbox_get_close_btn(message);
    button_matrix = lv_msgbox_get_btns(message);
    if (title) {
        lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, UI_FONT, LV_PART_MAIN);
    }
    if (text) {
        lv_obj_set_style_text_color(text, COLOR_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_text_font(text, UI_FONT, LV_PART_MAIN);
    }
    if (button_matrix) {
        lv_obj_set_style_bg_color(button_matrix, COLOR_PANEL_2, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(button_matrix, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(button_matrix, 6, LV_PART_MAIN);
        lv_obj_set_style_text_color(button_matrix, lv_color_white(), LV_PART_ITEMS);
        lv_obj_set_style_text_font(button_matrix, UI_FONT, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(button_matrix, COLOR_PIECE_RED_EDGE, LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(button_matrix, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_radius(button_matrix, 6, LV_PART_ITEMS);
    }
    if (close_btn) {
        lv_obj_set_style_bg_color(close_btn, COLOR_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(close_btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_text_color(close_btn, COLOR_BG, LV_PART_MAIN);
        lv_obj_set_style_text_font(close_btn, UI_FONT, LV_PART_MAIN);
        close_label = lv_obj_get_child(close_btn, 0);
        if (close_label) {
            lv_label_set_text(close_label, "×");
            lv_obj_set_style_text_color(close_label, COLOR_BG, LV_PART_MAIN);
            lv_obj_set_style_text_font(close_label, UI_FONT, LV_PART_MAIN);
        }
    }
    lv_obj_add_event_cb(message, ui_result_event, LV_EVENT_VALUE_CHANGED, NULL);
}

static void ui_back_to_menu_event(lv_event_t *event)
{
    (void)event;
    ui_show_menu();
}

static int ui_can_send_game_message(void)
{
    if (ui.mode != UI_ONLINE || !ui.net.connected || ui.local_color == CHESS_NONE ||
        ui.game.result != CHESS_ONGOING) {
        ui_set_status("当前还不能发送对局操作");
        return 0;
    }
    return 1;
}

static void ui_resign_event(lv_event_t *event)
{
    (void)event;
    if (!ui_can_send_game_message()) return;
    if (net_send_simple(&ui.net, NET_MSG_RESIGN, ui.local_color) != 0) {
        net_close(&ui.net);
        ui.local_color = CHESS_NONE;
        ui_set_status("认输消息发送失败");
        return;
    }
    ui.game.result = ui.local_color == CHESS_RED ? CHESS_BLACK_WIN : CHESS_RED_WIN;
    ui_clear_selection();
    ui_set_status("本机已认输");
    ui_refresh_board();
    ui_show_result();
}

static void ui_draw_event(lv_event_t *event)
{
    (void)event;
    if (!ui_can_send_game_message()) return;
    if (net_send_simple(&ui.net, NET_MSG_DRAW, ui.local_color) != 0) {
        net_close(&ui.net);
        ui.local_color = CHESS_NONE;
        ui_set_status("和棋消息发送失败");
        return;
    }
    ui.game.result = CHESS_DRAW;
    ui_clear_selection();
    ui_set_status("本机已发起和棋");
    ui_refresh_board();
    ui_show_result();
}

static void ui_result_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *message = lv_event_get_target(event);
        lv_msgbox_close(message);
        ui_show_menu();
    }
}

static void ui_textarea_trigger_event(lv_event_t *event)
{
    lv_obj_t *textarea;
    if (lv_event_get_code(event) != LV_EVENT_PRESSED) return;
    textarea = (lv_obj_t *)lv_event_get_user_data(event);
    if (!textarea) return;
    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_textarea_set_cursor_pos(textarea, LV_TEXTAREA_CURSOR_LAST);
    ui_show_keyboard(textarea);
}

static void ui_add_textarea_trigger(lv_obj_t *parent, lv_obj_t *textarea)
{
    lv_obj_t *trigger;
    if (!parent || !textarea) return;
    trigger = lv_btn_create(parent);
    lv_obj_remove_style_all(trigger);
    lv_obj_set_pos(trigger, lv_obj_get_x(textarea), lv_obj_get_y(textarea));
    lv_obj_set_size(trigger, lv_obj_get_width(textarea), lv_obj_get_height(textarea));
    lv_obj_set_style_bg_opa(trigger, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(trigger, 0, LV_PART_MAIN);
    lv_obj_clear_flag(trigger, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_flag(trigger, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(trigger, ui_textarea_trigger_event, LV_EVENT_PRESSED, textarea);
}

enum {
    UI_KEYBOARD_IP = 0,
    UI_KEYBOARD_NUMBER,
    UI_KEYBOARD_TEXT
};

static const char KEY_DELETE[] = "@delete@";
static const char KEY_CLEAR[] = "@clear@";
static const char KEY_CAPS[] = "@caps@";
static const char KEY_SPACE[] = "@space@";
static const char KEY_OK[] = "@ok@";
static const char KEY_CLOSE[] = "@close@";

static lv_obj_t *ui_make_keyboard_button(const char *label_text, const char *key_value,
                                         lv_coord_t x, lv_coord_t y,
                                         lv_coord_t width, lv_coord_t height)
{
    lv_obj_t *button = lv_btn_create(ui.keyboard);
    lv_obj_t *label;
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_add_style(button, &style_button, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(button, ui_keyboard_button_event, LV_EVENT_CLICKED, (void *)key_value);
    label = lv_label_create(button);
    lv_label_set_text(label, label_text);
    lv_obj_center(label);
    return button;
}

static void ui_add_keyboard_letter(const char *letter, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t width, lv_coord_t height)
{
    lv_obj_t *button = ui_make_keyboard_button(letter, letter, x, y, width, height);
    if (ui.keyboard_letter_count < 26) {
        ui.keyboard_letter_labels[ui.keyboard_letter_count++] = lv_obj_get_child(button, 0);
    }
}

static void ui_update_keyboard_case(void)
{
    static const char letters[] = "qwertyuiopasdfghjklzxcvbnm";
    int i;
    for (i = 0; i < ui.keyboard_letter_count; i++) {
        char text[2];
        char c = letters[i];
        if (ui.keyboard_uppercase) c = (char)(c - 'a' + 'A');
        text[0] = c;
        text[1] = '\0';
        lv_label_set_text(ui.keyboard_letter_labels[i], text);
    }
}

static void ui_build_numeric_keyboard(int show_dot)
{
    static const char *digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
    int i;
    const lv_coord_t start_x = 108;
    const lv_coord_t start_y = 9;
    const lv_coord_t width = 140;
    const lv_coord_t height = 46;
    const lv_coord_t gap_x = 8;
    const lv_coord_t gap_y = 7;

    for (i = 0; i < 9; i++) {
        int row = i / 3;
        int col = i % 3;
        ui_make_keyboard_button(digits[i], digits[i],
                                start_x + col * (width + gap_x),
                                start_y + row * (height + gap_y), width, height);
    }
    ui_make_keyboard_button("Del", KEY_DELETE, start_x + 3 * (width + gap_x), start_y,
                            width, height);
    ui_make_keyboard_button("Clear", KEY_CLEAR, start_x + 3 * (width + gap_x),
                            start_y + height + gap_y, width, height);
    if (show_dot) {
        ui_make_keyboard_button(".", ".", start_x + 3 * (width + gap_x),
                                start_y + 2 * (height + gap_y), width, height);
        ui_make_keyboard_button("0", "0", start_x, start_y + 3 * (height + gap_y),
                                2 * width + gap_x, height);
        ui_make_keyboard_button("OK", KEY_OK, start_x + 2 * (width + gap_x),
                                start_y + 3 * (height + gap_y), width, height);
    } else {
        ui_make_keyboard_button("OK", KEY_OK, start_x + 3 * (width + gap_x),
                                start_y + 2 * (height + gap_y), width, height);
        ui_make_keyboard_button("0", "0", start_x, start_y + 3 * (height + gap_y),
                                3 * width + 2 * gap_x, height);
    }
    ui_make_keyboard_button("Close", KEY_CLOSE, start_x + 3 * (width + gap_x),
                            start_y + 3 * (height + gap_y), width, height);
}

static void ui_build_text_keyboard(void)
{
    static const char *digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    static const char *row_one[] = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
    static const char *row_two[] = {"a", "s", "d", "f", "g", "h", "j", "k", "l"};
    static const char *row_three[] = {"z", "x", "c", "v", "b", "n", "m"};
    int i;
    const lv_coord_t key_w = 72;
    const lv_coord_t key_h = 38;
    const lv_coord_t gap = 6;

    for (i = 0; i < 10; i++) {
        ui_make_keyboard_button(digits[i], digits[i], 13 + i * (key_w + gap), 7, key_w, key_h);
        ui_add_keyboard_letter(row_one[i], 13 + i * (key_w + gap), 51, key_w, key_h);
    }
    for (i = 0; i < 9; i++) {
        ui_add_keyboard_letter(row_two[i], 52 + i * (key_w + gap), 95, key_w, key_h);
    }
    ui_make_keyboard_button("Caps", KEY_CAPS, 13, 139, key_w, key_h);
    for (i = 0; i < 7; i++) {
        ui_add_keyboard_letter(row_three[i], 91 + i * (key_w + gap), 139, key_w, key_h);
    }
    ui_make_keyboard_button("-", "-", 637, 139, key_w, key_h);
    ui_make_keyboard_button("_", "_", 715, 139, key_w, key_h);
    ui_make_keyboard_button("Space", KEY_SPACE, 33, 183, 270, key_h);
    ui_make_keyboard_button("Del", KEY_DELETE, 309, 183, 105, key_h);
    ui_make_keyboard_button("Clear", KEY_CLEAR, 420, 183, 105, key_h);
    ui_make_keyboard_button("OK", KEY_OK, 531, 183, 105, key_h);
    ui_make_keyboard_button("Close", KEY_CLOSE, 642, 183, 125, key_h);
}

static void ui_build_keyboard(int mode)
{
    lv_obj_clean(ui.keyboard);
    memset(ui.keyboard_letter_labels, 0, sizeof(ui.keyboard_letter_labels));
    ui.keyboard_letter_count = 0;
    ui.keyboard_uppercase = 0;
    ui.keyboard_mode = mode;
    if (mode == UI_KEYBOARD_TEXT) ui_build_text_keyboard();
    else ui_build_numeric_keyboard(mode == UI_KEYBOARD_IP);
}

static void ui_hide_keyboard(void)
{
    if (ui.keyboard) lv_obj_add_flag(ui.keyboard, LV_OBJ_FLAG_HIDDEN);
    if (ui.keyboard_target) lv_obj_clear_state(ui.keyboard_target, LV_STATE_FOCUSED);
    ui.keyboard_target = NULL;
}

static void ui_keyboard_button_event(lv_event_t *event)
{
    const char *key = (const char *)lv_event_get_user_data(event);
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !ui.keyboard_target || !key) return;
    if (key == KEY_DELETE) {
        lv_textarea_del_char(ui.keyboard_target);
    } else if (key == KEY_CLEAR) {
        lv_textarea_set_text(ui.keyboard_target, "");
    } else if (key == KEY_CAPS) {
        ui.keyboard_uppercase = !ui.keyboard_uppercase;
        ui_update_keyboard_case();
    } else if (key == KEY_SPACE) {
        lv_textarea_add_text(ui.keyboard_target, " ");
    } else if (key == KEY_OK || key == KEY_CLOSE) {
        ui_hide_keyboard();
    } else {
        char text[2] = {key[0], '\0'};
        if (ui.keyboard_uppercase && text[0] >= 'a' && text[0] <= 'z') {
            text[0] = (char)(text[0] - 'a' + 'A');
        }
        lv_textarea_add_text(ui.keyboard_target, text);
    }
}

static void ui_show_keyboard(lv_obj_t *textarea)
{
    int mode;
    if (!textarea) return;
    if (!ui.keyboard) {
        ui_set_status("键盘创建失败");
        return;
    }
    mode = textarea == ui.room_textarea ? UI_KEYBOARD_TEXT :
           (textarea == ui.ip_textarea ? UI_KEYBOARD_IP : UI_KEYBOARD_NUMBER);
    if (ui.keyboard_mode != mode) ui_build_keyboard(mode);
    if (ui.keyboard_target && ui.keyboard_target != textarea) {
        lv_obj_clear_state(ui.keyboard_target, LV_STATE_FOCUSED);
    }
    ui.keyboard_target = textarea;
    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_textarea_set_cursor_pos(textarea, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_clear_flag(ui.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui.keyboard);
    lv_obj_invalidate(ui.keyboard);
    ui_set_status("屏幕键盘使用中");
}

static void ui_toggle_side_event(lv_event_t *event)
{
    lv_obj_t *label;
    (void)event;
    if (!ui.side_panel || !ui.side_toggle) return;
    ui.side_open = !ui.side_open;
    if (ui.side_open) lv_obj_clear_flag(ui.side_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui.side_panel, LV_OBJ_FLAG_HIDDEN);
    label = lv_obj_get_child(ui.side_toggle, 0);
    if (label) lv_label_set_text(label, ui.side_open ? "<" : ">");
}

static void ui_refresh_board(void)
{
    int y;
    if (!ui.board_panel) return;
    for (y = 0; y < CHESS_ROWS; y++) {
        int x;
        for (x = 0; x < CHESS_COLS; x++) {
            lv_obj_t *cell = ui.cell_objects[y][x];
            lv_obj_t *piece_obj = ui.piece_objects[y][x];
            lv_obj_t *piece_label = ui.piece_labels[y][x];
            int piece = ui.game.board[y][x];
            if (!cell || !piece_obj || !piece_label) continue;
            lv_label_set_text(piece_label, piece_name(piece));
            lv_obj_remove_style(cell, &style_board_selected, LV_PART_MAIN);
            lv_obj_remove_style(cell, &style_board_hint, LV_PART_MAIN);
            if (ui.legal[y][x]) lv_obj_add_style(cell, &style_board_hint, LV_PART_MAIN);
            if (x == ui.selected_x && y == ui.selected_y) lv_obj_add_style(cell, &style_board_selected, LV_PART_MAIN);
            if (piece == CHESS_EMPTY) {
                lv_obj_add_flag(piece_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(piece_obj, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_style(piece_obj, &style_piece_red, LV_PART_MAIN);
                lv_obj_remove_style(piece_obj, &style_piece_black, LV_PART_MAIN);
                lv_obj_add_style(piece_obj, piece > 0 ? &style_piece_red : &style_piece_black, LV_PART_MAIN);
                lv_obj_set_style_border_color(piece_obj,
                    (x == ui.selected_x && y == ui.selected_y) ? COLOR_ACCENT :
                    (piece > 0 ? COLOR_PIECE_RED_EDGE : COLOR_PIECE_BLACK_EDGE), LV_PART_MAIN);
                lv_obj_set_style_text_color(piece_label, piece > 0 ? COLOR_RED : COLOR_BLACK, LV_PART_MAIN);
            }
        }
    }
    ui_update_turn_label();
}

static void ui_clear_selection(void)
{
    ui.selected_x = -1;
    ui.selected_y = -1;
    memset(ui.legal, 0, sizeof(ui.legal));
}

static void ui_try_move(int from_x, int from_y, int to_x, int to_y)
{
    ChessGame previous = ui.game;
    ChessColor moving = ui.game.turn;
    if (!chess_apply_move(&ui.game, from_x, from_y, to_x, to_y)) {
        ui_set_status("这一步不符合规则");
        return;
    }
    ui_clear_selection();
    if (ui.mode == UI_ONLINE) {
        if (net_send_move(&ui.net, from_x, from_y, to_x, to_y) != 0) {
            ui.game = previous;
            net_close(&ui.net);
            ui.local_color = CHESS_NONE;
            ui_set_status("网络发送失败，对局已回滚");
            ui_refresh_board();
            return;
        }
        if (ui.game.result != CHESS_ONGOING) net_send_result(&ui.net, moving);
    }
    ui_set_status(ui.game.result == CHESS_ONGOING ? "落子成功" : chess_result_text(ui.game.result));
    ui_refresh_board();
    if (ui.game.result != CHESS_ONGOING) ui_show_result();
}

static void ui_cell_event(lv_event_t *event)
{
    UiCellData *data = (UiCellData *)lv_event_get_user_data(event);
    int piece;
    if (!data || ui.game.result != CHESS_ONGOING) return;
    if (ui.mode == UI_ONLINE && ui.local_color != ui.game.turn) {
        ui_set_status("等待对方落子");
        return;
    }
    piece = ui.game.board[data->y][data->x];
    if (ui.selected_x < 0) {
        if (chess_piece_color(piece) != ui.game.turn) {
            ui_set_status("请先选择己方棋子");
            return;
        }
        ui.selected_x = data->x;
        ui.selected_y = data->y;
        memset(ui.legal, 0, sizeof(ui.legal));
        int legal_x[CHESS_MAX_LEGAL_MOVES];
        int legal_y[CHESS_MAX_LEGAL_MOVES];
        size_t count = chess_legal_destinations(&ui.game, data->x, data->y, legal_x, legal_y);
        size_t i;
        for (i = 0; i < count; i++) ui.legal[legal_y[i]][legal_x[i]] = 1;
        ui_set_status("请选择目标位置");
    } else if (data->x == ui.selected_x && data->y == ui.selected_y) {
        ui_clear_selection();
    } else if (chess_piece_color(piece) == ui.game.turn) {
        ui_clear_selection();
        lv_event_send(ui.cell_objects[data->y][data->x], LV_EVENT_CLICKED, NULL);
        return;
    } else {
        ui_try_move(ui.selected_x, ui.selected_y, data->x, data->y);
        return;
    }
    ui_refresh_board();
}

static void ui_build_board(void)
{
    int y;
    int x;
    int grain;
    ui.board_panel = lv_obj_create(ui.root);
    lv_obj_remove_style_all(ui.board_panel);
    lv_obj_set_pos(ui.board_panel, BOARD_X, BOARD_Y);
    lv_obj_set_size(ui.board_panel, BOARD_W, BOARD_H);
    lv_obj_set_style_bg_color(ui.board_panel, COLOR_BOARD_TOP, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ui.board_panel, COLOR_BOARD_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.board_panel, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.board_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.board_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.board_panel, 5, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.board_panel, COLOR_BOARD_FRAME, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui.board_panel, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(ui.board_panel, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ui.board_panel, COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(ui.board_panel, LV_OPA_40, LV_PART_MAIN);

    for (grain = 0; grain < 16; grain++) {
        lv_obj_t *grain_line = lv_obj_create(ui.board_panel);
        lv_obj_remove_style_all(grain_line);
        lv_obj_set_pos(grain_line, 10, 14 + grain * 27);
        lv_obj_set_size(grain_line, BOARD_W - 20, 2);
        lv_obj_set_style_bg_color(grain_line, COLOR_BOARD_GRAIN, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(grain_line, (grain % 3 == 0) ? LV_OPA_30 : LV_OPA_10, LV_PART_MAIN);
    }

    {
        lv_obj_t *river_band = lv_obj_create(ui.board_panel);
        lv_obj_remove_style_all(river_band);
        lv_obj_set_pos(river_band, 10, BOARD_POINT_Y(4) - 4);
        lv_obj_set_size(river_band, BOARD_W - 20, BOARD_RIVER_GAP + 8);
        lv_obj_set_style_bg_color(river_band, COLOR_RIVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(river_band, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(river_band, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(river_band, COLOR_BOARD_FRAME, LV_PART_MAIN);
    }

    for (y = 0; y < CHESS_ROWS; y++) {
        lv_obj_t *line = lv_line_create(ui.board_panel);
        board_horizontal_points[y][0].x = BOARD_POINT_X(0);
        board_horizontal_points[y][0].y = BOARD_POINT_Y(y);
        board_horizontal_points[y][1].x = BOARD_POINT_X(CHESS_COLS - 1);
        board_horizontal_points[y][1].y = BOARD_POINT_Y(y);
        lv_line_set_points(line, board_horizontal_points[y], 2);
        lv_obj_set_size(line, BOARD_W, BOARD_H);
        lv_obj_add_style(line, &style_board_line, LV_PART_MAIN);
    }

    for (x = 0; x < CHESS_COLS; x++) {
        lv_obj_t *top_line = lv_line_create(ui.board_panel);
        board_vertical_top_points[x][0].x = BOARD_POINT_X(x);
        board_vertical_top_points[x][0].y = BOARD_POINT_Y(0);
        board_vertical_top_points[x][1].x = BOARD_POINT_X(x);
        board_vertical_top_points[x][1].y = BOARD_POINT_Y(4);
        lv_line_set_points(top_line, board_vertical_top_points[x], 2);
        lv_obj_set_size(top_line, BOARD_W, BOARD_H);
        lv_obj_add_style(top_line, &style_board_line, LV_PART_MAIN);

        if (x == 0 || x == CHESS_COLS - 1) {
            lv_obj_t *bottom_line = lv_line_create(ui.board_panel);
            board_vertical_bottom_points[x][0].x = BOARD_POINT_X(x);
            board_vertical_bottom_points[x][0].y = BOARD_POINT_Y(4);
            board_vertical_bottom_points[x][1].x = BOARD_POINT_X(x);
            board_vertical_bottom_points[x][1].y = BOARD_POINT_Y(CHESS_ROWS - 1);
            lv_line_set_points(bottom_line, board_vertical_bottom_points[x], 2);
            lv_obj_set_size(bottom_line, BOARD_W, BOARD_H);
            lv_obj_add_style(bottom_line, &style_board_line, LV_PART_MAIN);
        } else {
            lv_obj_t *bottom_line = lv_line_create(ui.board_panel);
            board_vertical_bottom_points[x][0].x = BOARD_POINT_X(x);
            board_vertical_bottom_points[x][0].y = BOARD_POINT_Y(5);
            board_vertical_bottom_points[x][1].x = BOARD_POINT_X(x);
            board_vertical_bottom_points[x][1].y = BOARD_POINT_Y(CHESS_ROWS - 1);
            lv_line_set_points(bottom_line, board_vertical_bottom_points[x], 2);
            lv_obj_set_size(bottom_line, BOARD_W, BOARD_H);
            lv_obj_add_style(bottom_line, &style_board_line, LV_PART_MAIN);
        }
    }

    board_palace_points[0][0].x = BOARD_POINT_X(3);
    board_palace_points[0][0].y = BOARD_POINT_Y(0);
    board_palace_points[0][1].x = BOARD_POINT_X(5);
    board_palace_points[0][1].y = BOARD_POINT_Y(2);
    board_palace_points[1][0].x = BOARD_POINT_X(5);
    board_palace_points[1][0].y = BOARD_POINT_Y(0);
    board_palace_points[1][1].x = BOARD_POINT_X(3);
    board_palace_points[1][1].y = BOARD_POINT_Y(2);
    board_palace_points[2][0].x = BOARD_POINT_X(3);
    board_palace_points[2][0].y = BOARD_POINT_Y(7);
    board_palace_points[2][1].x = BOARD_POINT_X(5);
    board_palace_points[2][1].y = BOARD_POINT_Y(9);
    board_palace_points[3][0].x = BOARD_POINT_X(5);
    board_palace_points[3][0].y = BOARD_POINT_Y(7);
    board_palace_points[3][1].x = BOARD_POINT_X(3);
    board_palace_points[3][1].y = BOARD_POINT_Y(9);
    for (x = 0; x < 4; x++) {
        lv_obj_t *palace_line = lv_line_create(ui.board_panel);
        lv_line_set_points(palace_line, board_palace_points[x], 2);
        lv_obj_set_size(palace_line, BOARD_W, BOARD_H);
        lv_obj_add_style(palace_line, &style_board_line, LV_PART_MAIN);
    }

    {
        static const int mark_positions[][2] = {
            {1, 2}, {7, 2}, {1, 7}, {7, 7},
            {2, 3}, {4, 3}, {6, 3}, {2, 6}, {4, 6}, {6, 6}
        };
        size_t mark;
        for (mark = 0; mark < sizeof(mark_positions) / sizeof(mark_positions[0]); mark++) {
            lv_obj_t *point = lv_obj_create(ui.board_panel);
            lv_obj_remove_style_all(point);
            lv_obj_set_pos(point, BOARD_POINT_X(mark_positions[mark][0]) - 4,
                           BOARD_POINT_Y(mark_positions[mark][1]) - 4);
            lv_obj_set_size(point, 8, 8);
            lv_obj_add_style(point, &style_board_mark, LV_PART_MAIN);
        }
    }

    make_label(ui.board_panel, "楚河", BOARD_POINT_X(1) + 18, BOARD_POINT_Y(4) + 4,
               180, BOARD_RIVER_GAP - 4, &style_river);
    make_label(ui.board_panel, "汉界", BOARD_POINT_X(5) - 18, BOARD_POINT_Y(4) + 4,
               180, BOARD_RIVER_GAP - 4, &style_river);

    for (y = 0; y < CHESS_ROWS; y++) {
        for (x = 0; x < CHESS_COLS; x++) {
            lv_obj_t *cell = lv_btn_create(ui.board_panel);
            ui.cell_data[y][x].x = x;
            ui.cell_data[y][x].y = y;
            ui.cell_objects[y][x] = cell;
            lv_obj_remove_style_all(cell);
            lv_obj_set_pos(cell, BOARD_POINT_X(x) - CELL_HIT_W / 2,
                           BOARD_POINT_Y(y) - CELL_HIT_H / 2);
            lv_obj_set_size(cell, CELL_HIT_W, CELL_HIT_H);
            lv_obj_add_style(cell, &style_board_cell, LV_PART_MAIN);
            lv_obj_add_event_cb(cell, ui_cell_event, LV_EVENT_CLICKED, &ui.cell_data[y][x]);
        }
    }

    for (y = 0; y < CHESS_ROWS; y++) {
        for (x = 0; x < CHESS_COLS; x++) {
            lv_obj_t *piece = lv_btn_create(ui.board_panel);
            lv_obj_t *label;
            lv_obj_remove_style_all(piece);
            lv_obj_set_pos(piece, BOARD_POINT_X(x) - PIECE_SIZE / 2,
                           BOARD_POINT_Y(y) - PIECE_SIZE / 2);
            lv_obj_set_size(piece, PIECE_SIZE, PIECE_SIZE);
            lv_obj_add_style(piece, &style_piece_black, LV_PART_MAIN);
            lv_obj_add_event_cb(piece, ui_cell_event, LV_EVENT_CLICKED, &ui.cell_data[y][x]);
            lv_obj_add_flag(piece, LV_OBJ_FLAG_HIDDEN);
            ui.piece_objects[y][x] = piece;
            label = lv_label_create(piece);
            lv_obj_set_style_text_font(label, UI_FONT, LV_PART_MAIN);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_center(label);
            ui.piece_labels[y][x] = label;
        }
    }
}

static void ui_show_board(void)
{
    lv_obj_t *left_backdrop;
    lv_obj_t *right_backdrop;
    ui_clear_root(0);
    ui_clear_selection();
    ui.result_shown = 0;

    left_backdrop = lv_obj_create(ui.root);
    lv_obj_remove_style_all(left_backdrop);
    lv_obj_set_pos(left_backdrop, 0, 0);
    lv_obj_set_size(left_backdrop, BOARD_X, SCREEN_H);
    lv_obj_set_style_bg_color(left_backdrop, COLOR_BG_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(left_backdrop, COLOR_BOARD_FRAME, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(left_backdrop, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(left_backdrop, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(left_backdrop, LV_OBJ_FLAG_CLICKABLE);

    right_backdrop = lv_obj_create(ui.root);
    lv_obj_remove_style_all(right_backdrop);
    lv_obj_set_pos(right_backdrop, BOARD_X + BOARD_W, 0);
    lv_obj_set_size(right_backdrop, SCREEN_W - BOARD_X - BOARD_W, SCREEN_H);
    lv_obj_set_style_bg_color(right_backdrop, COLOR_BG_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(right_backdrop, COLOR_BOARD_FRAME, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(right_backdrop, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(right_backdrop, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(right_backdrop, LV_OBJ_FLAG_CLICKABLE);

    ui_build_board();
    ui.side_panel = lv_obj_create(ui.root);
    lv_obj_add_style(ui.side_panel, &style_panel, LV_PART_MAIN);
    lv_obj_set_pos(ui.side_panel, SIDE_PANEL_X, SIDE_PANEL_Y);
    lv_obj_set_size(ui.side_panel, SIDE_PANEL_W, SIDE_PANEL_H);
    make_label(ui.side_panel, ui.mode == UI_LOCAL ? "本地练习" : "联机对战", 18, 16, 300, 30, &style_title);
    ui.turn_label = make_label(ui.side_panel, "", 18, 60, 300, 30, &style_status);
    ui.status_label = make_label(ui.side_panel, "选择红方棋子开始", 18, 98, 300, 38, &style_muted);
    ui.role_label = make_label(ui.side_panel, ui.mode == UI_LOCAL ? "同屏双人，红方先手" :
               (ui.local_color == CHESS_RED ? "本机：红方" :
                (ui.local_color == CHESS_BLACK ? "本机：黑方" : "本机：等待服务器分配")),
               18, 150, 300, 28, &style_muted);
    make_label(ui.side_panel, "协议：JSON / TCP\n端口：6818    分辨率：800×480", 18, 196, 300, 58, &style_muted);
    if (ui.mode == UI_ONLINE) {
        make_button(ui.side_panel, "认输", 18, 270, 95, 42, ui_resign_event, 0);
        make_button(ui.side_panel, "和棋", 123, 270, 95, 42, ui_draw_event, 0);
        make_button(ui.side_panel, "断开对局", 228, 270, 97, 42, ui_back_to_menu_event, 0);
    }
    make_button(ui.side_panel, "返回菜单", 18, 330, 145, 48, ui_back_to_menu_event, 0);
    ui.side_toggle = make_button(ui.root, ">", SIDE_TOGGLE_X, SIDE_TOGGLE_Y,
                                 SIDE_TOGGLE_W, SIDE_TOGGLE_H, ui_toggle_side_event, 1);
    lv_obj_add_flag(ui.side_panel, LV_OBJ_FLAG_HIDDEN);
    ui_refresh_board();
    if (ui.mode == UI_ONLINE) ui.timer = lv_timer_create(ui_network_timer, 60, NULL);
}

static void ui_network_timer(lv_timer_t *timer)
{
    NetMessage message;
    (void)timer;
    if (ui.page == UI_HOST_WAIT && !ui.net.connected) {
        if (net_host_accept(&ui.net) > 0) {
            if (net_send_hello(&ui.net, CHESS_BLACK) != 0) {
                net_close(&ui.net);
                ui_show_online_menu();
                make_label(ui.root, "发送执棋方信息失败，请重新创建房间", 46, 350, 650, 32, &style_status);
                return;
            }
            ui.page = UI_BOARD;
            ui.mode = UI_ONLINE;
            ui.local_color = CHESS_RED;
            chess_init(&ui.game);
            ui_show_board();
            ui_set_status("客机已连接，红方先手");
        }
        return;
    }
    if (ui.page != UI_BOARD || ui.mode != UI_ONLINE) return;
    if (!ui.net.connected) {
        ui.local_color = CHESS_NONE;
        ui_set_status("网络连接已断开");
        return;
    }
    while (net_poll(&ui.net, &message) > 0) {
        if (message.type == NET_MSG_HELLO) {
            ui.local_color = message.color == 1 ? CHESS_RED : CHESS_BLACK;
            if (ui.role_label) lv_label_set_text(ui.role_label, ui.local_color == CHESS_RED ? "本机：红方" : "本机：黑方");
            ui_set_status(ui.local_color == CHESS_RED ? "服务器分配：红方先手" : "服务器分配：黑方后手");
        } else if (message.type == NET_MSG_MOVE) {
            if (ui.local_color != CHESS_NONE && ui.local_color != ui.game.turn &&
                chess_apply_move(&ui.game, message.from_x, message.from_y, message.to_x, message.to_y)) {
                ui_clear_selection();
                ui_set_status("收到对方落子");
                ui_refresh_board();
                if (ui.game.result != CHESS_ONGOING) ui_show_result();
            }
        } else if (message.type == NET_MSG_RESULT) {
            ui.game.result = message.winner == 1 ? CHESS_RED_WIN :
                             (message.winner == -1 ? CHESS_BLACK_WIN : CHESS_DRAW);
            ui_refresh_board();
            ui_show_result();
        } else if (message.type == NET_MSG_RESIGN) {
            if (ui.local_color == CHESS_NONE || message.color != -ui.local_color) continue;
            ui.game.result = message.color == 1 ? CHESS_BLACK_WIN : CHESS_RED_WIN;
            ui_set_status("对方认输");
            ui_refresh_board();
            ui_show_result();
        } else if (message.type == NET_MSG_DRAW) {
            if (ui.local_color == CHESS_NONE || message.color != -ui.local_color) continue;
            ui.game.result = CHESS_DRAW;
            ui_set_status("对方提议和棋");
            ui_refresh_board();
            ui_show_result();
        } else if (message.type == NET_MSG_DISCONNECT) {
            ui_set_status("对方已断开连接");
            ui.local_color = CHESS_NONE;
            net_close(&ui.net);
            break;
        }
    }
}

static void ui_local_event(lv_event_t *event)
{
    (void)event;
    ui.mode = UI_LOCAL;
    ui.page = UI_BOARD;
    ui.local_color = CHESS_NONE;
    chess_init(&ui.game);
    ui_show_board();
}

static void ui_online_event(lv_event_t *event)
{
    (void)event;
    ui_show_online_menu();
}

static void ui_host_event(lv_event_t *event)
{
    (void)event;
    if (net_host_start(&ui.net, NET_DEFAULT_PORT) != 0) {
        ui_show_online_menu();
        make_label(ui.root, "监听端口失败，请检查网络", 46, 350, 600, 32, &style_status);
        return;
    }
    ui.page = UI_HOST_WAIT;
    ui_clear_root(0);
    make_label(ui.root, "创建房间", 46, 36, 400, 42, &style_title);
    make_label(ui.root, "主机执红 · 等待客机加入", 46, 88, 400, 32, &style_muted);
    make_label(ui.root, "端口 6818\n请在另一块开发板输入本机局域网 IP", 46, 148, 500, 70, &style_status);
    make_button(ui.root, "返回", 46, 350, 150, 48, ui_back_to_menu_event, 0);
    ui.timer = lv_timer_create(ui_network_timer, 100, NULL);
}

static void ui_join_event(lv_event_t *event)
{
    const char *ip;
    const char *port_text;
    const char *room;
    int port;
    char *port_end;
    long parsed_port;
    (void)event;
    ip = lv_textarea_get_text(ui.ip_textarea);
    port_text = lv_textarea_get_text(ui.port_textarea);
    room = lv_textarea_get_text(ui.room_textarea);
    if (!room || room[0] == '\0' || strlen(room) >= 32) {
        ui_set_status("房间号不能为空且不能超过 31 个字符");
        return;
    }
    errno = 0;
    parsed_port = strtol(port_text ? port_text : "", &port_end, 10);
    if (errno != 0 || port_end == (port_text ? port_text : "") || *port_end != '\0' ||
        parsed_port < 1 || parsed_port > 65535 || parsed_port > INT_MAX) {
        ui_set_status("端口必须是 1 到 65535 之间的数字");
        return;
    }
    port = (int)parsed_port;
    if (net_client_connect(&ui.net, ip, (unsigned short)port) != 0) {
        ui_set_status("连接失败，请确认 IP、端口和防火墙");
        return;
    }
    if (net_send_join(&ui.net, room) != 0) {
        net_close(&ui.net);
        ui_set_status("房间号发送失败");
        return;
    }
    ui.page = UI_BOARD;
    ui.mode = UI_ONLINE;
    ui.local_color = CHESS_NONE;
    chess_init(&ui.game);
    ui_show_board();
    ui_set_status("已连接云端，等待服务器分配执棋方");
}

static void ui_join_form_event(lv_event_t *event)
{
    (void)event;
    ui_show_join_form();
}

static void ui_show_join_form(void)
{
    ui_clear_root(1);
    ui.page = UI_JOIN_FORM;
    make_label(ui.root, "加入房间", 46, 20, 400, 42, &style_title);
    make_label(ui.root, "输入阿里云 ECS 公网 IPv4 地址", 46, 62, 500, 30, &style_muted);
    make_label(ui.root, "服务器 IP", 46, 105, 120, 28, &style_muted);
    ui.ip_textarea = lv_textarea_create(ui.root);
    lv_obj_set_pos(ui.ip_textarea, 170, 96);
    lv_obj_set_size(ui.ip_textarea, 300, 42);
    lv_textarea_set_one_line(ui.ip_textarea, true);
    lv_textarea_set_text(ui.ip_textarea, DEFAULT_DEBUG_IP);
    lv_obj_add_style(ui.ip_textarea, &style_button, LV_PART_MAIN);
    lv_obj_add_flag(ui.ip_textarea, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    ui_add_textarea_trigger(ui.root, ui.ip_textarea);
    make_label(ui.root, "端口", 46, 157, 120, 28, &style_muted);
    ui.port_textarea = lv_textarea_create(ui.root);
    lv_obj_set_pos(ui.port_textarea, 170, 148);
    lv_obj_set_size(ui.port_textarea, 150, 42);
    lv_textarea_set_one_line(ui.port_textarea, true);
    lv_textarea_set_accepted_chars(ui.port_textarea, "0123456789");
    lv_textarea_set_text(ui.port_textarea, "6818");
    lv_obj_add_style(ui.port_textarea, &style_button, LV_PART_MAIN);
    lv_obj_add_flag(ui.port_textarea, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    ui_add_textarea_trigger(ui.root, ui.port_textarea);
    make_label(ui.root, "房间号", 46, 209, 120, 28, &style_muted);
    ui.room_textarea = lv_textarea_create(ui.root);
    lv_obj_set_pos(ui.room_textarea, 170, 200);
    lv_obj_set_size(ui.room_textarea, 200, 42);
    lv_textarea_set_one_line(ui.room_textarea, true);
    lv_textarea_set_max_length(ui.room_textarea, 31);
    lv_textarea_set_accepted_chars(ui.room_textarea, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_ ");
    lv_textarea_set_text(ui.room_textarea, "6818-demo");
    lv_obj_add_style(ui.room_textarea, &style_button, LV_PART_MAIN);
    lv_obj_add_flag(ui.room_textarea, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    ui_add_textarea_trigger(ui.root, ui.room_textarea);
    make_button(ui.root, "连接云端", 170, 278, 150, 48, ui_join_event, 1);
    make_button(ui.root, "返回", 340, 278, 130, 48, ui_back_to_menu_event, 0);
    ui.form_status_label = make_label(ui.root, "点击输入框后使用屏幕键盘", 46, 350, 700, 30, &style_muted);
    ui.keyboard = lv_obj_create(ui.root);
    lv_obj_remove_style_all(ui.keyboard);
    lv_obj_set_pos(ui.keyboard, 0, 248);
    lv_obj_set_size(ui.keyboard, SCREEN_W, 232);
    lv_obj_set_style_bg_color(ui.keyboard, COLOR_PANEL_2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.keyboard, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_clear_flag(ui.keyboard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui.keyboard, LV_OBJ_FLAG_HIDDEN);
    ui.keyboard_mode = -1;
}

static void ui_show_online_menu(void)
{
    ui_clear_root(1);
    ui.page = UI_ONLINE_MENU;
    make_label(ui.root, "联机对战", 46, 36, 500, 48, &style_title);
    make_label(ui.root, "阿里云 TCP 中转 / 局域网主机调试", 46, 92, 560, 30, &style_muted);
    make_button(ui.root, "创建房间（主机）", 46, 160, 250, 58, ui_host_event, 1);
    make_button(ui.root, "云端对战（阿里云）", 320, 160, 250, 58, ui_join_form_event, 0);
    make_button(ui.root, "返回主菜单", 46, 282, 180, 48, ui_back_to_menu_event, 0);
}

static void ui_show_menu(void)
{
    lv_obj_t *top_line;
    lv_obj_t *left_mark;
    lv_obj_t *right_mark;
    lv_obj_t *footer_line;
    lv_obj_t *main_title;
    lv_obj_t *subtitle;
    lv_obj_t *mode_label;
    ui_clear_root(1);
    ui.page = UI_MENU;
    lv_obj_set_style_bg_color(ui.root, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ui.root, COLOR_BG_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.root, LV_GRAD_DIR_VER, LV_PART_MAIN);

    left_mark = lv_obj_create(ui.root);
    lv_obj_remove_style_all(left_mark);
    lv_obj_set_pos(left_mark, 0, 0);
    lv_obj_set_size(left_mark, 12, SCREEN_H);
    lv_obj_set_style_bg_color(left_mark, COLOR_MENU_WARM, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(left_mark, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(left_mark, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    right_mark = lv_obj_create(ui.root);
    lv_obj_remove_style_all(right_mark);
    lv_obj_set_pos(right_mark, SCREEN_W - 12, 0);
    lv_obj_set_size(right_mark, 12, SCREEN_H);
    lv_obj_set_style_bg_color(right_mark, COLOR_MENU_TEAL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(right_mark, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(right_mark, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    make_menu_piece_badge(ui.root, "帅", 1, 244, 24);
    main_title = make_label(ui.root, "中国象棋", 325, 30, 150, 34, &style_title);
    lv_obj_set_style_text_align(main_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    make_menu_piece_badge(ui.root, "将", 0, 488, 24);
    top_line = lv_obj_create(ui.root);
    lv_obj_remove_style_all(top_line);
    lv_obj_set_pos(top_line, 180, 106);
    lv_obj_set_size(top_line, 440, 2);
    lv_obj_set_style_bg_color(top_line, COLOR_MENU_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top_line, LV_OPA_70, LV_PART_MAIN);
    lv_obj_clear_flag(top_line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    subtitle = make_label(ui.root, "6818 联机棋局 · LVGL 800×480", 220, 73, 360, 26, &style_muted);
    lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    mode_label = make_label(ui.root, "选择对局模式", 300, 114, 200, 24, &style_status);
    lv_obj_set_style_text_align(mode_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    make_menu_card(ui.root, "本地练习", "同屏双人轮流走棋", "开始本地对局", "帅", 1, 54, ui_local_event);
    make_menu_card(ui.root, "联机对战", "局域网 / 云端 TCP", "创建 / 加入房间", "将", 0, 416, ui_online_event);

    footer_line = lv_obj_create(ui.root);
    lv_obj_remove_style_all(footer_line);
    lv_obj_set_pos(footer_line, 54, 376);
    lv_obj_set_size(footer_line, 692, 1);
    lv_obj_set_style_bg_color(footer_line, COLOR_PANEL_2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(footer_line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(footer_line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    make_label(ui.root, "本地规则校验 · 屏幕操作", 54, 394, 330, 26, &style_muted);
    make_label(ui.root, "6818 开发板 / Linux framebuffer", 424, 394, 322, 26, &style_muted);
}

void chess_ui_init(void)
{
    memset(&ui, 0, sizeof(ui));
    ui.selected_x = -1;
    ui.selected_y = -1;
    net_init();
    net_context_init(&ui.net);
    ui_init_styles();
    ui_show_menu();
}
