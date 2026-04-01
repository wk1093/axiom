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
        ar_file = fopen(argv[1], "r");
    } else {
        ar_file = fopen(argv[1], "w");
    }
    if (!ar_file) {
        perror("fopen");
        return 1;
    }

    if (argc == 2) {
        fprintf(stderr, "No object files provided\n");
        axar_read_archive(ar_file);
        fclose(ar_file);
        return 1;
    }

    axar_write_archive(ar_file, &argv[2], argc - 2);

    fclose(ar_file);

    return 0;
}