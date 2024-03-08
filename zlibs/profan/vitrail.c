#include <stdlib.h>
#include <stdio.h>
#include <profan.h>
#include <syscall.h>

#define VITRAIL_C
#include <vitrail.h>

#define OUTLINE_COLOR 0xAAAAAA
#define MAX_WINDOWS 100
#define WINDOWS_GAP 10

#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFF
#endif

uint32_t *main_bg;
uint32_t *blurred_bg;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

window_t **windows;

void init(void);
int main(void) {
    init();
    return 0;
}

void draw_window_border(window_t *window, int errase);
void refresh_window(window_t *window);
void reorganize_windows(void);

void draw_bg(uint32_t x, uint32_t y, uint32_t size_x, uint32_t size_y) {
    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t i = 0; i < size_y; i++) {
        for (uint32_t j = 0; j < size_x; j++) {
            fb[(y + i) * pitch + x + j] = main_bg[((y + i) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (x + j) % VITRAIL_BG_SIDE];
        }
    }
}

window_t *create_window(uint32_t size_x, uint32_t size_y, int enable_border) {
    window_t *window = malloc(sizeof(window_t));
    window->pos_x = UINT32_MAX;
    window->pos_y = UINT32_MAX;
    window->size_x = size_x;
    window->size_y = size_y;

    window->pixels = calloc(size_x * size_y, sizeof(uint32_t));

    window->vesa_fb = c_vesa_get_fb();
    window->vesa_pitch = c_vesa_get_pitch();
    window->vesa_width = c_vesa_get_width();

    window->bg = blurred_bg;
    window->enable_border = enable_border;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i]) {
            windows[i] = window;
            windows[i + 1] = NULL;
            break;
        }
    }

    reorganize_windows();

    return window;
}

void destroy_window(window_t *window) {
    draw_bg(window->pos_x, window->pos_y, window->size_x, window->size_y);
    free(window->pixels);
    free(window);

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i] != window)
            continue;
        for (int j = i; j < MAX_WINDOWS - 1; j++) {
            windows[j] = windows[j + 1];
        }
        break;
    }
}

void refresh_window(window_t *window) {
    if (window->pos_x == UINT32_MAX || window->pos_y == UINT32_MAX)
        return;

    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    uint32_t color, alpha, bg_color;

    for (uint32_t y = 0; y < window->size_y; y++) {
        for (uint32_t x = 0; x < window->size_x; x++) {
            color = window->pixels[y * window->size_x + x];
            alpha = (color >> 24) & 0xFF;
            if (alpha == 0xFF) {
                fb[(window->pos_y + y) * pitch + window->pos_x + x] = color;
                continue;
            }
            bg_color = window->bg[((window->pos_y + y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + x) % VITRAIL_BG_SIDE];
            fb[(window->pos_y + y) * pitch + window->pos_x + x] =
                ((color & 0xFF) * alpha + (bg_color & 0xFF) * (255 - alpha)) / 255 |
                (((color >> 8) & 0xFF) * alpha + ((bg_color >> 8) & 0xFF) * (255 - alpha)) / 255 << 8 |
                (((color >> 16) & 0xFF) * alpha + ((bg_color >> 16) & 0xFF) * (255 - alpha)) / 255 << 16;
        }
    }
}

void move_window(window_t *window, uint32_t pos_x, uint32_t pos_y) {
    if (pos_x == window->pos_x && pos_y == window->pos_y)
        return;

    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t y = window->pos_y; y < window->pos_y + window->size_y; y++) {
        for (uint32_t x = window->pos_x; x < window->pos_x + window->size_x; x++) {
            if (x < pos_x || x >= pos_x + window->size_x || y < pos_y || y >= pos_y + window->size_y) {
                fb[y * pitch + x] = main_bg[(y % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + x % VITRAIL_BG_SIDE];
            }
        }
    }

    if (window->enable_border)
        draw_window_border(window, 1);

    window->pos_x = pos_x;
    window->pos_y = pos_y;
}

void draw_window_border(window_t *window, int errase) {
    if (!window->enable_border)
        return;
    if (window->pos_x == UINT32_MAX || window->pos_y == UINT32_MAX)
        return;

    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t x = 0; x < window->size_x; x++) {
        fb[((window->pos_y - 2) * pitch) + window->pos_x + x] = errase ? main_bg[((window->pos_y - 2) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + x) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
        fb[(window->pos_y + window->size_y + 1) * pitch + window->pos_x + x] = errase ? main_bg[((window->pos_y + window->size_y + 1) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + x) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;

        fb[((window->pos_y - 1) * pitch) + window->pos_x + x] = errase ? main_bg[((window->pos_y - 1) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + x) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
        fb[(window->pos_y + window->size_y) * pitch + window->pos_x + x] = errase ? main_bg[((window->pos_y + window->size_y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + x) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
    }

    for (uint32_t y = 0; y < window->size_y; y++) {
        fb[(window->pos_y + y) * pitch + window->pos_x - 2] = errase ? main_bg[((window->pos_y + y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x - 2) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
        fb[(window->pos_y + y) * pitch + window->pos_x + window->size_x + 1] = errase ? main_bg[((window->pos_y + y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + window->size_x + 1) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;

        fb[(window->pos_y + y) * pitch + window->pos_x - 1] = errase ? main_bg[((window->pos_y + y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x - 1) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
        fb[(window->pos_y + y) * pitch + window->pos_x + window->size_x] = errase ? main_bg[((window->pos_y + y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + window->size_x) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
    }

    fb[(window->pos_y - 1) * pitch + window->pos_x - 1] = errase ? main_bg[((window->pos_y - 1) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x - 1) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
    fb[(window->pos_y - 1) * pitch + window->pos_x + window->size_x] = errase ? main_bg[((window->pos_y - 1) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + window->size_x) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
    fb[(window->pos_y + window->size_y) * pitch + window->pos_x - 1] = errase ? main_bg[((window->pos_y + window->size_y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x - 1) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
    fb[(window->pos_y + window->size_y) * pitch + window->pos_x + window->size_x] = errase ? main_bg[((window->pos_y + window->size_y) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + (window->pos_x + window->size_x) % VITRAIL_BG_SIDE] : OUTLINE_COLOR;
}


uint32_t *open_bg_bmp(char *path) {
    // check if file exists
    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("File %s not found\n", path);
        return NULL;
    }

    // open file
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *file_content = malloc(file_size);
    fread(file_content, 1, file_size, file);
    fclose(file);

    // check if file is a bmp
    if (file_content[0] != 'B' || file_content[1] != 'M') {
        free(file_content);
        printf("File %s is not a bmp\n", path);
        return NULL;
    }

    // get image data
    int width = *(int *)(file_content + 18);
    int height = *(int *)(file_content + 22);
    int offset = *(int *)(file_content + 10);
    int size = *(int *)(file_content + 34);
    uint8_t *data = file_content + offset;

    if (width != VITRAIL_BG_SIDE || height != VITRAIL_BG_SIDE) {
        free(file_content);
        printf("File %s has invalid dimensions\n", path);
        return NULL;
    }

    int factor = size / (width * height);

    // copy image data to buffer
    if (factor != 3 && factor != 4) {
        free(file_content);
        printf("File %s has invalid pixel format\n", path);
        return NULL;
    }

    uint32_t *output = malloc(width * height * sizeof(uint32_t));

    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            uint32_t color = data[(j * width + i) * factor] |
                            (data[(j * width + i) * factor + 1] << 8) |
                            (data[(j * width + i) * factor + 2] << 16);

            if (factor == 4 && data[(j * width + i) * factor + 3] == 0) continue;
            output[width * height - (i + j * width + 1)] = color;
        }
    }

    free(file_content);
    return output;
}

void sort_windows(int nb_windows) {
    for (int i = 0; i < nb_windows; i++) {
        for (int j = i + 1; j < nb_windows; j++) {
            if (windows[i]->size_x * windows[i]->size_y < windows[j]->size_x * windows[j]->size_y) {
                window_t *tmp = windows[i];
                windows[i] = windows[j];
                windows[j] = tmp;
            }
        }
    }
}

void find_place_in_grid(uint32_t *grid, uint32_t grid_size_x, uint32_t grid_size_y, uint32_t size_x, uint32_t size_y, uint32_t *pos_x, uint32_t *pos_y) {
    for (uint32_t y = 0; y < grid_size_y - size_y; y++) {
        for (uint32_t x = 0; x < grid_size_x - size_x; x++) {
            int is_free = 1;
            for (uint32_t i = 0; i < size_y; i++) {
                for (uint32_t j = 0; j < size_x; j++) {
                    if (grid[(y + i) * grid_size_x + x + j]) {
                        is_free = 0;
                        break;
                    }
                }
                if (!is_free) break;
            }
            if (is_free) {
                *pos_x = x;
                *pos_y = y;
                return;
            }
        }
    }
    *pos_x = 0;
    *pos_y = 0;
}

void reorganize_windows(void) {
    int nb_windows;
    for (nb_windows = 0; nb_windows < MAX_WINDOWS; nb_windows++) {
        if (windows[nb_windows] == NULL) {
            break;
        }
    }

    sort_windows(nb_windows);

    uint32_t grid_size_x = c_vesa_get_width() / WINDOWS_GAP - 1;
    uint32_t grid_size_y = c_vesa_get_height() / WINDOWS_GAP - 1;

    uint32_t *grid = calloc(grid_size_x * grid_size_y, sizeof(uint32_t));

    for (int i = 0; i < nb_windows; i++) {
        uint32_t pos_x, pos_y;
        find_place_in_grid(grid, grid_size_x, grid_size_y, windows[i]->size_x / WINDOWS_GAP + 1, windows[i]->size_y / WINDOWS_GAP + 1, &pos_x, &pos_y);
        for (uint32_t y = pos_y; y < pos_y + windows[i]->size_y / WINDOWS_GAP + 2; y++) {
            for (uint32_t x = pos_x; x < pos_x + windows[i]->size_x / WINDOWS_GAP + 2; x++) {
                if (x >= grid_size_x || y >= grid_size_y) continue;
                grid[y * grid_size_x + x] = 1;
            }
        }
        pos_x++;
        pos_y++;
        move_window(windows[i], pos_x * WINDOWS_GAP, pos_y * WINDOWS_GAP);
    }
    for (uint32_t i = 0; i < grid_size_y; i++) {
        for (uint32_t j = 0; j < grid_size_x; j++) {
            serial_debug("%d", grid[i * grid_size_x + j]);
        }
        serial_debug("\n");
    }
    serial_debug("\n");

    free(grid);

    for (int i = 0; i < nb_windows; i++) {
        refresh_window(windows[i]);
        draw_window_border(windows[i], 0);
    }
}

void init(void) {
    windows = malloc(MAX_WINDOWS * sizeof(window_t *));
    windows[0] = NULL;

    main_bg = open_bg_bmp("/zada/vitrail/bg.bmp");
    blurred_bg = open_bg_bmp("/zada/vitrail/bg_blur.bmp");

    draw_bg(0, 0, 1024, 768);

    window_t *window = create_window(256, 256, 1);

    for (uint32_t y = 0; y < window->size_y / 2; y++) {
        for (uint32_t x = 0; x < window->size_x / 2; x++) {
            window->pixels[y * window->size_x + x] = 0x10000000;
        }
    }
    for (uint32_t y = 0; y < window->size_y / 2; y++) {
        for (uint32_t x = window->size_x / 2; x < window->size_x; x++) {
            window->pixels[y * window->size_x + x] = 0x1000FF00;
        }
    }
    for (uint32_t y = window->size_y / 2; y < window->size_y; y++) {
        for (uint32_t x = 0; x < window->size_x / 2; x++) {
            window->pixels[y * window->size_x + x] = 0x10FF0000;
        }
    }
    for (uint32_t y = window->size_y / 2; y < window->size_y; y++) {
        for (uint32_t x = window->size_x / 2; x < window->size_x; x++) {
            window->pixels[y * window->size_x + x] = 0x10FFFF00;
        }
    }

    refresh_window(window);
}
