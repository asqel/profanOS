#ifndef VITRAIL_ID
#define VITRAIL_ID 1001

#define VITRAIL_BG_SIDE 48

#include <type.h>

typedef struct {
    uint32_t pos_x;
    uint32_t pos_y;

    uint32_t size_x;
    uint32_t size_y;

    uint32_t *pixels;

    uint32_t *vesa_fb;
    uint32_t vesa_pitch;
    uint32_t vesa_width;

    int enable_border;

    uint32_t *bg;
} window_t;

#define windows_set_pixel_alpha(window, x, y, color) do { \
    (window)->pixels[(y) * (window)->size_x + (x)] = color;\
    uint32_t __alpha = ((color) >> 24) & 0xFF;\
    uint32_t __bg_color = (window)->bg[(((window)->pos_y + (y)) % VITRAIL_BG_SIDE) * VITRAIL_BG_SIDE + ((window)->pos_x + (x)) % VITRAIL_BG_SIDE];\
    (window)->vesa_fb[((window)->pos_y + (y)) * (window)->vesa_pitch + (window)->pos_x + (x)] =\
    (((color) & 0xFF) * __alpha + (__bg_color & 0xFF) * (255 - __alpha)) / 255 |\
    ((((color) >> 8) & 0xFF) * __alpha + ((__bg_color >> 8) & 0xFF) * (255 - __alpha)) / 255 << 8 |\
    ((((color) >> 16) & 0xFF) * __alpha + ((__bg_color >> 16) & 0xFF) * (255 - __alpha)) / 255 << 16;\
} while (0)

#define windows_set_pixel(window, x, y, color) do { \
    (window)->pixels[(y) * (window)->size_x + (x)] = color;\
    (window)->vesa_fb[((window)->pos_y + (y)) * (window)->vesa_pitch + (window)->pos_x + (x)] = color;\
} while (0)

#ifndef VITRAIL_C
#define get_func_addr ((uint32_t (*)(uint32_t, uint32_t)) *(uint32_t *) 0x1ffffb)

#define window_create ((window_t *(*)(uint32_t, uint32_t, int)) (get_func_addr(VITRAIL_ID, 3)))
#define window_destroy ((void (*)(window_t *)) (get_func_addr(VITRAIL_ID, 4)))
#define window_refresh ((void (*)(window_t *)) (get_func_addr(VITRAIL_ID, 5)))

#endif
#endif
