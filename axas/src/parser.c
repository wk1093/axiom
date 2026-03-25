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
        unit.directive.name = strdup(ax_lexerNextToken(l).str);
        AxToken value_token = ax_lexerNextToken(l);
        if (value_token.type == TOK_STRING) {
            unit.directive.value = strdup(value_token.str);
        } else if (value_token.type == TOK_IDENT) {
            unit.directive.value = strdup(value_token.str);
        } else if (value_token.type == TOK_DOT) {
            // Handle case like ".section .text"
            if (ax_lexerPeekToken(l).type == TOK_IDENT) {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), ".%s", ax_lexerNextToken(l).str);
                unit.directive.value = strdup(buffer);
            } else {
                // Handle error: expected identifier after dot
                printf("Error: Expected identifier after '.' in directive\n");
                unit.directive.value = NULL;
            }
        } else {
            // Handle error: expected string or identifier after directive
            printf("Error: Expected string or identifier after directive\n");
            unit.directive.value = NULL;
        }
        return unit;
    }
    strncpy(unit.instr.mnem, t.str, 15);
    unit.instr.mnem[15] = '\0';

    while (true) {
        AxToken arg = ax_lexerNextToken(l);
        if (arg.type == TOK_NEWLINE || arg.type == TOK_EOF) break;
        if (arg.type == TOK_COMMA) continue;

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
                unit.instr.is_post_index = true;
            } else if (ax_lexerPeekToken(l).type == TOK_COMMA) {
                ax_lexerNextToken(l); // consume comma
                unit.instr.is_pre_index = true;
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