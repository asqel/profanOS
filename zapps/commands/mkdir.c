#include <syscall.h>
#include <string.h>
#include <iolib.h>

int main(int argc, char **argv) {
    char *suffix = c_malloc(256);
    str_cpy(suffix, argv[2]);
    char *current_dir = c_malloc(256);
    str_cpy(current_dir, argv[1]);
    if (!str_cmp(suffix, "..")) {
        fsprint("$3Un dossier ne peut pas avoir comme nom .. !\n");
    } else {
        c_fs_make_dir(current_dir, suffix);
    }
    c_free(current_dir);
    c_free(suffix);
    return 0;
}
