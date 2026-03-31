#include "axas_lexer.h"

AxLexer *ax_lexerNew(const char *src) {
    AxLexer *lexer = malloc(sizeof(AxLexer));
    lexer->src = src;
    lexer->pos = 0;
    return lexer;
}

void ax_lexerFree(AxLexer *lexer) {
    free(lexer);
}

char ax_lexerPeek(AxLexer *lexer) {
    return lexer->src[lexer->pos];
}

char ax_lexerNext(AxLexer *lexer) {
    return lexer->src[lexer->pos++];
}

void ax_lexerSkipWhitespace(AxLexer *lexer) {
    while (ax_lexerPeek(lexer) == ' ' || ax_lexerPeek(lexer) == '\t') {
        ax_lexerNext(lexer);
    }
}

AxToken ax_lexerNextToken(AxLexer* l) {
bool encountered_newline = false;

    while (true) {
        ax_lexerSkipWhitespace(l); // Skip spaces/tabs
        char c = ax_lexerPeek(l);

        // 1. Handle Comments
        if (c == '/' && l->src[l->pos + 1] == '/') {
            while (ax_lexerPeek(l) != '\n' && ax_lexerPeek(l) != '\0') {
                ax_lexerNext(l);
            }
            // After the comment, we loop again. 
            // The newline at the end of the comment will be caught by the '\n' case.
            continue;
        }

        // 2. Handle Newlines (Collapsing multiple into one)
        if (c == '\n') {
            encountered_newline = true;
            ax_lexerNext(l);
            continue; // Keep looping to catch more newlines or comments
        }

        // 3. If we hit anything else, stop skipping
        break;
    }
    char c = ax_lexerPeek(l);

    if (c == '\0') {
        return (AxToken){.type = TOK_EOF};
    }

    if (encountered_newline) {
        return (AxToken){.type = TOK_NEWLINE};
    }

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        // Identifier or register
        size_t start = l->pos;
        while ((ax_lexerPeek(l) >= 'a' && ax_lexerPeek(l) <= 'z') ||
               (ax_lexerPeek(l) >= 'A' && ax_lexerPeek(l) <= 'Z') ||
               (ax_lexerPeek(l) >= '0' && ax_lexerPeek(l) <= '9') ||
                ax_lexerPeek(l) == '_') {
            ax_lexerNext(l);
        }
        size_t len = l->pos - start;
        char *str = strndup(&l->src[start], len);
        if ((str[0] == 'x' || str[0] == 'w') && len > 1 && str[1] >= '0' && str[1] <= '9') {
            // Register
            return (AxToken){.type = TOK_REG, .str = str};
        } else if ((strcmp(str, "sp") == 0) || (strcmp(str, "xzr") == 0) || (strcmp(str, "wzr") == 0)) {
            // Special registers
            return (AxToken){.type = TOK_REG, .str = str};
        } else {
            // Identifier
            return (AxToken){.type = TOK_IDENT, .str = str};
        }
    }
    // we also need to support immediates with # prefix, e.g. #16

    if ((c >= '0' && c <= '9') || c == '-' || c == '#') {
        // Immediate
        if (c == '#') {
            ax_lexerNext(l);
            c = ax_lexerPeek(l);
        }
        size_t start = l->pos;
        if (c == '-') {
            ax_lexerNext(l);
        }
        while (ax_lexerPeek(l) >= '0' && ax_lexerPeek(l) <= '9') {
            ax_lexerNext(l);
        }
        size_t len = l->pos - start;
        if (len == 1 && l->src[start] == '-') {
            // Handle the case where we have a standalone '-' which is not a valid immediate
            return (AxToken){.type = TOK_DASH};
        }
        char *numStr = strndup(&l->src[start], len);
        int imm = atoi(numStr);
        free(numStr);
        return (AxToken){.type = TOK_IMM, .imm = imm};
    }

    // String literals
    if (c == '"') {
        ax_lexerNext(l); // consume opening quote
        size_t start = l->pos;
        while (ax_lexerPeek(l) != '"' && ax_lexerPeek(l) != '\0') {
            if (ax_lexerPeek(l) == '\\') {
                ax_lexerNext(l); // skip escape character
                if (ax_lexerPeek(l) != '\0') {
                    ax_lexerNext(l); // skip escaped character
                }
            } else {
                ax_lexerNext(l);
            }
        }
        if (ax_lexerPeek(l) == '"') {
            size_t len = l->pos - start;
            char *str = strndup(&l->src[start], len);
            ax_lexerNext(l); // consume closing quote
            // escape sequences
            char *escapedStr = malloc(len + 1);
            size_t j = 0;
            for (size_t i = 0; i < len; i++) {
                if (str[i] == '\\' && i + 1 < len) {
                    i++;
                    switch (str[i]) {
                        case 'n': escapedStr[j++] = '\n'; break;
                        case 't': escapedStr[j++] = '\t'; break;
                        case '\\': escapedStr[j++] = '\\'; break;
                        case '"': escapedStr[j++] = '"'; break;
                        default: escapedStr[j++] = str[i]; break;
                    }
                } else {
                    escapedStr[j++] = str[i];
                }
            }
            escapedStr[j] = '\0';
            free(str);
            return (AxToken){.type = TOK_STRING, .str = escapedStr};
        } else {
            return (AxToken){.type = TOK_ERROR};
        }
    }

    // Single-character tokens
    switch (c) {
        case ',':
            ax_lexerNext(l);
            return (AxToken){.type = TOK_COMMA};
        case '[':
            ax_lexerNext(l);
            return (AxToken){.type = TOK_LBRACK};
        case ']':
            ax_lexerNext(l);
            return (AxToken){.type = TOK_RBRACK};
        case '!':
            ax_lexerNext(l);
            return (AxToken){.type = TOK_BANG};
        case ':':
            ax_lexerNext(l);
            return (AxToken){.type = TOK_COLON};
        case '.':
            ax_lexerNext(l);
            return (AxToken){.type = TOK_DOT};
        case '%':
            ax_lexerNext(l);
            return (AxToken){.type = TOK_PERCENT};
        default:
            ax_lexerNext(l);
            return (AxToken){.type = TOK_ERROR};
    }
}

AxToken ax_lexerPeekToken(AxLexer* l) {
    size_t savedPos = l->pos;
    AxToken token = ax_lexerNextToken(l);
    l->pos = savedPos;
    return token;
}

void ax_printToken(AxToken token) {
    switch (token.type) {
        case TOK_IDENT:
            printf("IDENT(%s)", token.str);
            break;
        case TOK_REG:
            printf("REG(%s)", token.str);
            break;
        case TOK_IMM:
            printf("IMM(%d)", token.imm);
            break;
        case TOK_COMMA:
            printf("COMMA");
            break;
        case TOK_LBRACK:
            printf("LBRACK");
            break;
        case TOK_RBRACK:
            printf("RBRACK");
            break;
        case TOK_BANG:
            printf("BANG");
            break;
        case TOK_COLON:
            printf("COLON");
            break;
        case TOK_EOF:
            printf("EOF");
            break;
        case TOK_ERROR:
            printf("ERROR");
            break;
        case TOK_NEWLINE:
            printf("NEWLINE");
            break;
        case TOK_DOT:
            printf("DOT");
            break;
        case TOK_STRING:
            printf("STRING(\"%s\")", token.str);
            break;
    }
}