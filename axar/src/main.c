#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <axar_archive.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <archive_file> [obj_file]...\n", argv[0]);
        return 1;
    }

    
    FILE *ar_file;
    if (argc == 2) {
        AxArchive ar;
        if (!axar_read_archive(argv[1], &ar)) return 1;
        printf("Members: %zu, Symbols: %zu\n", ar.num_members, ar.num_symbols);
        for (size_t i = 0; i < ar.num_symbols; i++)
            printf("  %s -> member %u\n", ar.symtab[i].name, ar.symtab[i].member_idx);
        axar_archive_free(&ar);
        return 0;
    }

    ar_file = fopen(argv[1], "w");
    if (!ar_file) {
        perror("fopen");
        return 1;
    }
    axar_write_archive(ar_file, &argv[2], argc - 2);
    fclose(ar_file);

    return 0;
}