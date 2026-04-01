#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "axar_archive.h"
#include <arpa/inet.h>
#include <libgen.h>
#include <endian.h>
#include "axas_object.h"

static uint32_t hton_32(uint32_t x) {
    return htonl(x);
}

static uint64_t decode_be64(const void* ptr) {
    uint64_t val;
    memcpy(&val, ptr, 8);
    return be64toh(val);
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

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

bool axar_read_archive(const char* path, AxArchive* out) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror("axar_read_archive: fopen"); return false; }

    char magic[AXAR_MAGIC_LEN];
    if (fread(magic, 1, AXAR_MAGIC_LEN, f) != AXAR_MAGIC_LEN ||
        memcmp(magic, AXAR_MAGIC, AXAR_MAGIC_LEN) != 0) {
        fprintf(stderr, "axar_read_archive: %s is not a valid archive\n", path);
        fclose(f);
        return false;
    }

    typedef struct { long data_offset; long size; } MemberLoc;
    MemberLoc* locs   = NULL;
    size_t locs_cap   = 0, locs_n = 0;

    uint8_t* sym_raw   = NULL; // raw bytes of the symbol table member
    long     sym_raw_n = 0;
    bool     sym_is_64 = false; // true for /SYM64/ (8-byte BE offsets)

    char* longnametab   = NULL; // raw bytes of the '//' extended filename table

    struct ar_header hdr;
    while (fread(&hdr, sizeof(hdr), 1, f) == 1) {
        if (memcmp(hdr.ar_fmag, "`\n", 2) != 0) {
            fprintf(stderr, "axar_read_archive: bad member magic\n");
            break;
        }
        long size       = strtol(hdr.ar_size, NULL, 10);
        long data_start = ftell(f);

        // Identify the three special member types by their name field (16 bytes, not null-terminated).
        // '/'        → GNU 32-bit symbol table  (ar_name = "/ " ... padded with spaces)
        // '/SYM64/'  → GNU 64-bit symbol table  (ar_name starts with "/SYM64/")
        // '//'       → GNU extended filename table (ar_name = "// " ... padded with spaces)
        // '/<digits>' inside a regular member name → offset into '//' table (treated as regular member)
        bool is_sym64    = (memcmp(hdr.ar_name, "/SYM64/", 7) == 0);
        bool is_longname = (hdr.ar_name[0] == '/' && hdr.ar_name[1] == '/');
        bool is_symtab   = !is_sym64 && !is_longname &&
                           (hdr.ar_name[0] == '/' && hdr.ar_name[1] == ' ');

        if (is_symtab || is_sym64) {
            free(sym_raw);
            sym_raw   = malloc(size);
            sym_raw_n = size;
            sym_is_64 = is_sym64;
            if (fread(sym_raw, 1, size, f) != (size_t)size) {
                free(sym_raw); sym_raw = NULL; sym_raw_n = 0;
            }
        } else if (is_longname) {
            free(longnametab);
            longnametab   = malloc(size + 1);
            if (fread(longnametab, 1, size, f) != (size_t)size) {
                free(longnametab); longnametab = NULL;
            } else {
                longnametab[size] = '\0';
            }
        } else {
            // Regular object member (inline name, or '/<offset>' long-name reference)
            if (locs_n == locs_cap) {
                locs_cap = locs_cap ? locs_cap * 2 : 8;
                locs = realloc(locs, locs_cap * sizeof(MemberLoc));
            }
            locs[locs_n++] = (MemberLoc){ data_start, size };
            fseek(f, size, SEEK_CUR);
        }
        if (size % 2 != 0) fseek(f, 1, SEEK_CUR);
    }

    // -- Sub-pass B: load each member as an AxObject --
    out->members     = malloc((locs_n ? locs_n : 1) * sizeof(AxObject));
    out->num_members = 0;

    char buf[4096];
    for (size_t i = 0; i < locs_n; i++) {
        FILE* tmp = tmpfile();
        fseek(f, locs[i].data_offset, SEEK_SET);
        long remaining = locs[i].size;
        while (remaining > 0) {
            size_t chunk = remaining < (long)sizeof(buf) ? (size_t)remaining : sizeof(buf);
            size_t nr = fread(buf, 1, chunk, f);
            if (nr == 0) break;
            fwrite(buf, 1, nr, tmp);
            remaining -= (long)nr;
        }
        rewind(tmp);

        int fd = fileno(tmp);
        char fd_path[64];
        snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);

        AxObject* obj = &out->members[out->num_members];
        ax_objectInit(obj);
        if (ax_objectLoad(obj, fd_path)) {
            out->num_members++;
        } else {
            // Not every archive member is necessarily an ELF we can parse; skip silently.
        }
        fclose(tmp);
    }

    // -- Parse symtab to build AxArSymEntry array --
    out->symtab      = NULL;
    out->num_symbols = 0;

    if (sym_raw && !sym_is_64 && sym_raw_n >= 4) {
        // 32-bit GNU format: [4B BE count][4B BE offsets * count][null-terminated names...]
        uint32_t nsym;
        memcpy(&nsym, sym_raw, 4);
        nsym = ntohl(nsym);

        if ((long)(4 + (uint64_t)4 * nsym) <= sym_raw_n) {
            out->symtab      = malloc(nsym * sizeof(AxArSymEntry));
            out->num_symbols = 0;

            uint32_t*   offsets = (uint32_t*)(sym_raw + 4);
            const char* names   = (const char*)(sym_raw + 4 + 4 * nsym);

            for (uint32_t s = 0; s < nsym; s++) {
                uint32_t file_off = ntohl(offsets[s]);
                long data_off = (long)file_off + (long)sizeof(struct ar_header);
                size_t midx = SIZE_MAX;
                for (size_t m = 0; m < locs_n; m++) {
                    if (locs[m].data_offset == data_off) { midx = m; break; }
                }
                out->symtab[out->num_symbols++] = (AxArSymEntry){
                    .name       = strdup(names),
                    .member_idx = (uint32_t)midx,
                };
                names += strlen(names) + 1;
            }
        }
    } else if (sym_raw && sym_is_64 && sym_raw_n >= 8) {
        // 64-bit GNU format (/SYM64/): [8B BE count][8B BE offsets * count][null-terminated names...]
        uint64_t nsym = decode_be64(sym_raw);

        if ((long)(8 + nsym * 8) <= sym_raw_n) {
            out->symtab      = malloc(nsym * sizeof(AxArSymEntry));
            out->num_symbols = 0;

            const char* names = (const char*)(sym_raw + 8 + nsym * 8);

            for (uint64_t s = 0; s < nsym; s++) {
                uint64_t file_off = decode_be64(sym_raw + 8 + s * 8);
                long data_off = (long)file_off + (long)sizeof(struct ar_header);
                size_t midx = SIZE_MAX;
                for (size_t m = 0; m < locs_n; m++) {
                    if (locs[m].data_offset == data_off) { midx = m; break; }
                }
                out->symtab[out->num_symbols++] = (AxArSymEntry){
                    .name       = strdup(names),
                    .member_idx = (uint32_t)midx,
                };
                names += strlen(names) + 1;
            }
        }
    }

    free(sym_raw);
    free(longnametab);
    free(locs);
    fclose(f);
    return true;
}

void axar_archive_free(AxArchive* ar) {
    for (size_t i = 0; i < ar->num_members; i++)
        ax_objectFree(&ar->members[i]);
    free(ar->members);
    for (size_t i = 0; i < ar->num_symbols; i++)
        free(ar->symtab[i].name);
    free(ar->symtab);
    ar->members     = NULL;
    ar->symtab      = NULL;
    ar->num_members = 0;
    ar->num_symbols = 0;
}

size_t axar_find_symbol(const AxArchive* ar, const char* name) {
    for (size_t i = 0; i < ar->num_symbols; i++)
        if (strcmp(ar->symtab[i].name, name) == 0)
            return ar->symtab[i].member_idx;
    return SIZE_MAX;
}

void axar_list_symbols(const AxArchive* ar) {
    for (size_t i = 0; i < ar->num_symbols; i++) {
        printf("%s -> member %u\n", ar->symtab[i].name, ar->symtab[i].member_idx);
    }
}