#ifndef AXAR_ARCHIVE_H
#define AXAR_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <axas_object.h>

#define AXAR_MAGIC "!<arch>\n"
#define AXAR_MAGIC_LEN 8

struct ar_header {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6];
    char ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
};

// One entry in the archive's global symbol index.
typedef struct {
    char*    name;       // symbol name (owned)
    uint32_t member_idx; // index into AxArchive.members
} AxArSymEntry;

// A parsed .a archive. Members are fully loaded AxObjects ready for linking.
typedef struct {
    AxObject*    members;     // owned array
    size_t       num_members;
    AxArSymEntry* symtab;    // owned array (from the '/' index member)
    size_t       num_symbols;
} AxArchive;

// Write an archive from a list of object file paths.
void axar_write_archive(FILE* ar_file, char* obj_files[], int num_obj_files);

// Parse an archive file into out. Returns false on error.
bool axar_read_archive(const char* path, AxArchive* out);

// Free all resources owned by an AxArchive.
void axar_archive_free(AxArchive* ar);

// Look up which member index defines a symbol. Returns SIZE_MAX if not found.
size_t axar_find_symbol(const AxArchive* ar, const char* name);

void axar_list_symbols(const AxArchive* ar);

#endif