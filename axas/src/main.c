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

int test() {
    const char* contents = ".section .data\n"
                           "prompt: .asciz \"Hello, World!\\n\"\n"
                           "\n"
                           ".section .text\n"
                           ".global main\n"
                           "main:\n"
                           "    stp x29, x30, [sp, #-16]!\n"
                           "    adr x0, prompt\n"
                           "    bl puts\n"
                           "    movz x0, #42\n"
                           "    ldp x29, x30, [sp], #16\n"
                           "    ret";
    AxLexer *lexer = ax_lexerNew(contents);
    AxToken token;
    do {
        token = ax_lexerNextToken(lexer);
        ax_printToken(token);
        if (token.type != TOK_NEWLINE) {
            printf(" ");
        } else {
            printf("\n");
        }
    } while (token.type != TOK_EOF && token.type != TOK_ERROR);
    printf("\n");
    lexer->pos = 0;
    AxParsedUnit unit;
    do {
        unit = ax_parseUnit(lexer);
        ax_debugUnit(unit);
    } while (lexer->pos < strlen(contents) && unit.type != UNIT_EOF);

    lexer->pos = 0;
    
    AxObject obj;
    ax_objectInit(&obj);
    ax_assemble(&obj, lexer);
    ax_objectWrite(&obj, "bin/output.o");
    printf("Axiom: Generated bin/output.o with %zu instructions.\n", ax_vecSize(obj.text));
    ax_objectFree(&obj);

    ax_lexerFree(lexer);
    return 0;
}

// New assembler entry point with command line
// Options:
// axas <input_file> [-o <output_file>] [--lex] [--parse]
// If --lex is provided, print tokens and exit
// If --parse is provided, print parsed units and exit
int main(int argc, char** argv) {
    if (argc == 1) {
        printf("Usage: %s <input_file> [-o <output_file>] [--lex] [--parse]\n", argv[0]);
        printf("Options:\n");
        printf("  --lex       Print tokens and exit\n");
        printf("  --parse     Print parsed units and exit\n");
        printf("  -o <file>   Specify output file (default is input file with .o extension)\n");
        return 1;
    }

    const char* filename = NULL;
    bool do_lex = false;
    bool do_parse = false;
    char* output_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lex") == 0) {
            do_lex = true;
        } else if (strcmp(argv[i], "--parse") == 0) {
            do_parse = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        printf("Error: No input file specified.\n");
        return 1;
    }

    if (do_lex && do_parse) {
        printf("Error: Cannot specify both --lex and --parse.\n");
        return 1;
    }

    bool output_alloc = false;

    if (!output_filename) {
        output_alloc = true;
        // Default output filename is input filename with .o extension
        size_t len = strlen(filename);
        output_filename = malloc(len + 3); // for .o and null terminator
        strncpy(output_filename, filename, len+1);
        char* dot = strrchr(output_filename, '.');
        if (dot) {
            *dot = '\0'; // remove existing extension
        }
        strcat(output_filename, ".o");
    }
    
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("Failed to open input file");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    size_t filesize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* contents = malloc(filesize + 1);
    fread(contents, 1, filesize, f);
    contents[filesize] = '\0';
    fclose(f);

    AxLexer *lexer = ax_lexerNew(contents);
    if (do_lex) {
        AxToken token;
        do {
            token = ax_lexerNextToken(lexer);
            ax_printToken(token);
            if (token.type != TOK_NEWLINE) {
                printf(" ");
            } else {
                printf("\n");
            }
        } while (token.type != TOK_EOF && token.type != TOK_ERROR);
        printf("\n");
        ax_lexerFree(lexer);
        free(contents);
        return 0;
    }

    if (do_parse) {
        AxParsedUnit unit;
        do {
            unit = ax_parseUnit(lexer);
            ax_debugUnit(unit);
        } while (lexer->pos < strlen(contents) && unit.type != UNIT_EOF);
        ax_lexerFree(lexer);
        free(contents);
        return 0;
    }

    AxObject obj;
    ax_objectInit(&obj);
    ax_assemble(&obj, lexer);
    ax_objectWrite(&obj, output_filename);
    ax_printObjectInfo(&obj);
    printf("Axiom: Generated %s with %zu instructions.\n", output_filename, ax_vecSize(obj.text));
    ax_objectFree(&obj);
    ax_lexerFree(lexer);
    free(contents);
    if (output_alloc) {
        free((void*)output_filename);
    }

    return 0;
}
