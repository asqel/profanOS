/*****************************************************************************\
|   === panda.c : 2024 ===                                                    |
|                                                                             |
|    Terminal emulator as kernel module with psf font              .pi0iq.    |
|                                                                 d"  . `'b   |
|    This file is part of profanOS and is released under          q. /|\  "   |
|    the terms of the GNU General Public License                   `// \\     |
|                                                                  //   \\    |
|   === elydre : https://github.com/elydre/profanOS ===         #######  \\   |
\*****************************************************************************/

#include <profan/syscall.h>
#include <profan/filesys.h>
#include <profan/libmmq.h>

#define SCROLL_LINES 8

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t charcount;
    uint32_t charsize;

    uint8_t *data;
} font_data_t;

typedef struct {
    char content;
    char color;
} screen_char_t;

typedef struct {
    int cursor_x;
    int cursor_y;
    int scroll_offset;

    int max_lines;
    int max_cols;

    int saved_cursor_x;
    int saved_cursor_y;

    uint8_t cursor_is_hidden;
    uint8_t color;

    uint32_t *fb;
    uint32_t pitch;

    uint32_t xfrom;
    uint32_t yfrom;
    uint32_t xto;
    uint32_t yto;

    screen_char_t *screen_buffer;

    font_data_t *font;
} panda_global_t;

panda_global_t **g_panda;
int g_panda_count;

#define set_pixel(index, x, y, color) \
    g_panda[index]->fb[(x) + (y) * g_panda[index]->pitch] = color

void init_panda();

int main(void) {
    init_panda();
    return 0;
}

font_data_t *load_psf_font(char *file) {
    sid_t sid = fu_path_to_sid(ROOT_SID, file);
    if (IS_NULL_SID(sid)) return NULL;

    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t charcount;
    uint32_t charsize;
    uint32_t height;
    uint32_t width;

    fu_file_read(sid, &magic, 0, 4);
    fu_file_read(sid, &version, 4, 4);
    fu_file_read(sid, &headersize, 8, 4);
    fu_file_read(sid, &charcount, 16, 4);
    fu_file_read(sid, &charsize, 20, 4);
    fu_file_read(sid, &height, 24, 4);
    fu_file_read(sid, &width, 28, 4);

    if (magic != 0x864ab572 || version != 0)
        return NULL;

    uint8_t *font = malloc_ask(charcount * charsize);
    fu_file_read(sid, font, headersize, charcount * charsize);

    font_data_t *psf = malloc_ask(sizeof(font_data_t));
    psf->width = width;
    psf->height = height;
    psf->charcount = charcount;
    psf->charsize = charsize;
    psf->data = font;

    return psf;
}

void free_font(font_data_t *pff) {
    free(pff->data);
    free(pff);
}

uint32_t compute_color(uint8_t color) {
    uint32_t rgb[] = {
        0x222222, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA8800, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    };

    if (color > 0xF) return 0xFFFFFF;
    return rgb[(int) color];
}

char compute_ansi_color(char ansi_nb, int part, char old_color) {
    char fg = old_color & 0xF;
    char bg = (old_color >> 4) & 0xF;

    switch (ansi_nb) {
        case '0': ansi_nb = 0; break;
        case '1': ansi_nb = 4; break;
        case '2': ansi_nb = 2; break;
        case '3': ansi_nb = 6; break;
        case '4': ansi_nb = 1; break;
        case '5': ansi_nb = 5; break;
        case '6': ansi_nb = 3; break;
        default:  ansi_nb = 7; break;
    }

    if (part == 0) {
        fg = ansi_nb;
    } else if (part == 1) {
        fg = ansi_nb + 8;
    } else if (part == 2) {
        bg = ansi_nb;
    }

    return (bg << 4) | fg;
}

void print_char(uint32_t index, uint32_t xo, uint32_t yo, uint8_t c, uint8_t color_code) {
    uint32_t bg_color = compute_color((color_code >> 4) & 0xF);
    uint32_t fg_color = compute_color(color_code & 0xF);

    uint8_t *char_data = g_panda[index]->font->data + (c * g_panda[index]->font->charsize);

    uint32_t x = 0;
    uint32_t y = 0;
    for (uint32_t i = 0; i < g_panda[index]->font->charsize; i++) {
        if (x >= g_panda[index]->font->width) {
            x = 0;
            y++;
        }
        for (int j = 7; j >= 0; j--) {
            set_pixel(index, xo + x, yo + y, char_data[i] & (1 << j) ? fg_color : bg_color);
            if (x >= g_panda[index]->font->width) break;
            x++;
        }
    }
}

void set_char(uint32_t index, uint32_t x, uint32_t y, char c, char color) {
    uint32_t offset = y * g_panda[index]->max_cols + x;
    if (g_panda[index]->screen_buffer[offset].content == c && g_panda[index]->screen_buffer[offset].color == color) return;
    g_panda[index]->screen_buffer[offset].content = c;
    g_panda[index]->screen_buffer[offset].color = color;
    print_char(index, x * g_panda[index]->font->width + g_panda[index]->xfrom, y * g_panda[index]->font->height + g_panda[index]->yfrom, c, color);
}

void panda_clear_screen(uint32_t index) {
    if (!g_panda) return;
    for (int i = 0; i < g_panda[index]->max_lines; i++) {
        for (int j = 0; j < g_panda[index]->max_cols; j++) {
            set_char(index, j, i, ' ', 0xF);
        }
    }
    g_panda[index]->cursor_x = 0;
    g_panda[index]->cursor_y = 0;
    g_panda[index]->scroll_offset = 0;
}

int compute_ansi_escape(uint32_t index, char *str) {
    char *start = str;

    if (str[1] == '[') str += 2;
    else return 1;

    // cursor save and restore
    if (str[0] == 's') {
        g_panda[index]->saved_cursor_x = g_panda[index]->cursor_x;
        g_panda[index]->saved_cursor_y = g_panda[index]->cursor_y;
    } else if (str[0] == 'u') {
        if (g_panda[index]->saved_cursor_y < g_panda[index]->scroll_offset) {
            g_panda[index]->saved_cursor_y = g_panda[index]->scroll_offset;
        } else {
            g_panda[index]->cursor_y = g_panda[index]->saved_cursor_y;
        }
        g_panda[index]->cursor_x = g_panda[index]->saved_cursor_x;
    } else if (str[0] == 'K') {
        for (int i = g_panda[index]->cursor_x; i < g_panda[index]->max_cols; i++) {
            set_char(index, i, g_panda[index]->cursor_y - g_panda[index]->scroll_offset, ' ', g_panda[index]->color);
        }
    }

    // font color
    if (str[0] == '3' && str[2] == 'm') {
        g_panda[index]->color = compute_ansi_color(str[1], 0, g_panda[index]->color);
        return 4;
    }

    // highlight font color
    if (str[0] == '9' && str[2] == 'm') {
        g_panda[index]->color = compute_ansi_color(str[1], 1, g_panda[index]->color);
        return 4;
    }

    // background color
    if (str[0] == '4' && str[2] == 'm') {
        g_panda[index]->color = compute_ansi_color(str[1], 2, g_panda[index]->color);
        return 4;
    }

    // reset color
    if (str[0] == '0' && str[1] == 'm') {
        g_panda[index]->color = 0xF;
        return 3;
    }

    // cursor hide and show
    if (strncmp(str, "?25", 3) == 0) {
        if (str[3] == 'l') {
            g_panda[index]->cursor_is_hidden = 0;
        } else if (str[3] == 'h') {
            g_panda[index]->cursor_is_hidden = 1;
        }
        return 5;
    }

    // clear screen
    if (str[0] == '2' && str[1] == 'J') {
        panda_clear_screen(index);
        return 3;
    }

    // set top left
    if (str[0] == 'H') {
        g_panda[index]->cursor_x = 0;
        g_panda[index]->cursor_y = 0;
        g_panda[index]->scroll_offset = 0;
        return 2;
    }

    // number
    char *tmp = str;
    while (*tmp >= '0' && *tmp <= '9') tmp++;

    // cursor up
    if (tmp[0] == 'A') {
        int n = atoi(str);
        g_panda[index]->cursor_y -= n;
    }

    // cursor down
    if (tmp[0] == 'B') {
        int n = atoi(str);
        g_panda[index]->cursor_y += n;
    }

    // cursor forward
    if (tmp[0] == 'C') {
        int n = atoi(str);
        g_panda[index]->cursor_x += n;
        if (g_panda[index]->cursor_x >= g_panda[index]->max_cols) {
            g_panda[index]->cursor_x -= g_panda[index]->max_cols;
            g_panda[index]->cursor_y++;
        }
    }

    // cursor backward
    if (tmp[0] == 'D') {
        int n = atoi(str);
        g_panda[index]->cursor_x -= n;
        if (g_panda[index]->cursor_x < 0) {
            g_panda[index]->cursor_x += g_panda[index]->max_cols;
            g_panda[index]->cursor_y--;
        }
    }

    return tmp - start;
}

void panda_scroll(uint32_t index, uint32_t line_count) {
    int offset;

    g_panda[index]->cursor_x = 0;
    g_panda[index]->cursor_y++;

    if (g_panda[index]->cursor_y - g_panda[index]->scroll_offset < g_panda[index]->max_lines) {
        // fill new line with spaces
        for (int j = 0; j < g_panda[index]->max_cols; j++) {
            set_char(index, j, g_panda[index]->cursor_y - g_panda[index]->scroll_offset, ' ', 0xF);
        }
        return;
    }

    g_panda[index]->scroll_offset += line_count;

    // scroll the display and print it
    int new_offset;
    for (uint32_t i = 0; i < g_panda[index]->max_lines - line_count; i++) {
        for (int j = 0; j < g_panda[index]->max_cols; j++) {
            new_offset = i * g_panda[index]->max_cols + j;
            offset = new_offset + g_panda[index]->max_cols * line_count;
            if (g_panda[index]->screen_buffer[new_offset].content == g_panda[index]->screen_buffer[offset].content &&
                g_panda[index]->screen_buffer[new_offset].color == g_panda[index]->screen_buffer[offset].color
            ) continue;
            g_panda[index]->screen_buffer[new_offset].content = g_panda[index]->screen_buffer[offset].content;
            g_panda[index]->screen_buffer[new_offset].color = g_panda[index]->screen_buffer[offset].color;
            print_char(index, j * g_panda[index]->font->width + g_panda[index]->xfrom, i * g_panda[index]->font->height + g_panda[index]->yfrom,
                    g_panda[index]->screen_buffer[new_offset].content, g_panda[index]->screen_buffer[new_offset].color);
        }
    }

    // clear the last line
    for (uint32_t i = 0; i < line_count; i++) {
        for (int j = 0; j < g_panda[index]->max_cols; j++) {
            set_char(index, j, g_panda[index]->max_lines - 1 - i, ' ', 0xF);
        }
    }
}

void draw_cursor(uint32_t index, int errase) {
    uint32_t offset;
    if (!errase) {
        for (uint32_t i = 0; i < g_panda[index]->font->height; i++) {
            set_pixel(index, g_panda[index]->cursor_x * g_panda[index]->font->width + 1 + g_panda[index]->xfrom,
                    (g_panda[index]->cursor_y - g_panda[index]->scroll_offset) * g_panda[index]->font->height + i + g_panda[index]->yfrom, 0xFFFFFF);
        }
    } else {
        offset = (g_panda[index]->cursor_y - g_panda[index]->scroll_offset) * g_panda[index]->max_cols + g_panda[index]->cursor_x;
        print_char(index, g_panda[index]->cursor_x * g_panda[index]->font->width + g_panda[index]->xfrom,
                   (g_panda[index]->cursor_y - g_panda[index]->scroll_offset) * g_panda[index]->font->height + g_panda[index]->yfrom,
                   g_panda[index]->screen_buffer[offset].content,
                   g_panda[index]->screen_buffer[offset].color
        );
    }
}

uint8_t panda_print_string(uint32_t index, char *string, int len, int tmp_color) {
    if (!g_panda) return 0;
    int tmp, old_color;

    if (tmp_color != -1) {
        old_color = g_panda[index]->color;
        g_panda[index]->color = tmp_color;
    }

    for (int i = 0; (len < 0) ? (string[i]) : (i < len); i++) {
        if (!g_panda[index]->cursor_is_hidden)
            draw_cursor(index, 1);
        if (string[i] == '\n')
            panda_scroll(index, SCROLL_LINES);
        else if (string[i] == '\r')
            g_panda[index]->cursor_x = 0;
        else if (string[i] == '\t') {
            tmp = g_panda[index]->cursor_x + 4 - (g_panda[index]->cursor_x % 4);
            for (; g_panda[index]->cursor_x < tmp; g_panda[index]->cursor_x++)
                set_char(index, g_panda[index]->cursor_x, g_panda[index]->cursor_y - g_panda[index]->scroll_offset, ' ', g_panda[index]->color);
        } else if (string[i] == '\e')
            i += compute_ansi_escape(index, string + i);
        else {
            set_char(index, g_panda[index]->cursor_x, g_panda[index]->cursor_y - g_panda[index]->scroll_offset, string[i], g_panda[index]->color);
            g_panda[index]->cursor_x++;
        }
        if (g_panda[index]->cursor_x >= g_panda[index]->max_cols)
            panda_scroll(index, SCROLL_LINES);
        if (g_panda[index]->cursor_y - g_panda[index]->scroll_offset >= g_panda[index]->max_lines)
            panda_scroll(index, SCROLL_LINES);
        if (!g_panda[index]->cursor_is_hidden)
            draw_cursor(index, 0);
    }
    if (tmp_color != -1) {
        tmp_color = g_panda[index]->color;
        g_panda[index]->color = old_color;
    }
    return tmp_color;
}

#define offset_to_cursor_y(offset, max_cols) ((offset) / (2 * (max_cols)))

void panda_set_start(uint32_t index, int kernel_cursor) {
    if (!g_panda) return;
    uint32_t kmax_cols = c_vesa_get_width() / 8;

    g_panda[index]->cursor_x = 0;
    g_panda[index]->cursor_y = ((offset_to_cursor_y(kernel_cursor, kmax_cols) + 1) * 16) / g_panda[index]->font->height;
    g_panda[index]->scroll_offset = 0;
}

void panda_get_cursor(uint32_t index, uint32_t *x, uint32_t *y) {
    if (!g_panda) {
        *x = 0;
        *y = 0;
    } else {
        *x = g_panda[index]->cursor_x;
        *y = g_panda[index]->cursor_y;
    }
}

void panda_draw_cursor(uint32_t index, uint32_t x, uint32_t y) {
    if (!g_panda) return;
    draw_cursor(index, 1);
    g_panda[index]->cursor_x = x;
    g_panda[index]->cursor_y = y;
    draw_cursor(index, 0);
}

void panda_get_size(uint32_t index, uint32_t *x, uint32_t *y) {
    if (!g_panda) {
        *x = 0;
        *y = 0;
    } else {
        *x = g_panda[index]->max_cols;
        *y = g_panda[index]->max_lines;
    }
}

int panda_change_font(uint32_t index, char *file) {
    if (!g_panda) return 1;
    font_data_t *font = load_psf_font(file);
    if (font == NULL) return 1;

    panda_clear_screen(index);
    free_font(g_panda[index]->font);
    g_panda[index]->font = font;
    g_panda[index]->max_lines = (g_panda[index]->yto - g_panda[index]->yfrom) / g_panda[index]->font->height;
    g_panda[index]->max_cols = (g_panda[index]->xto - g_panda[index]->xfrom) / g_panda[index]->font->width;
    return 0;
}

int new_terminal(int xfrom, int yfrom, int xto, int yto) {
    int i;
    for (i = 0; i < g_panda_count; i++) {
        if (g_panda[i] == NULL) break;
    }
    if (i == g_panda_count) {
        g_panda = realloc_ask(g_panda, (g_panda_count + 1) * sizeof(panda_global_t *));
        g_panda_count++;
    }
    g_panda[i] = malloc_ask(sizeof(panda_global_t));    

    g_panda[i] = malloc_ask(sizeof(panda_global_t));
    g_panda[i]->font = load_psf_font("/zada/fonts/lat38-bold18.psf");

    if (g_panda[i]->font == NULL) {
        fd_printf(2, "\n Failed to load font\n");
    }

    g_panda[i]->cursor_x = 0;
    g_panda[i]->cursor_y = 0;

    g_panda[i]->xfrom = xfrom;
    g_panda[i]->yfrom = yfrom;
    g_panda[i]->xto = xto;
    g_panda[i]->yto = yto;

    g_panda[i]->saved_cursor_x = 0;
    g_panda[i]->saved_cursor_y = 0;

    g_panda[i]->scroll_offset = 0;

    g_panda[i]->cursor_is_hidden = 1;
    g_panda[i]->color = 0x0F;

    g_panda[i]->max_lines = (yto - yfrom) / g_panda[i]->font->height;
    g_panda[i]->max_cols = (xto - xfrom) / g_panda[i]->font->width;

    g_panda[i]->fb = c_vesa_get_fb();
    g_panda[i]->pitch = c_vesa_get_pitch();

    g_panda[i]->screen_buffer = calloc_ask(g_panda[i]->max_lines * g_panda[i]->max_cols, sizeof(screen_char_t));

    panda_clear_screen(i);
    return i;
}

void init_panda(void) {
    if (!c_vesa_does_enable()) {
        fd_printf(2, "[panda] VESA is not enabled\n");
        g_panda = NULL;
        return;
    }

    g_panda = calloc_ask(1, sizeof(panda_global_t *));
    g_panda_count = 1;

    new_terminal(0, 0, c_vesa_get_width() / 2, c_vesa_get_height());
    new_terminal(c_vesa_get_width() / 2, 0, c_vesa_get_width(), c_vesa_get_height());
}
