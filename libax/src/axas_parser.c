#include "axas_parser.h"

uint8_t regToIdx(const char* reg) {
    if (strcmp(reg, "sp") == 0) return 31;
    if (strcmp(reg, "xzr") == 0) return 31;
    if (strcmp(reg, "wzr") == 0) return 31;
    if ((reg[0] == 'x' || reg[0] == 'w') && atoi(&reg[1]) >= 0 && atoi(&reg[1]) <= 30) {
        return atoi(&reg[1]);
    }
    // Handle error: invalid register
    printf("Error: Invalid register '%s'\n", reg);
    return 0;
}

bool regIs64(const char* reg) {
    if (strcmp(reg, "sp") == 0) return true;
    if (strcmp(reg, "xzr") == 0) return true;
    if (reg[0] == 'x' && atoi(&reg[1]) >= 0 && atoi(&reg[1]) <= 30) {
        return true;
    }
    return false;
}

AxParsedUnit ax_parseUnit(AxLexer* l) {
    AxParsedUnit unit = {0};
    AxToken t = ax_lexerNextToken(l);

    while (t.type == TOK_NEWLINE) {
        t = ax_lexerNextToken(l);
    }
    if (t.type == TOK_EOF) { unit.type = UNIT_EOF; return unit; }

    if (t.type == TOK_IDENT && ax_lexerPeekToken(l).type == TOK_COLON) {
        unit.type = UNIT_LABEL;
        unit.label = strdup(t.str);
        ax_lexerNextToken(l); // consume colon
        return unit;
    }

    unit.type = (t.type == TOK_DOT) ? UNIT_DIRECTIVE : UNIT_INSTR;
    if (unit.type == UNIT_DIRECTIVE) {
        AxToken dir_name_tok = ax_lexerNextToken(l);
        if (dir_name_tok.type != TOK_IDENT || dir_name_tok.str == NULL) {
            printf("Error: Expected directive name after '.'\n");;
            if (dir_name_tok.type != TOK_NEWLINE && dir_name_tok.type != TOK_EOF) {
                while (ax_lexerPeek(l) != '\n' && ax_lexerPeek(l) != '\0') ax_lexerNext(l);
            }
            unit.type = UNIT_EOF;
            return unit;
        }
        unit.directive.name = strdup(dir_name_tok.str);
        char value_buf[512] = {0};
        ax_lexerSkipWhitespace(l);
        size_t i = 0;
        while (ax_lexerPeek(l) != '\n' && ax_lexerPeek(l) != '\0' && i < sizeof(value_buf) - 1) {
            if (ax_lexerPeek(l) == '"') {
                // our lexer has code to handle string literals, so we can just use that instead of duplicating code
                AxToken strToken = ax_lexerNextToken(l);
                if (strToken.type == TOK_STRING) {
                    // Append the string literal to the value buffer
                    size_t str_len = strlen(strToken.str);
                    if (i + str_len < sizeof(value_buf)) {
                        memcpy(&value_buf[i], strToken.str, str_len);
                        i += str_len;
                    } else {
                        printf("Error: Directive value too long when appending string literal\n");
                        break;
                    }
                } else {
                    printf("Error: Expected string literal (wtf)\n");
                    break;
                }
            } else {
                value_buf[i++] = ax_lexerPeek(l);
                ax_lexerNext(l);
            }
        }
        value_buf[i] = '\0';
        
        unit.directive.value = strdup(value_buf);
        return unit;
    }
    if ((t.type != TOK_IDENT && t.type != TOK_REG) || t.str == NULL) {
        printf("Error: Unexpected token where instruction mnemonic expected\n");
        while (ax_lexerPeek(l) != '\n' && ax_lexerPeek(l) != '\0') ax_lexerNext(l);
        unit.type = UNIT_EOF;
        return unit;
    }
    strncpy(unit.instr.mnem, t.str, 15);
    unit.instr.mnem[15] = '\0';

    while (true) {
        AxToken arg = ax_lexerNextToken(l);
        if (arg.type == TOK_NEWLINE || arg.type == TOK_EOF) break;
        if (arg.type == TOK_COMMA) continue;

        if (unit.instr.arg_count >= 3) {
            printf("Error: Too many operands for instruction '%s' (max 3)\n", unit.instr.mnem);
            while (arg.type != TOK_NEWLINE && arg.type != TOK_EOF) arg = ax_lexerNextToken(l);
            break;
        }
        AxIrArg* current = &unit.instr.args[unit.instr.arg_count++];
        if (arg.type == TOK_REG) {
            int idx = regToIdx(arg.str);
            bool is_64 = regIs64(arg.str);
            *current = (AxIrArg){ .type = ARG_REG, .reg_idx = idx, .is_64 = is_64 };
        } else if (arg.type == TOK_IMM) {
            *current = (AxIrArg){ .type = ARG_IMM, .val = arg.imm };
        } else if (arg.type == TOK_IDENT) {
            *current = (AxIrArg){ .type = ARG_SYM, .label = strdup(arg.str) };
        } else if (arg.type == TOK_LBRACK) {
            AxToken base_reg = ax_lexerNextToken(l);
            if (base_reg.type != TOK_REG) {
                // Handle error: expected register after '['
                printf("Error: Expected register after '[' in memory operand\n");
                unit.instr.arg_count--; // undo the pre-increment so the slot isn't left dirty
                continue;
            }
            current->type = ARG_REG_IMM;
            current->reg_idx = regToIdx(base_reg.str);
            current->is_64 = regIs64(base_reg.str);
            current->val = 0; // Default offset

            AxToken next = ax_lexerPeekToken(l);
            if (next.type == TOK_COMMA) {
                ax_lexerNextToken(l); // consume comma
                AxToken imm = ax_lexerNextToken(l);
                if (imm.type == TOK_IMM) {
                    current->val = imm.imm;
                } else {
                    // Handle error: expected immediate after comma
                    printf("Error: Expected immediate after comma in memory operand\n");
                }
            }
            // TODO: lsl shit
            if (ax_lexerNextToken(l).type != TOK_RBRACK) {
                // Handle error: expected closing bracket
                printf("Error: Expected closing bracket in memory operand\n");
            }

            if (ax_lexerPeekToken(l).type == TOK_BANG) {
                ax_lexerNextToken(l); // consume '!'
                unit.instr.is_pre_index = true;
            } else if (ax_lexerPeekToken(l).type == TOK_COMMA) {
                ax_lexerNextToken(l); // consume comma
                unit.instr.is_post_index = true;
                AxToken imm = ax_lexerNextToken(l);
                if (imm.type == TOK_IMM) {
                    current->val = imm.imm;
                } else {
                    // Handle error: expected immediate after comma
                    printf("Error: Expected immediate after comma in post-indexed memory operand\n");
                }
            }
        }
    }
    return unit;
}

void ax_debugUnit(AxParsedUnit unit) {
    switch (unit.type) {
        case UNIT_LABEL:
            printf("Label: %s\n", unit.label);
            break;
        case UNIT_INSTR:
            printf("Instr: %s, Args: %d, PreIdx: %d, PostIdx: %d\n", unit.instr.mnem, unit.instr.arg_count, unit.instr.is_pre_index, unit.instr.is_post_index);
            for (int i = 0; i < unit.instr.arg_count; i++) {
                AxIrArg arg = unit.instr.args[i];
                if (arg.type == ARG_REG) {
                    printf("  Arg%d: REG x%d%s\n", i+1, arg.reg_idx, arg.is_64 ? " (64-bit)" : "");
                } else if (arg.type == ARG_IMM) {
                    printf("  Arg%d: IMM #%lu\n", i+1, arg.val);
                } else if (arg.type == ARG_SYM) {
                    printf("  Arg%d: SYM %s\n", i+1, arg.label);
                } else if (arg.type == ARG_REG_IMM) {
                    printf("  Arg%d: REG x%d + #%ld%s\n", i+1, arg.reg_idx, (int64_t)arg.val, arg.is_64 ? " (64-bit)" : "");
                }
            }
            break;
        case UNIT_DIRECTIVE:
            printf("Directive: %s %s\n", unit.directive.name, unit.directive.value);
            break;
        case UNIT_EOF:
            printf("End of File\n");
            break;
    }
}