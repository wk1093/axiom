#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <elf.h>
#include <string.h>
#include <ax_vec.h>
#include <ax_ir.h>
#include "axas_object.h"
#include "axas_lexer.h"
#include "axas_parser.h"
#include "axas_assembler.h"

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("Usage: %s <input_file>... [-o <output_file>]\n", argv[0]);
        printf("Options:\n");
        printf("  -o <file>   Specify output file (default: first input with .o extension)\n");
        return 1;
    }

    const char** input_files = malloc(argc * sizeof(char*));
    int input_count = 0;
    char* output_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        } else {
            input_files[input_count++] = argv[i];
        }
    }

    if (input_count == 0) {
        printf("Error: No input files specified.\n");
        free(input_files);
        return 1;
    }

    bool output_alloc = false;

    if (!output_filename) {
        output_alloc = true;
        // Default: first input filename with .o extension
        size_t len = strlen(input_files[0]);
        output_filename = malloc(len + 3);
        strncpy(output_filename, input_files[0], len + 1);
        char* dot = strrchr(output_filename, '.');
        if (dot) {
            *dot = '\0';
        }
        strcat(output_filename, ".o");
    }

    AxObject obj;
    ax_objectInit(&obj);

    for (int i = 0; i < input_count; i++) {
        FILE* f = fopen(input_files[i], "r");
        if (!f) {
            fprintf(stderr, "Error: Failed to open '%s': ", input_files[i]);
            perror(NULL);
            ax_objectFree(&obj);
            free(input_files);
            if (output_alloc) free(output_filename);
            return 1;
        }

        fseek(f, 0, SEEK_END);
        size_t filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* contents = malloc(filesize + 1);
        fread(contents, 1, filesize, f);
        contents[filesize] = '\0';
        fclose(f);

        AxLexer* lexer = ax_lexerNew(contents);
        ax_assemble(&obj, lexer);
        ax_lexerFree(lexer);
        free(contents);
    }

    ax_objectWrite(&obj, output_filename);
    // printf("Axiom: Generated %s with %zu instructions from %d file(s).\n",
    //        output_filename, ax_vecSize(obj.text), input_count);
    ax_objectFree(&obj);
    free(input_files);
    if (output_alloc) {
        free(output_filename);
    }

    return 0;
}
