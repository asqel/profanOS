#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>

#define VITRAIL_C
#include <vitrail.h>

#define OUTLINE_COLOR 0xAAAAAA
uint32_t *main_bg;
uint32_t *blurred_bg;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

void init(void);
int main(void) {
    init();
    return 0;
}

void draw_window_border(window_t *window, int errase);
void refresh_window(window_t *window);

void draw_bg(uint32_t x, uint32_t y, uint32_t size_x, uint32_t size_y) {
    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t y_ = 0; y_ < size_y; y_++) {
        for (uint32_t x_ = 0; x_ < size_x; x_++) {
            fb[(y + y_) * pitch + x + x_] = main_bg[y + y_ * 1024 + x_];
        }
    }
}

window_t *create_window(int pos_x, int pos_y, int size_x, int size_y) {
    window_t *window = malloc(sizeof(window_t));
    window->pos_x = pos_x;
    window->pos_y = pos_y;
    window->size_x = size_x;
    window->size_y = size_y;
    window->pixels = calloc(size_x * size_y, sizeof(uint32_t));
    window->vesa_fb = c_vesa_get_fb();
    window->vesa_pitch = c_vesa_get_pitch();
    window->bg = blurred_bg;
    draw_window_border(window, 0);
    refresh_window(window);
    return window;
}

void destroy_window(window_t *window) {
    draw_bg(window->pos_x, window->pos_y, window->size_x, window->size_y);
    free(window->pixels);
    free(window);
}

void refresh_window(window_t *window) {
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
            bg_color = window->bg[(window->pos_y + y) * 1024 + window->pos_x + x];
            fb[(window->pos_y + y) * pitch + window->pos_x + x] = 
                ((color & 0xFF) * alpha + (bg_color & 0xFF) * (255 - alpha)) / 255 |
                (((color >> 8) & 0xFF) * alpha + ((bg_color >> 8) & 0xFF) * (255 - alpha)) / 255 << 8 |
                (((color >> 16) & 0xFF) * alpha + ((bg_color >> 16) & 0xFF) * (255 - alpha)) / 255 << 16;
        }
    }
}

void move_window(window_t *window, uint32_t pos_x, uint32_t pos_y) {
    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t y = window->pos_y; y < window->pos_y + window->size_y; y++) {
        for (uint32_t x = window->pos_x; x < window->pos_x + window->size_x; x++) {
            if (x < pos_x || x >= pos_x + window->size_x || y < pos_y || y >= pos_y + window->size_y) {
                fb[y * pitch + x] = main_bg[y * 1024 + x];
            }
        }
    }

    draw_window_border(window, 1);
    window->pos_x = pos_x;
    window->pos_y = pos_y;
    refresh_window(window);
    draw_window_border(window, 0);
}

void draw_window_border(window_t *window, int errase) {
    uint32_t *fb = c_vesa_get_fb();
    uint32_t pitch = c_vesa_get_pitch();

    for (uint32_t x = 0; x < window->size_x; x++) {
        fb[((window->pos_y - 2) * pitch) + window->pos_x + x] = errase ? main_bg[(window->pos_y - 2) * 1024 + window->pos_x + x] : OUTLINE_COLOR;
        fb[(window->pos_y + window->size_y + 1) * pitch + window->pos_x + x] = errase ? main_bg[(window->pos_y + window->size_y + 1) * 1024 + window->pos_x + x] : OUTLINE_COLOR;

        fb[((window->pos_y - 1) * pitch) + window->pos_x + x] = errase ? main_bg[(window->pos_y - 1) * 1024 + window->pos_x + x] : OUTLINE_COLOR;
        fb[(window->pos_y + window->size_y) * pitch + window->pos_x + x] = errase ? main_bg[(window->pos_y + window->size_y) * 1024 + window->pos_x + x] : OUTLINE_COLOR;
    }

    for (uint32_t y = 0; y < window->size_y; y++) {
        fb[(window->pos_y + y) * pitch + window->pos_x - 2] = errase ? main_bg[(window->pos_y + y) * 1024 + window->pos_x - 2] : OUTLINE_COLOR;
        fb[(window->pos_y + y) * pitch + window->pos_x + window->size_x + 1] = errase ? main_bg[(window->pos_y + y) * 1024 + window->pos_x + window->size_x + 1] : OUTLINE_COLOR;

        fb[(window->pos_y + y) * pitch + window->pos_x - 1] = errase ? main_bg[(window->pos_y + y) * 1024 + window->pos_x - 1] : OUTLINE_COLOR;
        fb[(window->pos_y + y) * pitch + window->pos_x + window->size_x] = errase ? main_bg[(window->pos_y + y) * 1024 + window->pos_x + window->size_x] : OUTLINE_COLOR;
    }

    fb[(window->pos_y - 1) * pitch + window->pos_x - 1] = errase ? main_bg[(window->pos_y - 1) * 1024 + window->pos_x - 1] : OUTLINE_COLOR;
    fb[(window->pos_y - 1) * pitch + window->pos_x + window->size_x] = errase ? main_bg[(window->pos_y - 1) * 1024 + window->pos_x + window->size_x] : OUTLINE_COLOR;
    fb[(window->pos_y + window->size_y) * pitch + window->pos_x - 1] = errase ? main_bg[(window->pos_y + window->size_y) * 1024 + window->pos_x - 1] : OUTLINE_COLOR;
    fb[(window->pos_y + window->size_y) * pitch + window->pos_x + window->size_x] = errase ? main_bg[(window->pos_y + window->size_y) * 1024 + window->pos_x + window->size_x] : OUTLINE_COLOR;
}


uint32_t *open_bmp(char *path) {
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

    if (width <= 0 || height <= 0) {
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

void init(void) {
    main_bg = open_bmp("/user/win.bmp");
    blurred_bg = open_bmp("/user/win_blur.bmp");

    draw_bg(0, 0, 1024, 768);

    window_t *window = create_window(700, 300, 256, 256);

    for (uint32_t y = 0; y < window->size_y / 2; y++) {
        for (uint32_t x = 0; x < window->size_x / 2; x++) {
            window->pixels[y * window->size_x + x] = 0x100000FF;
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

    // move_window(window, 600, 50);
}
