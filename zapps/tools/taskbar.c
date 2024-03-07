#include <syscall.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <vitrail.h>

void buffer_print(uint32_t *pixel_buffer, int x, int y, char *msg) {
    unsigned char *glyph;
    for (int i = 0; msg[i] != '\0'; i++) {
        glyph = c_font_get(0) + msg[i] * 16;
        for (int j = 0; j < 16; j++) {
            for (int k = 0; k < 8; k++) {
                if (!(glyph[j] & (1 << k))) continue;
                pixel_buffer[i * 8 + x + 8 - k + 1024 * (y + j)] = 0xffffffff;
            }
        }
    }
}

int main(void) {
    window_t *window = window_create(1024, 20, 0);
    char *buffer = malloc(1024);

    // wake up the parent process
    c_process_wakeup(c_process_get_ppid(c_process_get_pid()));

    int cpu, last_idle, last_total;
    int idle = 0;
    int total = 0;

    while (1) {
        for (int i = 0; i < 1024 * 20; i++) {
            window->pixels[i] = 0xff666666;
        }

        last_idle = idle;
        last_total = total;

        idle = c_process_get_run_time(1);
        total = c_timer_get_ms();

        cpu = total - last_total;
        cpu = 100 - (idle - last_idle) * 100 / (cpu ? cpu : 1);

        sprintf(buffer, "CPU: %d%% | MEM: %dKB | ALLOC: %d", cpu, c_mem_get_info(6, 0) / 1024, c_mem_get_info(4, 0) - c_mem_get_info(5, 0));
        buffer_print(window->pixels, 0, 5, buffer);

        window_refresh(window);

        c_process_sleep(c_process_get_pid(), 200);
    }
}
