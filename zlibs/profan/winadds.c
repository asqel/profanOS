#include <syscall.h>
#include <stdlib.h>
#include <string.h>

#include <i_libdaube.h>

#define BUFFER_SIZE 0x1000

char *char_buffer;
int curent_len;

int main() {
    char_buffer = calloc(BUFFER_SIZE, sizeof(char));
    curent_len = 0;

    return 0;
}

void wterm_append_string(char *str) {
    int len = strlen(str);
    if (curent_len + len > BUFFER_SIZE - 2) {
        for (int i = 0; i < BUFFER_SIZE - len; i++) {
            char_buffer[i] = char_buffer[i + len];
        }
        curent_len -= len;
    }

    for (int i = 0; i < len; i++) {
        char_buffer[curent_len + i] = str[i];
    }

    curent_len += len;
    char_buffer[curent_len] = 0;
}

void wterm_append_char(char c) {
    if (curent_len + 1 > BUFFER_SIZE - 2) {
        for (int i = 0; i < BUFFER_SIZE - 1; i++) {
            char_buffer[i] = char_buffer[i + 1];
        }
    }
    char_buffer[curent_len] = c;
    curent_len++;
    if (curent_len >= BUFFER_SIZE) {
        curent_len = BUFFER_SIZE - 1;
    }
    char_buffer[curent_len] = 0;
}

char *wterm_get_buffer() {
    return char_buffer;
}

int wterm_get_len() {
    return curent_len;
}

button_t *wadds_create_exitbt(window_t *window, void (*exit_callback)(clickevent_t *)) {
    int x = window->width - 3;

    // draw bg rectangle
    for (int i = 3; i < 16; i++) {
        for (int j = 3; j < 16; j++) {
            window_set_pixel_out(window, x - i, j, 0x880000);
        }
    }

    // draw cross
    for (int i = 3; i < 16; i++) {
        for (int j = -1; j < 2; j++) {
            window_set_pixel_out(window, x - i, (i + j != 2) ? i + j : 3, 0xa7a0b9);
            window_set_pixel_out(window, x - (19 - i), (i + j != 2) ? i + j : 3, 0xa7a0b9);
        }
    }

    // draw outlines
    for (int i = 3; i < 16; i++) {
        window_set_pixel_out(window, x - i, 3, 0x880000);
        window_set_pixel_out(window, x - i - 1, 16, 0x880000);
        window_set_pixel_out(window, x - 3, i + 1, 0x880000);
        window_set_pixel_out(window, x - 16, i, 0x880000);
    }

    return create_button(window, x - 18, 3, 16, 16, exit_callback);
}

void wadds_line(window_t *window, int x1, int y1, int x2, int y2, int color) {
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        window_set_pixel(window, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void wadds_putc(window_t *window, int x, int y, char c, uint32_t color, uint32_t bg_color) {
    uint8_t *glyph = c_font_get(0) + c * 16;
    for (int j = 0; j < 16; j++) {
        for (int k = 0; k < 8; k++) {
            window_set_pixel(window, x + 8 - k, y + j, (glyph[j] & (1 << k)) ? color : bg_color);
        }
    }
}