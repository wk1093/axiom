#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "axld_exec.h"
#include "axas_assembler.h"
#include "axas_object.h"
#include "axar_archive.h"

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("Usage: %s <input_files> [-o <output_file>] [-l <static_library>]\n", argv[0]);
        printf("Options:\n");
        printf("  -o <file>   Specify output file (default is a.out extension)\n");
        printf("  -l <file>   Specify static library to link against\n");
        return 1;
    }

    const char** filenames = ax_vecNew(const char*);
    char* output_filename = NULL;

    const char** static_libs = ax_vecNew(const char*);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            ax_vecPush(static_libs, argv[++i]);
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
    // AxObject* objs = malloc(sizeof(AxObject) * n);
    AxObject** objs = ax_vecNew(AxObject*);

    for (size_t i = 0; i < n; i++) {
        const char* filename = filenames[i];
        // printf("Processing file: %s\n", filename);
        ax_vecPush(objs, malloc(sizeof(AxObject)));
        if (!ax_objectLoad(objs[ax_vecSize(objs) - 1], filename)) {
            printf("Error: Failed to load object file %s.\n", filename);
            free(objs);
            return 1;
        }
    }

    // Pass 1: register all symbols and compute layout offsets.
    for (size_t i = 0; i < ax_vecSize(objs); i++)
        ax_execRegisterSymbols(&exec, objs[i]);
    
    // Pass 1.5: find undefined symbols, and see if any of the static libraries can resolve them. (if not, print errors and exit)
    char** undefined_syms = exec.undefined_sym_names;
    for (size_t i = 0; i < ax_vecSize(undefined_syms); i++) {
        const char* sym_name = undefined_syms[i];
        bool resolved = false;
        for (size_t j = 0; j < ax_vecSize(static_libs); j++) {
            const char* lib_path = static_libs[j];
            AxArchive ar;
            if (!axar_read_archive(lib_path, &ar)) {
                printf("Error: Failed to read static library %s.\n", lib_path);
                continue;
            }

            size_t sym_idx = axar_find_symbol(&ar, sym_name);
            if (sym_idx != SIZE_MAX) {
                printf("Resolved symbol %s in library %s (member %zu)\n", sym_name, lib_path, ar.symtab[sym_idx].member_idx);
                AxObject* lib_obj = &ar.members[ar.symtab[sym_idx].member_idx];
                ax_execRegisterSymbols(&exec, lib_obj);
                ax_vecPush(objs, lib_obj); // add this library member to the list of objects to link
                resolved = true;
                break;
            }
            axar_archive_free(&ar);
        }
        if (!resolved) {
            printf("Error: Unresolved symbol %s\n", sym_name);
            for (size_t j = 0; j < ax_vecSize(objs); j++)
                ax_objectFree(objs[j]);
            ax_vecFree(objs);

            
            return 1;
        }
    }

    // Pass 2: copy code/data and patch all relocations.
    for (size_t i = 0; i < ax_vecSize(objs); i++) {
        ax_execCopyAndPatch(&exec, objs[i]);
        ax_objectFree(objs[i]);
    }
    for (size_t i = 0; i < ax_vecSize(objs); i++)
        free(objs[i]);
    ax_vecFree(objs);
    
    if (ax_execWrite(&exec, output_filename)) {
        // printf("Axiom: Linked executable %s successfully.\n", output_filename);
    } else {
        printf("Axiom: Failed to write executable %s.\n", output_filename);
    }
    ax_execFree(&exec);

    return 0;
}
