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
        return 1;
    }

    const char** input_filenames = ax_vecNew(const char*);
    const char** lib_paths = ax_vecNew(const char*);
    char* output_filename = "a.out";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            ax_vecPush(lib_paths, argv[++i]);
        } else {
            ax_vecPush(input_filenames, argv[i]);
        }
    }

    AxExecutable exec;
    ax_execInit(&exec);
    AxObject** objs = ax_vecNew(AxObject*);

    // 1. Load primary object files
    for (size_t i = 0; i < ax_vecSize(input_filenames); i++) {
        AxObject* obj = malloc(sizeof(AxObject));
        if (!ax_objectLoad(obj, input_filenames[i])) {
            fprintf(stderr, "Error: Failed to load %s\n", input_filenames[i]);
            free(obj);
            return 1;
        }
        ax_vecPush(objs, obj);
        ax_execRegisterSymbols(&exec, obj);
    }

    // 2. Pre-load archives to avoid repeated disk I/O
    AxArchive* archives = calloc(ax_vecSize(lib_paths), sizeof(AxArchive));
    for (size_t i = 0; i < ax_vecSize(lib_paths); i++) {
        if (!axar_read_archive(lib_paths[i], &archives[i])) {
            fprintf(stderr, "Warning: Could not read archive %s\n", lib_paths[i]);
        }
        // printf("Loaded archive %s with %zu members and %zu symbols\n", lib_paths[i], archives[i].num_members, archives[i].num_symbols);
        // axar_list_symbols(&archives[i]); // Debug: list symbols in each archive
    }

    // 3. Recursive Library Resolution
    // We keep looping as long as we successfully resolve at least one symbol,
    // because that resolution might have introduced new undefined symbols.
    bool changed = true;
    while (changed) {
        changed = false;
        char** undefs = exec.undefined_sym_names;

        for (size_t i = 0; i < ax_vecSize(undefs); i++) {
            const char* needed = undefs[i];
            bool found_in_lib = false;

            for (size_t j = 0; j < ax_vecSize(lib_paths); j++) {
                size_t sym_idx = axar_find_symbol(&archives[j], needed);
                
                if (sym_idx != SIZE_MAX) {
                    size_t mem_idx = archives[j].symtab[sym_idx].member_idx;
                    AxObject* lib_obj = &archives[j].members[mem_idx];

                    // Avoid double-loading the same member if multiple symbols point to it
                    if (!lib_obj->is_linked) { 
                        // printf("Resolving %s -> %s(%zu)\n", needed, lib_paths[j], mem_idx);
                        
                        lib_obj->is_linked = true; // Mark to avoid duplicate inclusion
                        ax_execRegisterSymbols(&exec, lib_obj);
                        ax_vecPush(objs, lib_obj);
                        
                        changed = true;
                        found_in_lib = true;
                        break; 
                    }
                }
            }
            if (found_in_lib) break; // Restart undef scan because list changed
        }
    }

    // 4. Final Unresolved Check
    if (ax_vecSize(exec.undefined_sym_names) > 0) {
        for (size_t i = 0; i < ax_vecSize(exec.undefined_sym_names); i++) {
            printf("Error: Unresolved symbol %s\n", exec.undefined_sym_names[i]);
        }
        return 1;
    }

    // 5. Relocation and Write
    for (size_t i = 0; i < ax_vecSize(objs); i++) {
        ax_execCopyAndPatch(&exec, objs[i]);
    }

    ax_execWrite(&exec, output_filename);

    // Cleanup
    for (size_t i = 0; i < ax_vecSize(lib_paths); i++) axar_archive_free(&archives[i]);
    free(archives);
    // Note: lib_obj pointers in 'objs' are owned by the archives, 
    // only free the ones we malloc'd for input files.
    ax_execFree(&exec);
    
    return 0;
}