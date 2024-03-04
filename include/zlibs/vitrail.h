#ifndef VITRAIL_ID
#define VITRAIL_ID 1001

#include <type.h>

typedef struct {
    uint32_t pos_x;
    uint32_t pos_y;

    uint32_t size_x;
    uint32_t size_y;

    uint32_t *pixels;

    uint32_t *vesa_fb;
    uint32_t vesa_pitch;

    uint32_t *bg;
} window_t;

#define windows_set_pixel_alpha(window, x, y, color) do { \
    (window)->pixels[(y) * (window)->size_x + (x)] = color;\
    uint32_t alpha = ((color) >> 24) & 0xFF;\
    uint32_t bg_color = (window)->bg[((window)->pos_y + (y)) * 1024 + (window)->pos_x + (x)];\
    (window)->vesa_fb[((window)->pos_y + (y)) * (window)->vesa_pitch + (window)->pos_x + (x)] =\
    (((color) & 0xFF) * alpha + (bg_color & 0xFF) * (255 - alpha)) / 255 |\
    ((((color) >> 8) & 0xFF) * alpha + ((bg_color >> 8) & 0xFF) * (255 - alpha)) / 255 << 8 |\
    ((((color) >> 16) & 0xFF) * alpha + ((bg_color >> 16) & 0xFF) * (255 - alpha)) / 255 << 16;\
} while (0)

#define windows_set_pixel(window, x, y, color) do { \
    (window)->pixels[(y) * (window)->size_x + (x)] = color;\
    (window)->vesa_fb[((window)->pos_y + (y)) * (window)->vesa_pitch + (window)->pos_x + (x)] = color;\
} while (0)

#ifndef VITRAIL_C
#define get_func_addr ((uint32_t (*)(uint32_t, uint32_t)) *(uint32_t *) 0x1ffffb)

#define create_window ((window_t *(*)(uint32_t, uint32_t, uint32_t, uint32_t)) (get_func_addr(VITRAIL_ID, 3)))
#define destroy_window ((void (*)(window_t *)) (get_func_addr(VITRAIL_ID, 4)))
#define refresh_window ((void (*)(window_t *)) (get_func_addr(VITRAIL_ID, 5)))

#endif
#endif
