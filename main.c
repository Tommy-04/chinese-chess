#define _DEFAULT_SOURCE

#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include "src/ui.h"

#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#define DISP_HOR_RES 800
#define DISP_VER_RES 480
#define DISP_BUF_SIZE (DISP_HOR_RES * DISP_VER_RES)

uint32_t custom_tick_get(void);

int main(void)
{
    static lv_color_t display_buffer[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t display_drv;
    static lv_indev_drv_t input_drv;

    lv_init();
    fbdev_init();
    lv_disp_draw_buf_init(&draw_buf, display_buffer, NULL, DISP_BUF_SIZE);
    lv_disp_drv_init(&display_drv);
    display_drv.draw_buf = &draw_buf;
    display_drv.flush_cb = fbdev_flush;
    display_drv.hor_res = DISP_HOR_RES;
    display_drv.ver_res = DISP_VER_RES;
    lv_disp_drv_register(&display_drv);

    evdev_init();
    lv_indev_drv_init(&input_drv);
    input_drv.type = LV_INDEV_TYPE_POINTER;
    input_drv.read_cb = evdev_read;
    lv_indev_drv_register(&input_drv);

    chess_ui_init();
    for (;;) {
        lv_timer_handler();
        usleep(5000);
    }
    return 0;
}

uint32_t custom_tick_get(void)
{
    static uint64_t start_ms;
    struct timeval now;
    uint64_t now_ms;
    if (start_ms == 0) {
        gettimeofday(&now, NULL);
        start_ms = (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_usec / 1000U;
    }
    gettimeofday(&now, NULL);
    now_ms = (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_usec / 1000U;
    return (uint32_t)(now_ms - start_ms);
}
