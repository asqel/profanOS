#include <kernel/ramdisk.h>
#include <gui/gnrtx.h>
#include <function.h>
#include <minilib.h>
#include <system.h>
#include <mem.h>

#include <iolib.h>
#include <lib.h>

#define BFR_SIZE 65

int shell_command(char command[]);

void start_kshell() {
    sys_warning("You are now in the kernel-level shell");
    kprint("\n");
    char char_buffer[BFR_SIZE];
    while (1) {
        rainbow_print("kernel-shell> ");
        input(char_buffer, BFR_SIZE, 9);
        kprint("\n");
        if (shell_command(char_buffer)) break;
        char_buffer[0] = '\0';
    }
    clear_screen();
}


void shell_so(char suffix[]) {
    char path[100] = "/bin/";
    str_cat(path, suffix);
    str_cat(path, ".bin");
    kprintf("path: %s\n", path);
    run_ifexist(path, 0, (char **)0);    
}

void shell_help() {
    char *help[] = {
        "ADDR   - show main address",
        "ALLOC  - allocate *0x1000",
        "EXIT   - quit the kshell",
        "GO     - go file as binary",
        "HELP   - show this help",
        "MEM    - show memory state",
        "REBOOT - reboot the system",
        "SO     - run file in /bin",
    };

    for (int i = 0; i < ARYLEN(help); i++)
        kprintf("%s\n", help[i]);
}

void shell_addr() {
    fsprint("physic:  %x (%fMo)\n", mem_get_phys_size(), mem_get_phys_size() / 1024.0 / 1024.0);
    fsprint("ramdisk: %x (%fMo)\n", ramdisk_get_address(), ramdisk_get_size() / 2048.0);
    fsprint("b3 mm:   %x\n", mem_get_base_addr());
    fsprint("watfunc: %x\n", WATFUNC_ADDR);
    fsprint("rand sv: %x\n", RAND_SAVE);
}

void test_smart_switch() {
    dily_load("/lib/lib_v1.bin", 123);  // we load the lib_v1 whis ID 123
    lib_print_version();                // we call the lib function
    dily_unload(123);                   // we unload the lib whis ID 123
    
    dily_load("/lib/lib_v2.bin", 123);  // we load the lib_v2 whis ID 123
    lib_print_version();                // we call the lib function
    dily_unload(123);                   // we unload the lib whis ID 123
}

int shell_command(char command[]) {
    char prefix[BFR_SIZE], suffix[BFR_SIZE];
    int part = 0, i = 0;

    for (i = 0; i < str_len(command); i++) {
        if (command[i] == ' ') {
            prefix[i] = '\0';
            part = 1;
        }
        else if (part == 0) prefix[i] = command[i];
        else if (part == 1) suffix[i - str_len(prefix) - 1] = command[i];
    }
    if (part == 0) prefix[i] = '\0';
    else suffix[i - str_len(prefix) - 1] = '\0';

    if      (str_cmp(prefix, "addr") == 0) shell_addr();
    else if (str_cmp(prefix, "alloc") == 0) mem_alloc(str2int(suffix) * 0x1000);
    else if (str_cmp(prefix, "exit") == 0) return 1;
    else if (str_cmp(prefix, "go") == 0) run_ifexist(suffix, 0, (char **)0);
    else if (str_cmp(prefix, "help") == 0) shell_help();
    else if (str_cmp(prefix, "mem") == 0) mem_print();
    else if (str_cmp(prefix, "reboot") == 0) sys_reboot();
    else if (str_cmp(prefix, "so") == 0) shell_so(suffix);
    else if (str_cmp(prefix, "tss") == 0) test_smart_switch();
    else if (prefix[0] != '\0') kprintf("not found: %s\n", prefix);

    return 0;
}
