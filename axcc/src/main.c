#include <stdio.h>
#include "axas_lexer.h"
#include "axas_parser.h"
#include "axas_assembler.h"
#include "axas_object.h"
#include "axld_exec.h"

AxObject call_assembler(const char* input_code) {
    AxLexer* lexer = ax_lexerNew(input_code);
    AxObject obj;
    ax_objectInit(&obj);
    ax_assemble(&obj, lexer);
    ax_lexerFree(lexer);
    return obj;
}

void call_linker(const char* output_filename, AxObject* objs, size_t obj_count) {
    AxExecutable exec;
    ax_execInit(&exec);
    for (size_t i = 0; i < obj_count; i++) {
        ax_execRegisterSymbols(&exec, &objs[i]);
    }
    for (size_t i = 0; i < obj_count; i++) {
        ax_execCopyAndPatch(&exec, &objs[i]);
    }
    ax_execWrite(&exec, output_filename);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <input_files> [-o <output_file>]\n", argv[0]);
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
    size_t n = ax_vecSize(filenames);
    AxObject* objs = malloc(sizeof(AxObject) * n);
    size_t obj_count = 0;
    for (size_t i = 0; i < n; i++) {
        const char* filename = filenames[i];
        if (filename[strlen(filename) - 2] == '.' && filename[strlen(filename) - 1] == 'o') {
            printf("Loading object file: %s\n", filename);
            if (!ax_objectLoad(&objs[obj_count], filename)) {
                printf("Error: Failed to load object file %s.\n", filename);
                free(objs);
                return 1;
            }
            obj_count++;
        } else if (filename[strlen(filename) - 2] == '.' && (filename[strlen(filename) - 1] == 's' || filename[strlen(filename) - 1] == 'S')) {
            printf("Assembling source file: %s\n", filename);
            FILE* f = fopen(filename, "r");
            if (!f) {
                printf("Error: Failed to open source file %s.\n", filename);
                free(objs);
                return 1;
            }
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* input_code = malloc(fsize + 1);
            fread(input_code, 1, fsize, f);
            input_code[fsize] = '\0';
            fclose(f);
            objs[obj_count] = call_assembler(input_code);
            free(input_code);
            obj_count++;
        } else {
            printf("Error: Unrecognized file type for %s. Only .o and .s are supported (.c WIP).\n", filename);
            free(objs);
            return 1;
        }
    }

    call_linker(output_filename, objs, obj_count);
    return 0;
}