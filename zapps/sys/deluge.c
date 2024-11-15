/*****************************************************************************\
|   === deluge.c : 2024 ===                                                   |
|                                                                             |
|    profanOS dynamic linker, load and run ELF                     .pi0iq.    |
|                                                                 d"  . `'b   |
|    This file is part of profanOS and is released under          q. /|\  "   |
|    the terms of the GNU General Public License                   `// \\     |
|                                                                  //   \\    |
|   === elydre : https://github.com/elydre/profanOS ===         #######  \\   |
\*****************************************************************************/

#define _SYSCALL_CREATE_FUNCS

#include <profan/syscall.h>
#include <profan/filesys.h>
#include <profan/libmmq.h>
#include <profan/dlgext.h>

#include <dlfcn.h>

#define DELUGE_VERSION "3.1"
#define ALWAYS_DEBUG 0

/****************************
 *                         *
 *    Types and globals    *
 *                         *
****************************/

char *profan_fn_name(void *ptr, char **libname);

#define raise_error(fmt, ...) do {  \
            fd_printf(2, "DELUGE FATAL: "fmt"\n", ##__VA_ARGS__); \
            exit(1);  \
        } while (0)

#define debug_printf(lvl, fmt, ...) if (g_print_debug >= lvl) {  \
            fd_putstr(2, "\e[37m[DELUGE] ");  \
            fd_printf(2, fmt, __VA_ARGS__);  \
            fd_putstr(2, "\e[0m\n");  \
        }

// internal variables
elfobj_t **g_loaded_libs;
elfobj_t *g_prog;

int g_already_fini;
int g_lib_count;
char **g_envp;

// command line options
char *g_extralib_path;
int   g_dlfcn_error;
int   g_print_debug;

// extra symbols structure
typedef struct {
    const char *name;
    uint32_t hash;
    void *data;
} dlg_extra_t;

// extra symbols table
dlg_extra_t g_extra_syms[] = {
    { "profan_fn_name", 0x62289205, profan_fn_name },
    { "dlclose"       , 0xDE67CAC5, dlclose        },
    { "dlopen"        , 0xCEF94D0E, dlopen         },
    { "dlsym"         , 0x0677DB8D, dlsym          },
    { "dlerror"       , 0xDE8AD652, dlerror        },
    { NULL, 0, NULL }
};

/*************************
 *                      *
 *    Misc functions    *
 *                      *
*************************/

void add_loaded_lib(elfobj_t *lib) {
    g_loaded_libs = realloc(g_loaded_libs, ++g_lib_count * sizeof(elfobj_t *));
    g_loaded_libs[g_lib_count - 1] = lib;
}

char *ft_getenv(const char *name) {
    uint32_t len = strlen(name);
    for (int i = 0; g_envp[i]; i++) {
        if (strncmp(g_envp[i], name, len) == 0 && g_envp[i][len] == '=')
            return g_envp[i] + len + 1;
    }
    return NULL;
}

char *assemble_path(const char *dir, const char *file) {
    int len1 = strlen(dir);
    char *path = malloc(len1 + strlen(file) + 2);
    strcpy(path, dir);
    path[len1] = '/';
    strcpy(path + len1 + 1, file);
    return path;
}

/**************************
 *                       *
 *    Find file funcs    *
 *                       *
**************************/

uint32_t search_inpath(const char *src_path, const char *filename, char **fullpath) {
    char *path = strdup(src_path);

    char *fullname = malloc(strlen(filename) + 5); // 5 => .elf + null
    strcpy(fullname, filename);
    strcat(fullname, ".elf");

    int start = 0;
    for (int i = 0;; i++) {
        if (path[i] != ':' && path[i] != '\0')
            continue;
        path[i] = '\0';
        uint32_t sid = fu_path_to_sid(ROOT_SID, path + start);
        if (!IS_SID_NULL(sid)) {
            sid = fu_path_to_sid(sid, fullname);
            if (fu_is_file(sid)) {
                if (path)
                    *fullpath = assemble_path(path + start, fullname);
                free(fullname);
                free(path);
                return sid;
            }
        }
        if (src_path[i] == '\0')
            break;
        start = i + 1;
    }

    free(fullname);
    free(path);
    return SID_NULL;
}

uint32_t search_elf_sid(const char *name, uint16_t type, char **path) {
    uint32_t sid;

    if (name == NULL)
        return SID_NULL;

    if (type == ET_EXEC) {
        if (name[0] == '/') {
            sid = fu_path_to_sid(ROOT_SID, name);
            if (!IS_SID_NULL(sid) && path)
                *path = strdup(name);
            return sid;
        }

        if (name[0] == '.' && name[1] == '/') {
            char *cwd = ft_getenv("PWD");
            if (!cwd)
                return SID_NULL;
            char *full_path = assemble_path(cwd, name + 2);
            fu_simplify_path(full_path);
            sid = fu_path_to_sid(ROOT_SID, full_path);
            if (!IS_SID_NULL(sid) && path)
                *path = full_path;
            else
                free(full_path);
            return sid;
        }

        char *env_path = ft_getenv("PATH");
        if (!env_path)
            return SID_NULL;
        return search_inpath(env_path, name, path);
    }

    char *full_path = assemble_path("/lib", name);
    sid = fu_path_to_sid(ROOT_SID, full_path);

    if (IS_SID_NULL(sid)) {
        free(full_path);
        if (!g_extralib_path)
            return SID_NULL;
        full_path = assemble_path(g_extralib_path, name);
        sid = fu_path_to_sid(ROOT_SID, full_path);
        if (IS_SID_NULL(sid))
            free(full_path);
        else if (path)
            *path = full_path;
    } else if (path) {
        *path = full_path;
    }

    return sid;
}

/***************************
 *                        *
 *    Hash table funcs    *
 *                        *
***************************/

uint32_t hash(const char *str) {
    uint32_t hash = 0;
    for (int i = 0; str[i]; i++) {
        hash = (hash << 5) + str[i];
    }
    return hash;
}

dlg_hash_t *hash_create(elfobj_t *obj) {
    uint32_t size = obj->dym_size / sizeof(Elf32_Sym);

    dlg_hash_t *table = calloc(size, sizeof(dlg_hash_t));
    dlg_hash_t *later = calloc(size, sizeof(dlg_hash_t));
    int later_index = 0;

    for (uint32_t i = 0; i < size; i++) {
        const char *key = obj->dym_str + obj->dym_tab[i].st_name;
        uint32_t full_h = hash(key);
        uint32_t h = full_h % size;

        if (!table[h].data) {
            table[h].data = obj->dym_tab + i;
            table[h].key = key;
            table[h].hash = full_h;
        } else {
            later[later_index].data = obj->dym_tab + i;
            later[later_index].key = key;
            later[later_index].hash = full_h;
            later_index++;
        }
    }

    uint32_t table_index = 0;
    for (int i = 0; i < later_index; i++) {
        uint32_t h = later[i].hash % size;
        dlg_hash_t *entry = &table[h];

        while (table[table_index].data) {
            if (++table_index == size) {
                raise_error("Internal error: hash table is full");
            }
        }

        table[table_index] = later[i];

        while (entry->next)
            entry = (void *) entry->next;
        entry->next = (void *) &table[table_index];

        table_index++;
    }
    free(later);

    return table;
}

Elf32_Sym *hash_get(elfobj_t *obj, uint32_t full_h, const char *key) {
    dlg_hash_t *entry = obj->hash_table + full_h % (obj->dym_size / sizeof(Elf32_Sym));;

    while (entry) {
        if (entry->hash == full_h && strcmp(entry->key, key) == 0)
            return entry->data;
        entry = entry->next;
    }
    return NULL;
}

/************************
 *                     *
 *    ELF functions    *
 *                     *
************************/

int is_valid_elf(void *data, uint16_t required_type) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)data;
    return !(
        memcmp(ehdr->e_ident, (void *) ELFMAG, SELFMAG) != 0 ||
        ehdr->e_type != required_type ||
        ehdr->e_machine != EM_386
    );
}

char **get_required_libs(elfobj_t *obj) {
    if (obj->dynamic == NULL) {
        return NULL;
    }

    char **libs = NULL;
    int lib_count = 0;
    int max_libs = 0;

    if (obj->dynamic == NULL) {
        raise_error("no dynamic section found in '%s'", obj->name);
        return NULL;
    }

    for (int i = 0; obj->dynamic[i].d_tag != 0; i++) {
        if (obj->dynamic[i].d_tag != 1) // DT_NEEDED
            continue;
        if (lib_count >= max_libs) {
            max_libs += 16;
            libs = realloc(libs, max_libs * sizeof(char *));
        }
        libs[lib_count++] = (char *) obj->dym_str + obj->dynamic[i].d_un.d_val;
    }

    if (lib_count == 0)
        return NULL;

    libs[lib_count] = NULL;
    return libs;
}

void *get_base_addr(uint8_t *data, uint16_t type) {
    if (type != ET_EXEC)
        return 0;

    // find the lowest address of a PT_LOAD segment
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)data;
    Elf32_Phdr *phdr = (Elf32_Phdr *)(data + ehdr->e_phoff);

    uint32_t base_addr = 0xFFFFFFFF;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == 1 && phdr[i].p_vaddr < base_addr)
            base_addr = phdr[i].p_vaddr;
    }

    return (void *) base_addr;
}

int load_sections(elfobj_t *obj, uint16_t type) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)obj->file;
    Elf32_Shdr *shdr = (Elf32_Shdr *)(obj->file + ehdr->e_shoff);

    debug_printf(2, "| Load '%s'", obj->name);

    void *base_addr = get_base_addr(obj->file, type);
    uint32_t required_size = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_addr + shdr[i].sh_size > required_size)
            required_size = shdr[i].sh_addr + shdr[i].sh_size;
    }

    if (type == ET_EXEC)
        required_size -= (uint32_t) base_addr;
    required_size = (required_size + 0xFFF) & ~0xFFF;

    if (type == ET_EXEC) {
        obj->mem = (void *) base_addr;
        syscall_scuba_generate(base_addr, required_size / 0x1000);
    } else {
        obj->mem = (void *) syscall_mem_alloc(required_size, 0x1000, 1);
    }
    memset(obj->mem, 0, required_size);

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_PROGBITS && shdr[i].sh_addr) {
            if (type == ET_EXEC)
                memcpy((void *) shdr[i].sh_addr, obj->file + shdr[i].sh_offset, shdr[i].sh_size);
            else
                memcpy(obj->mem + shdr[i].sh_addr, obj->file + shdr[i].sh_offset, shdr[i].sh_size);
        }
    }

    return 0;
}

#define R_386_NONE      0   // None
#define R_386_32        1   // word32  S + A
#define R_386_PC32      2   // word32  S + A - P
#define R_386_GOT32     3   // word32  G + A
#define R_386_PLT32     4   // word32  L + A - P
#define R_386_COPY      5   // None
#define R_386_GLOB_DAT  6   // word32  S
#define R_386_JMP_SLOT  7   // word32  S
#define R_386_RELATIVE  8   // word32  B + A
#define R_386_GOTOFF    9   // word32  S + A - GOT
#define R_386_GOTPC     10  // word32  GOT + A - P
#define R_386_32PLT     11  // word32  L + A

// S: value of the symbol
// A: addend
// P: place of the storage unit being relocated
// B: base address of the shared object
// GOT: address of the global offset table
// L: address of the procedure linkage table

int does_type_required_sym(uint8_t type) {
    switch (type) {
        case R_386_32:
        case R_386_PC32:
        case R_386_COPY:
        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT:
        case R_386_GOTOFF:
            return 1;
        default:
            return 0;
    }
}

uint32_t get_sym_extra(const char *name, uint32_t full_h) {
    for (int i = 0; g_extra_syms[i].name; i++) {
        if (g_extra_syms[i].hash == full_h && strcmp(g_extra_syms[i].name, name) == 0)
            return (uint32_t) g_extra_syms[i].data;
    }
    return 0;
}

uint32_t get_sym_value(const char *name, Elf32_Sym **sym_ptr) {
    Elf32_Sym *sym;

    uint32_t val, full_h = hash(name);

    val = get_sym_extra(name, full_h);
    if (val) return val;

    for (int k = 0; k < g_lib_count; k++) {
        sym = hash_get(g_loaded_libs[k], full_h, name);
        if (!sym || sym->st_shndx == 0)
            continue;
        if (sym_ptr)
            *sym_ptr = sym;
        return (uint32_t) sym->st_value + (uint32_t) g_loaded_libs[k]->mem;
    }
    return 0;
}

int file_relocate(elfobj_t *dl) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)dl->file;
    Elf32_Shdr *shdr = (Elf32_Shdr *)(dl->file + ehdr->e_shoff);

    debug_printf(2, "| RLoc '%s'", dl->name);

    if (ehdr->e_type == ET_EXEC) {
        return 0;
    }

    uint32_t val;
    uint8_t type;
    char *name;

    for (uint32_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == 4) // SHT_RELA
            raise_error("SHT_RELA is not supported but found in '%s'", dl->name);

        if (shdr[i].sh_type != 9) // SHT_REL
            continue;

        Elf32_Rel *rel = (Elf32_Rel *)(dl->file + shdr[i].sh_offset);
        for (uint32_t j = 0; j < shdr[i].sh_size / sizeof(Elf32_Rel); j++) {
            name = (char *) dl->dym_str + (((Elf32_Sym *) dl->dym_tab) + ELF32_R_SYM(rel[j].r_info))->st_name;
            val = 0;
            type = ELF32_R_TYPE(rel[j].r_info);
            if (does_type_required_sym(type)) {
                val = get_sym_value(name, NULL);
                if (val == 0)
                    raise_error("'%s' requires symbol '%s'", dl->name, name);
            }
            switch (type) {
                case R_386_32:          // word32  S + A
                    val += *(uint32_t *)(dl->mem + rel[j].r_offset);
                    *(uint32_t *)(dl->mem + rel[j].r_offset) = val;
                    break;
                case R_386_PC32:        // word32  S + A - P
                    val += *(uint32_t *)(dl->mem + rel[j].r_offset);
                    val -= (uint32_t) (dl->mem + rel[j].r_offset);
                    *(uint32_t *)(dl->mem + rel[j].r_offset) = val;
                    break;
                case R_386_RELATIVE:    // word32  B + A
                    val = (uint32_t) dl->mem;
                    val += *(uint32_t *)(dl->mem + rel[j].r_offset);
                    *(uint32_t *)(dl->mem + rel[j].r_offset) = val;
                    break;
                case R_386_JMP_SLOT:    // word32  S
                    *(uint32_t *)(dl->mem + rel[j].r_offset) = val;
                    break;
                case R_386_GLOB_DAT:    // word32  S
                    *(uint32_t *)(dl->mem + rel[j].r_offset) = val;
                    break;
                default:
                    raise_error("relocation type %d in '%s' not supported", type, dl->name);
                    break;
            }
        }
    }
    return 0;
}

void *open_elf(const char *filename, uint16_t required_type, int isfatal) {
    static elfobj_t *libc = NULL;
    char *path = NULL;

    if (strcmp(filename, "libc.so")      == 0 ||
        strcmp(filename, "/lib/libc.so") == 0
    ) {
        if (libc) {
            debug_printf(1, "| E-Rf '%s'", filename);
            libc->ref_count++;
            return libc;
        }

        debug_printf(1, "| E-CP '%s'", filename);
        libc = dlgext_libc();
        add_loaded_lib(libc);
        libc->ref_count = 1;
        return libc;
    }

    uint32_t sid = search_elf_sid(filename, required_type, &path);
    if (IS_SID_NULL(sid)) {
        if (isfatal)
            raise_error("'%s' not found", filename);
        return NULL;
    }

    for (int i = 0; i < g_lib_count; i++) {
        if (strcmp(g_loaded_libs[i]->name, path))
            continue;
        debug_printf(1, "| Find '%s'", path);
        g_loaded_libs[i]->ref_count++;
        return g_loaded_libs[i];
    }

    debug_printf(1, "| Open '%s'", path);

    elfobj_t *obj = calloc(1, sizeof(elfobj_t));

    obj->size = fu_file_get_size(sid);
    obj->file = malloc(obj->size);
    obj->ref_count = 1;
    obj->need_free = 1;
    obj->name = path;

    fu_file_read(sid, obj->file, 0, obj->size);

    if (obj->size < sizeof(Elf32_Ehdr) || !is_valid_elf(obj->file, required_type)) {
        if (isfatal)
            raise_error("'%s' is not a valid ELF file", path);
        free(obj->file);
        free(obj->name);
        free(obj);
        return NULL;
    }

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)obj->file;
    Elf32_Shdr *shdr = (Elf32_Shdr *)(obj->file + ehdr->e_shoff);

    for (int i = 0; i < ehdr->e_shnum; i++) {
        switch (shdr[i].sh_type) {
            case 2: // SHT_SYMTAB
                obj->sym_tab = (Elf32_Sym *)(obj->file + shdr[i].sh_offset);
                obj->sym_str = (char *) obj->file + shdr[shdr[i].sh_link].sh_offset;
                obj->sym_size = shdr[i].sh_size;
                break;

            case 6: // SHT_sym_str
                obj->dynamic = (Elf32_Dyn *)(obj->file + shdr[i].sh_offset);
                break;

            case 11: // SHT_DYNSYM
                obj->dym_tab = (Elf32_Sym *)(obj->file + shdr[i].sh_offset);
                obj->dym_str = (char *) obj->file + shdr[shdr[i].sh_link].sh_offset;
                obj->dym_size = shdr[i].sh_size;
                break;

            default:
                break;
        }
    }

    if (obj->dym_tab == NULL) {
        if (required_type == ET_DYN) {
            if (isfatal)
                raise_error("no dynamic symbol table found in '%s'", path);
            free(obj->file);
            free(obj->name);
            free(obj);
            return NULL;
        }
        return obj;
    }

    obj->hash_table = hash_create(obj);

    if (required_type == ET_DYN) {
        add_loaded_lib(obj);
        return obj;
    }

    char **new_libs = get_required_libs(obj);

    if (new_libs == NULL) {
        return obj;
    }

    for (int i = 0; new_libs[i] != NULL; i++) {
        int found = 0;
        for (int j = 0; g_loaded_libs && j < g_lib_count; j++) {
            if (strcmp(new_libs[i], g_loaded_libs[j]->name))
                continue;
            found = 1;
            break;
        }
        if (found) continue;

        elfobj_t *lib = open_elf(new_libs[i], ET_DYN, isfatal);
        if (lib == NULL)
            raise_error("dlopen failed for '%s'", new_libs[i]);
    }

    free(new_libs);

    return obj;
}

void init_lib(elfobj_t *lib) {
    // call constructors

    debug_printf(2, "| Init '%s'", lib->name);

    if (lib->dynamic == NULL)
        return;

    void (**init_array)(void) = NULL;
    int size = 0;

    for (int j = 0; lib->dynamic[j].d_tag != 0; j++) {
        if (lib->dynamic[j].d_tag == 25) { // DT_INIT
            init_array = (void (**)(void)) (lib->mem + lib->dynamic[j].d_un.d_ptr);
        }
        if (lib->dynamic[j].d_tag == 27) { // DT_INIT_ARRAYSZ
            size = lib->dynamic[j].d_un.d_val / sizeof(void *);
        }
    }

    if (init_array == NULL)
        return;

    for (int i = 0; i < size; i++) {
        init_array[i]();
    }
}

void fini_lib(elfobj_t *lib) {
    // call destructors

    if (lib->dynamic == NULL)
        return;

    debug_printf(2, "| Fini '%s'", lib->name);

    void (**fini_array)(void) = NULL;
    int size = 0;

    for (int j = 0; lib->dynamic[j].d_tag != 0; j++) {
        if (lib->dynamic[j].d_tag == 26) { // DT_FINI
            fini_array = (void (**)(void)) (lib->mem + lib->dynamic[j].d_un.d_ptr);
        }
        if (lib->dynamic[j].d_tag == 28) { // DT_FINI_ARRAYSZ
            size = lib->dynamic[j].d_un.d_val / sizeof(void *);
        }
    }

    if (fini_array == NULL)
        return;

    for (int i = 0; i < size; i++) {
        fini_array[i]();
    }
}

void *dlopen(const char *filename, int flag) {
    elfobj_t *dl = open_elf(filename, ET_DYN, flag == RTLD_FATAL);
    if (dl == NULL) {
        g_dlfcn_error = 1;
        return NULL;
    }

    g_dlfcn_error = 0;
    if (dl->ref_count > 1)
        return dl;

    load_sections(dl, ET_DYN);
    file_relocate(dl);
    init_lib(dl);
    return dl;
}

void *dlsym(void *handle, const char *symbol) {
    if (handle == NULL) {
        return (void *) get_sym_value(symbol, NULL);
    }

    uint32_t val, full_h = hash(symbol);
    elfobj_t *dl = handle;
    Elf32_Sym *ret;

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *) dl->file;

    g_dlfcn_error = 0;
    if (ehdr->e_type == ET_EXEC) {
        ret = hash_get(dl, full_h, symbol);
        if (!ret)
            g_dlfcn_error = 2;
        return (void *) ret->st_value;
    }

    val = get_sym_extra(symbol, full_h);
    if (!val) {
        g_dlfcn_error = 2;
        return NULL;
    }

    ret = hash_get(dl, full_h, symbol);
    if (!ret) {
        g_dlfcn_error = 2;
        return NULL;
    }
    return (void *) dl->mem + ret->st_value;
}

int dlclose(void *handle) {
    if (handle == NULL)
        return 0;

    elfobj_t *dl = handle;

    if (--dl->ref_count > 0) {
        debug_printf(1, "| Dref '%s' (%d)", dl->name, dl->ref_count);
        return 0;
    }

    debug_printf(1, "| Free '%s'", dl->name);

    // remove from loaded libs
    for (int i = 0; i < g_lib_count; i++) {
        if (g_loaded_libs[i] != dl)
            continue;
        g_lib_count--;
        for (int j = i; j < g_lib_count; j++) {
            g_loaded_libs[j] = g_loaded_libs[j + 1];
        }
        break;
    }

    if (!g_already_fini) {
        fini_lib(dl);
    }

    if (dl->need_free) {
        free(dl->hash_table);
        free(dl->file);
        free(dl->name);
    }

    free(dl->mem);
    free(dl);
    return 0;
}

char *dlerror(void) {
    char *error;
    if (!g_dlfcn_error)
        return NULL;
    if (g_dlfcn_error == 1)
        error = "deluge dlfcn: failed to open file";
    else
        error = "deluge dlfcn: symbol not found";
    g_dlfcn_error = 0;
    return error;
}

int dynamic_linker(elfobj_t *exec) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)exec->file;
    Elf32_Shdr *shdr = (Elf32_Shdr *)(exec->file + ehdr->e_shoff);

    if (ehdr->e_type != ET_EXEC || !exec->dym_tab) {
        return 0;
    }

    debug_printf(1, "| Link '%s'", exec->name);

    Elf32_Sym *sym;
    uint32_t val;
    uint8_t type;
    char *name;

    for (uint32_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == 4) // SHT_RELA
            raise_error("SHT_RELA is not supported but found in '%s'", exec->name);

        if (shdr[i].sh_type != 9) // SHT_REL
            continue;

        Elf32_Rel *rel = (Elf32_Rel *)(exec->file + shdr[i].sh_offset);
        for (uint32_t j = 0; j < shdr[i].sh_size / sizeof(Elf32_Rel); j++) {
            name = (char *) exec->dym_str + (exec->dym_tab + ELF32_R_SYM(rel[j].r_info))->st_name;
            type = ELF32_R_TYPE(rel[j].r_info);
            if (does_type_required_sym(type)) {
                val = get_sym_value(name, &sym);
                if (!val) {
                    sym = hash_get(exec, hash(name), name);
                    if (!sym || sym->st_shndx == 0)
                        raise_error("'%s' requires symbol '%s'", exec->name, name);
                    val = (uint32_t) sym->st_value;
                }
            }
            switch (type) {
                case R_386_32:          // word32  S + A
                    val += *(uint32_t *)(rel[j].r_offset);
                    *(uint32_t *)(rel[j].r_offset) = val;
                    break;
                case R_386_COPY:        // symbol  S
                    memcpy((void *) rel[j].r_offset, (void *) val, sym->st_size);
                    break;
                case R_386_GLOB_DAT:    // word32  S
                    *(uint32_t *)(rel[j].r_offset) = val;
                    break;
                case R_386_JMP_SLOT:    // word32  S
                    *(uint32_t *)(rel[j].r_offset) = val;
                    break;
                default:
                    raise_error("relocation type %d in '%s' not supported", type, exec->name);
                    break;
            }
        }
    }

    return 0;
}

char *profan_fn_name(void *ptr, char **libname) {
    uint32_t addr = (uint32_t) ptr;

    if (libname)
        *libname = NULL;

    // look inside the g_prog
    if (g_prog->sym_tab) {
        for (uint32_t j = 0; j < g_prog->sym_size / sizeof(Elf32_Sym); j++) {
            if (addr < g_prog->sym_tab[j].st_value ||
                addr >= g_prog->sym_tab[j].st_value + g_prog->sym_tab[j].st_size
            ) continue;
            if (libname)
                *libname = g_prog->name;
            return g_prog->sym_str + g_prog->sym_tab[j].st_name;
        }
    }

    // look inside the loaded libraries
    for (int i = 0; i < g_lib_count; i++) {
        if (!g_loaded_libs[i]->sym_tab)
            continue;

        Elf32_Sym *sym = g_loaded_libs[i]->sym_tab;
        char *sym_str = g_loaded_libs[i]->sym_str;

        for (uint32_t k = 0; k < g_loaded_libs[i]->sym_size / sizeof(Elf32_Sym); k++) {
            uint32_t val = (uint32_t) g_loaded_libs[i]->mem + sym[k].st_value;
            if (addr < val || addr >= val + sym[k].st_size)
                continue;
            if (libname)
                *libname = g_loaded_libs[i]->name;
            return sym_str + sym[k].st_name;
        }
    }

    return NULL;
}

/***********************************
 *                                *
 *    LibC interface functions    *
 *                                *
***********************************/

void libc_enable_leaks(void) {
    void (*buddy_enable_leaks)(void) = (void *) get_sym_value("__buddy_enable_leaks", NULL);

    if (buddy_enable_leaks) {
        buddy_enable_leaks();
    } else {
        raise_error("failed to enable leaks");
    }
}

/*********************************
 *                              *
 *    Command line Interface    *
 *                              *
*********************************/

typedef struct {
    char *name;
    uint8_t show_leaks;
    int arg_offset;
} deluge_args_t;

void show_help(int full) {
    if (!full) {
        fd_printf(2, "Try 'deluge -h' for more information.\n");
        exit(1);
    }
    fd_printf(1,
        "Usage: deluge [options] <file> [args]\n"
        "Options:\n"
        "  -d  show additional debug info\n"
        "  -e  don't use filename as argument\n"
        "  -h  show this help message\n"
        "  -l  list main linking steps\n"
        "  -m  show userspace memory leaks\n"
        "  -p  add path to extra libraries\n"
        "  -v  show version\n"
    );
}

deluge_args_t deluge_parse(int argc, char **argv) {
    deluge_args_t args;
    args.name = NULL;

    g_print_debug = ALWAYS_DEBUG;
    args.show_leaks = 0;

    g_extralib_path = NULL;

    int move_arg = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            args.name = argv[i];
            args.arg_offset = i + move_arg;
            break;
        }
        switch (argv[i][1]) {
            case 'l':
                if (!g_print_debug)
                    g_print_debug = 1;
                break;
            case 'd':
                g_print_debug = 2;
                break;
            case 'e':
                move_arg = 1;
                break;
            case 'h':
                show_help(1);
                exit(0);
                break; // unreachable
            case 'm':
                args.show_leaks = 1;
                break;
            case 'p':
                if (i + 1 >= argc) {
                    fd_printf(2, "deluge: missing argument for -p\n");
                    show_help(0);
                }
                if (g_extralib_path) {
                    fd_printf(2, "deluge: extra library path already set\n");
                    show_help(0);
                }
                g_extralib_path = argv[++i];
                break;
            case 'v':
                fd_printf(1, "deluge %s\n", DELUGE_VERSION);
                exit(0);
                break; // unreachable
            default:
                fd_printf(2, "deluge: invalid option -- '%c'\n", argv[i][1]);
                show_help(0);
                break; // unreachable
        }
    }

    if (args.name == NULL) {
        fd_printf(2, "deluge: missing file name\n");
        show_help(0);
    }

    return args;
}

int main(int argc, char **argv, char **envp) {
    g_loaded_libs = NULL;
    g_already_fini = 0;
    g_dlfcn_error = 0;
    g_lib_count = 0;
    g_envp = envp;


    deluge_args_t args = deluge_parse(argc, argv);
    int pid, parent_pid = syscall_process_pid();

    int start, ret;

    if (g_print_debug) {
        start = syscall_timer_get_ms();
    }

    g_prog = open_elf(args.name, ET_EXEC, 1);

    if (g_prog == NULL) {
        raise_error("failed to open '%s'", args.name);
        return 1;
    }

    for (int i = 0; i < g_lib_count; i++) {
        load_sections(g_loaded_libs[i], ET_DYN);
    }

    for (int i = 0; i < g_lib_count; i++) {
        file_relocate(g_loaded_libs[i]);
    }

    for (int i = 0; i < g_lib_count; i++) {
        init_lib(g_loaded_libs[i]);
    }

    load_sections(g_prog, ET_EXEC);
    dynamic_linker(g_prog);

    debug_printf (1, "Link time: %d ms", syscall_timer_get_ms() - start);

    int (*main)() = (int (*)(int, char **, char **)) ((Elf32_Ehdr *) g_prog->file)->e_entry;

    free(g_prog->hash_table);

    g_dlfcn_error = 0;

    if (g_print_debug) {
        start = syscall_timer_get_ms();
    }

    if (args.show_leaks) {
        libc_enable_leaks();
    }

    ret = main(argc - args.arg_offset, argv + args.arg_offset, envp);

    pid = syscall_process_pid();

    if (pid != parent_pid) {
        debug_printf(1, "PID %d (child of %d) exit with code %d after %d ms",
                pid, parent_pid, ret, syscall_timer_get_ms() - start);
        syscall_process_exit(pid, ret, 1);
    }

    debug_printf(1, "PID %d exit with code %d in %d ms", pid, ret, syscall_timer_get_ms() - start);

    for (int i = 0; i < g_lib_count; i++)
        fini_lib(g_loaded_libs[i]);

    g_already_fini = 1;

    while (g_lib_count) {
        if (g_loaded_libs[0]->ref_count > 1) {
            debug_printf(1, "Unclosed library '%s'", g_loaded_libs[0]->name);
            g_loaded_libs[0]->ref_count = 1;
        }
        dlclose(g_loaded_libs[0]);
    }

    free(g_loaded_libs);
    free(g_prog->file);
    free(g_prog->name);
    free(g_prog);

    return ret;
}
