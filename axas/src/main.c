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

int main() {
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
