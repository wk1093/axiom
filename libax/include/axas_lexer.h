#ifndef AXAS_LEXER_H
#define AXAS_LEXER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOK_IDENT,   // adr, movz, main, puts
    TOK_REG,     // x0, sp, x29
    TOK_IMM,     // -16, 40
    TOK_COMMA,   // ,
    TOK_LBRACK,  // [
    TOK_RBRACK,  // ]
    TOK_BANG,    // ! (for pre-index)
    TOK_COLON,   // : (for label definition)
    TOK_NEWLINE,
    TOK_DOT,     // . (for directives and section names)
    TOK_STRING,  // "Hello, world!\n"
    TOK_EOF,
    TOK_ERROR
} AxTokType;

typedef struct {
    AxTokType type;
    char *str;  // for TOK_IDENT, TOK_REG, and TOK_STRING
    int imm;    // for TOK_IMM
} AxToken;

typedef struct {
    const char *src;
    size_t pos;
} AxLexer;

AxLexer *ax_lexerNew(const char *src);
void ax_lexerFree(AxLexer *lexer);
AxToken ax_lexerNextToken(AxLexer* l);
void ax_printToken(AxToken token);
AxToken ax_lexerPeekToken(AxLexer* l);

#endif
