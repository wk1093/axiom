#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "axld_exec.h"
#include "axas_assembler.h"
#include "axas_object.h"

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("Usage: %s <input_files> [-o <output_file>]\n", argv[0]);
        printf("Options:\n");
        printf("  -o <file>   Specify output file (default is a.out extension)\n");
        return 1;
    }

    const char** filenames = ax_vecNew(const char*);
    char* output_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        } else {
            ax_vecPush(filenames, argv[i]);
        }
    }

    if (ax_vecSize(filenames) == 0) {
        printf("Error: No input files specified.\n");
        return 1;
    }

    if (!output_filename) {
        output_filename = "a.out";
    }
    
    // AxObject obj;
    // if (!ax_objectLoad(&obj, filename)) {
    //     printf("Error: Failed to load object file %s.\n", filename);
    //     return 1;
    // }

    AxExecutable exec;
    ax_execInit(&exec);

    // Load all objects up front so we can do two-pass linking:
    // pass 1 registers every symbol before any relocations are patched,
    // which lets forward cross-file references resolve correctly.
    size_t n = ax_vecSize(filenames);
    AxObject* objs = malloc(sizeof(AxObject) * n);
    size_t obj_count = 0;

    for (size_t i = 0; i < n; i++) {
        const char* filename = filenames[i];
        // printf("Processing file: %s\n", filename);
        if (!ax_objectLoad(&objs[obj_count], filename)) {
            printf("Error: Failed to load object file %s.\n", filename);
            free(objs);
            return 1;
        }
        obj_count++;
    }

    // Pass 1: register all symbols and compute layout offsets.
    for (size_t i = 0; i < obj_count; i++)
        ax_execRegisterSymbols(&exec, &objs[i]);

    // Pass 2: copy code/data and patch all relocations.
    for (size_t i = 0; i < obj_count; i++) {
        ax_execCopyAndPatch(&exec, &objs[i]);
        ax_objectFree(&objs[i]);
    }
    free(objs);
    
    if (ax_execWrite(&exec, output_filename)) {
        // printf("Axiom: Linked executable %s successfully.\n", output_filename);
    } else {
        printf("Axiom: Failed to write executable %s.\n", output_filename);
    }
    ax_execFree(&exec);

    return 0;
}
