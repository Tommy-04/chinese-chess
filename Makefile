# STM32MP157 (正点原子 ATK) 构建配置，交叉编译用 arm-none-linux-gnueabihf-gcc
CC ?= gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= $(CURDIR)
LV_DRIVERS_DIR_NAME ?= lv_drivers

CPPFLAGS += -I$(LVGL_DIR) -I$(LVGL_DIR)/src -I$(LVGL_DIR)/$(LVGL_DIR_NAME) -Isrc -include src/platform.h
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wshadow -Wundef -Wpointer-arith \
          -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare \
          -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700
LDFLAGS ?= -lm -lpthread
BIN ?= chinese_xiangqi
CHESS_TEST_BIN ?= chess_test_run
NET_TEST_BIN ?= net_test_run

MAINSRC = main.c
CSRCS = src/chess.c src/net.c src/cJSON.c src/ui.c
# Keep the firmware build focused on the LVGL modules used by this app.
# This avoids the demo/example make fragments that invoke a shell `find`.
include $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/core/lv_core.mk
include $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/draw/lv_draw.mk
include $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/font/lv_font.mk
include $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/hal/lv_hal.mk
include $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/misc/lv_misc.mk
include $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/widgets/lv_widgets.mk
# The app uses LVGL's optional extra modules. Keep the list explicit so the
# firmware build does not pull in host-specific or unused extra components.
CSRCS += $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/lv_extra.c
CSRCS += $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/themes/default/lv_theme_default.c
CSRCS += $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/layouts/flex/lv_flex.c
CSRCS += $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/widgets/keyboard/lv_keyboard.c
CSRCS += $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/widgets/msgbox/lv_msgbox.c
CPPFLAGS += -I$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra
CPPFLAGS += -I$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/themes/default
CPPFLAGS += -I$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/layouts/flex
CPPFLAGS += -I$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/widgets/keyboard
CPPFLAGS += -I$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra/widgets/msgbox
# Only the drivers wired up by main.c belong in the firmware.  The upstream
# lv_drivers.mk wildcard also pulls in SDL/Wayland/Windows and optional input
# drivers, which makes a board build depend on unrelated host headers.
CSRCS += $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/display/fbdev.c
CSRCS += $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/indev/evdev.c

OBJEXT ?= .build.o
COBJS = $(CSRCS:.c=$(OBJEXT))
MAINOBJ = $(MAINSRC:.c=$(OBJEXT))

.PHONY: all clean test test-chess test-net test-ui

all: $(BIN)

%$(OBJEXT): %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN): $(COBJS) $(MAINOBJ)
	$(CC) -o $@ $(MAINOBJ) $(COBJS) $(LDFLAGS)

test: test-chess test-net test-ui

test-chess: src/chess_test.c src/chess.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCHESS_TEST_MAIN -o $(CHESS_TEST_BIN) src/chess_test.c src/chess.c
	./$(CHESS_TEST_BIN)

test-net: src/net_test.c src/net.c src/cJSON.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $(NET_TEST_BIN) src/net_test.c src/net.c src/cJSON.c $(LDFLAGS)
	./$(NET_TEST_BIN)

test-ui: src/ui$(OBJEXT) lv_font_simsun_16_cjk$(OBJEXT)

clean:
	$(RM) $(COBJS) $(MAINOBJ)
	rm -f $(BIN) $(CHESS_TEST_BIN) $(NET_TEST_BIN)
