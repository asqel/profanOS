#include <i_libdaube.h>
#include <i_string.h>
#include <i_mouse.h>
#include <i_time.h>
#include <i_vgui.h>

#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_WINDOWS 10

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

void perf_demo();
void main_process();

int main(int argc, char **argv) {
    vgui_t vgui = vgui_setup(SCREEN_WIDTH, SCREEN_HEIGHT);

    desktop_t *desktop = desktop_init(&vgui, MAX_WINDOWS, SCREEN_WIDTH, SCREEN_HEIGHT);
    desktop_refresh(desktop);

    c_run_ifexist("/bin/winapps/cpu.bin", 0, NULL);
    c_run_ifexist("/bin/winapps/demo.bin", 0, NULL);
    c_run_ifexist("/bin/winapps/pong.bin", 0, NULL);
    c_run_ifexist("/bin/winapps/counter.bin", 0, NULL);
    c_run_ifexist("/bin/winapps/usage.bin", 0, NULL);
    c_run_ifexist("/bin/winapps/term.bin", 0, NULL);

    while (1) {
        refresh_mouse(desktop);
        ms_sleep(10);
    }

    return 0;
}
