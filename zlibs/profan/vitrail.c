#include <stdlib.h>
#include <stdio.h>

#include <syscall.h>

#define VITRAIL_C
#include <vitrail.h>

#define bg_color 0x100820

#define min(a, b) ((a) < (b) ? (a) : (b))

void init(void);
int main(void) {
    init();
    return 0;
}

void draw_bg(uint32_t x, uint32_t y, uint32_t size_x, uint32_t size_y, uint32_t color) {
    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t y_ = 0; y_ < size_y; y_++) {
        for (uint32_t x_ = 0; x_ < size_x; x_++) {
            fb[(y + y_) * pitch + x + x_] = color;
        }
    }
}

window_t *create_window(int pos_x, int pos_y, int size_x, int size_y) {
    window_t *window = malloc(sizeof(window_t));
    window->pos_x = pos_x;
    window->pos_y = pos_y;
    window->size_x = size_x;
    window->size_y = size_y;
    window->pixels = malloc(size_x * size_y * sizeof(uint32_t));
    return window;
}

void destroy_window(window_t *window) {
    draw_bg(window->pos_x, window->pos_y, window->size_x, window->size_y, bg_color);
    free(window->pixels);
    free(window);
}

void refresh_window(window_t *window) {
    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t y = 0; y < window->size_y; y++) {
        for (uint32_t x = 0; x < window->size_x; x++) {
            fb[(window->pos_y + y) * pitch + window->pos_x + x] = window->pixels[y * window->size_x + x];
        }
    }
}

void move_window(window_t *window, uint32_t pos_x, uint32_t pos_y) {
    if (pos_x > window->pos_x && pos_y > window->pos_y) {
        draw_bg(window->pos_x, window->pos_y,
            min(pos_x - window->pos_x, window->size_x),
            window->size_y, 0xFF0000);
        if (pos_x - window->pos_x < window->size_x)
            draw_bg(window->pos_x + pos_x - window->pos_x,
                window->pos_y, window->size_x - (pos_x - window->pos_x),
                min(pos_y - window->pos_y, window->size_y), 0x00FF00);
    } else if (pos_x < window->pos_x && pos_y < window->pos_y) {
        draw_bg(window->size_x + pos_x, window->pos_y,
            min(window->pos_x - pos_x, window->size_x),
            window->size_y, 0xFF0000);
        if (window->pos_x - pos_x < window->size_x)
            draw_bg(window->pos_x, window->size_y + pos_y,
                window->size_x - (window->pos_x - pos_x),
                min(window->pos_y - pos_y, window->size_y), 0x00FF00);
    } else if (pos_x < window->pos_x && pos_y > window->pos_y) {
        draw_bg(window->size_x + pos_x, window->pos_y,
            min(window->pos_x - pos_x, window->size_x),
            window->size_y, 0xFF0000);
        if (window->pos_x - pos_x < window->size_x)
            draw_bg(window->pos_x, window->pos_y,
                window->size_x - (window->pos_x - pos_x),
                min(pos_y - window->pos_y, window->size_y), 0x00FF00);
    } else if (pos_x > window->pos_x && pos_y < window->pos_y) {
        draw_bg(window->pos_x, window->pos_y,
            min(pos_x - window->pos_x, window->size_x),
            window->size_y, 0xFF0000);
        if (pos_x - window->pos_x < window->size_x)
            draw_bg(window->pos_x + pos_x - window->pos_x,
                window->size_y + pos_y, window->size_x - (pos_x - window->pos_x),
                min(window->pos_y - pos_y, window->size_y), 0x00FF00);
    }

    window->pos_x = pos_x;
    window->pos_y = pos_y;
    refresh_window(window);
}

void init(void) {
    draw_bg(0, 0, 1024, 768, bg_color);

    window_t *window = create_window(100, 100, 200, 200);

    for (uint32_t y = 0; y < window->size_y; y++) {
        for (uint32_t x = 0; x < window->size_x; x++) {
            window->pixels[y * window->size_x + x] = rand() % 0xFFFFFF;
        }
    }
    refresh_window(window);

    move_window(window, 150, 50);
}
