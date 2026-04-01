#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "axar_archive.h"
#include <arpa/inet.h>
#include <libgen.h>
#include "axas_object.h"

static uint32_t hton_32(uint32_t x) {
    return htonl(x);
}

// Write a value into a fixed-width ar header field as left-justified ASCII, space-padded, no null.
#define AR_WRITE_FIELD(field, fmt, val) do { \
    char _tmp[64]; \
    int _n = snprintf(_tmp, sizeof(_tmp), fmt, val); \
    if (_n > (int)sizeof(field)) _n = (int)sizeof(field); \
    memcpy(field, _tmp, _n); \
} while (0)

void axar_write_archive(FILE *ar_file, char *obj_files[], int num_obj_files) {
    fwrite(AXAR_MAGIC, 1, AXAR_MAGIC_LEN, ar_file);

    // --- Pass 1: load all objects, collect global symbols ---
    AxObject* objs    = malloc(num_obj_files * sizeof(AxObject));
    struct stat* sts  = malloc(num_obj_files * sizeof(struct stat));
    int* valid        = malloc(num_obj_files * sizeof(int));

    // sym_names: strdup'd copies so they survive ax_objectFree
    char**    sym_names   = ax_vecNew(char*);
    uint32_t* sym_obj_idx = ax_vecNew(uint32_t);

    for (int i = 0; i < num_obj_files; i++) {
        ax_objectInit(&objs[i]);
        valid[i] = ax_objectLoad(&objs[i], obj_files[i]);
        if (!valid[i]) {
            fprintf(stderr, "Failed to load object file %s\n", obj_files[i]);
            continue;
        }
        stat(obj_files[i], &sts[i]);

        for (size_t j = 0; j < ax_vecSize(objs[i].symtab); j++) {
            Elf64_Sym* sym = &objs[i].symtab[j];
            if (ELF64_ST_BIND(sym->st_info) == STB_GLOBAL && sym->st_shndx != SHN_UNDEF) {
                char* name = strdup(&objs[i].strtab[sym->st_name]);
                uint32_t idx = (uint32_t)i;
                ax_vecPush(sym_names, name);
                ax_vecPush(sym_obj_idx, idx);
            }
        }
    }

    uint32_t num_symbols = (uint32_t)ax_vecSize(sym_names);

    // --- Compute symbol table content size ---
    // Layout: [4B count] [4B * num_symbols offsets] [null-terminated names...]
    long sym_table_size = 4 + 4 * (long)num_symbols;
    for (uint32_t i = 0; i < num_symbols; i++) {
        sym_table_size += (long)strlen(sym_names[i]) + 1;
    }

    // --- Compute the file offset at which each object member header will start ---
    // Layout on disk:
    //   [8B magic] [60B symtab ar_header] [sym_table_size B] [0-1B padding]
    //   [60B obj0 ar_header] [obj0 data] [0-1B padding] ...
    long obj_start = AXAR_MAGIC_LEN
                   + (long)sizeof(struct ar_header)
                   + sym_table_size
                   + (sym_table_size % 2 ? 1 : 0);

    long* obj_offsets = malloc(num_obj_files * sizeof(long));
    for (int i = 0; i < num_obj_files; i++) {
        if (!valid[i]) { obj_offsets[i] = 0; continue; }
        obj_offsets[i] = obj_start;
        obj_start += (long)sizeof(struct ar_header)
                   + sts[i].st_size
                   + (sts[i].st_size % 2 ? 1 : 0);
    }

    // --- Write symbol table member (must be first, immediately after magic) ---
    struct ar_header sym_header;
    memset(&sym_header, ' ', sizeof(sym_header));
    memcpy(sym_header.ar_name, "/               ", 16);
    AR_WRITE_FIELD(sym_header.ar_date, "%ld", (long)time(NULL));
    AR_WRITE_FIELD(sym_header.ar_size, "%ld", sym_table_size);
    memcpy(sym_header.ar_fmag, "`\n", 2);
    fwrite(&sym_header, sizeof(sym_header), 1, ar_file);

    uint32_t num_sym_be = hton_32(num_symbols);
    fwrite(&num_sym_be, sizeof(uint32_t), 1, ar_file);

    for (uint32_t i = 0; i < num_symbols; i++) {
        uint32_t off_be = hton_32((uint32_t)obj_offsets[sym_obj_idx[i]]);
        fwrite(&off_be, sizeof(uint32_t), 1, ar_file);
    }
    for (uint32_t i = 0; i < num_symbols; i++) {
        fwrite(sym_names[i], strlen(sym_names[i]) + 1, 1, ar_file);
    }
    if (sym_table_size % 2 != 0) fputc('\n', ar_file);

    // --- Write each object member ---
    for (int i = 0; i < num_obj_files; i++) {
        if (!valid[i]) continue;

        // Member name: basename(path) + '/', space-padded, no null
        char name_buf[18];
        char* path_copy = strdup(obj_files[i]);
        snprintf(name_buf, sizeof(name_buf), "%s/", basename(path_copy));
        free(path_copy);

        struct ar_header header;
        memset(&header, ' ', sizeof(header));
        size_t name_len = strlen(name_buf);
        if (name_len > 16) name_len = 16;
        memcpy(header.ar_name, name_buf, name_len);
        AR_WRITE_FIELD(header.ar_date, "%ld", (long)sts[i].st_mtime);
        AR_WRITE_FIELD(header.ar_mode, "%o",  (unsigned)sts[i].st_mode);
        AR_WRITE_FIELD(header.ar_size, "%ld", (long)sts[i].st_size);
        memcpy(header.ar_fmag, "`\n", 2);
        fwrite(&header, sizeof(header), 1, ar_file);

        FILE* obj_fp = fopen(obj_files[i], "rb");
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), obj_fp)) > 0) {
            fwrite(buffer, 1, n, ar_file);
        }
        fclose(obj_fp);

        if (sts[i].st_size % 2 != 0) fputc('\n', ar_file);

        ax_objectFree(&objs[i]);
    }

    // Cleanup
    for (uint32_t i = 0; i < num_symbols; i++) free(sym_names[i]);
    ax_vecFree(sym_names);
    ax_vecFree(sym_obj_idx);
    free(obj_offsets);
    free(objs);
    free(sts);
    free(valid);
}

void axar_read_archive(FILE *ar_file) {
    char magic[AXAR_MAGIC_LEN];
    fread(magic, 1, AXAR_MAGIC_LEN, ar_file);
    if (strncmp(magic, AXAR_MAGIC, AXAR_MAGIC_LEN) != 0) {
        fprintf(stderr, "Not a valid archive file\n");
        return;
    }

    printf("Is a valid archive file\n");

    struct ar_header header;
    while (fread(&header, sizeof(struct ar_header), 1, ar_file) == 1) {
        long size = strtol(header.ar_size, NULL, 10);

        if (strncmp(header.ar_fmag, "`\n", 2) != 0) {
            fprintf(stderr, "Invalid header file magic\n");
            return;
        }

        if (strncmp(header.ar_name, "/ ", 2) == 0) {
            printf("Symbol lookup table found\n");
            // TODO: process symbol table
        } else {
            printf("File: %.*s, size: %ld\n", (int)sizeof(header.ar_name), header.ar_name, size);
        }

        // TODO: read file content
        fseek(ar_file, size, SEEK_CUR);

        // even byte alignment
        if (size % 2 != 0) {
            fseek(ar_file, 1, SEEK_CUR);
        }
    }
}
