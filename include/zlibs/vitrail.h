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
} window_t;

#define instant_pixel(window, x, y, color) \
    window->pixels[(y) * window->size_x + (x)] = color;\
    window->vesa_fb[(window->pos_y + (y)) * window->vesa_pitch + window->pos_x + (x)] = color;

#ifndef VITRAIL_C
#define get_func_addr ((uint32_t (*)(uint32_t, uint32_t)) *(uint32_t *) 0x1ffffb)

#define create_window ((window_t *(*)(uint32_t, uint32_t, uint32_t, uint32_t)) (get_func_addr(VITRAIL_ID, 3)))
#define destroy_window ((void (*)(window_t *)) (get_func_addr(VITRAIL_ID, 4)))
#define refresh_window ((void (*)(window_t *)) (get_func_addr(VITRAIL_ID, 5)))

#endif
#endif
