#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "axld_exec.h"
#include "axas_assembler.h"
#include "axas_object.h"

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("Usage: %s <input_file> [-o <output_file>]\n", argv[0]);
        printf("Options:\n");
        printf("  -o <file>   Specify output file (default is a.out extension)\n");
        return 1;
    }

    const char* filename = NULL;
    char* output_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        } else if (!filename) {
            filename = argv[i];
        } else {
            printf("Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (!filename) {
        printf("Error: No input file specified.\n");
        return 1;
    }

    if (!output_filename) {
        output_filename = "a.out";
    }

    AxObject obj;
    if (!ax_objectLoad(&obj, output_filename)) {
        printf("Error: Failed to load object file %s.\n", output_filename);
        return 1;
    }

    ax_printObjectInfo(&obj);

    AxExecutable exec;
    ax_execInit(&exec);
    ax_execLink(&exec, &obj);
    char exec_filename[256];
    snprintf(exec_filename, sizeof(exec_filename), "%s", output_filename);
    if (ax_execWrite(&exec, exec_filename)) {
        printf("Axiom: Linked executable %s successfully.\n", exec_filename);
    } else {
        printf("Axiom: Failed to write executable %s.\n", exec_filename);
    }
    ax_objectFree(&obj);
    ax_execFree(&exec);

    return 0;
}
