#include "axas_assembler.h"

typedef enum {
    SECTION_TEXT,
    SECTION_DATA,
    SECTION_BSS,
    SECTION_RODATA
} AxSection;

void ax_resolveSpecialArgs(AxIrInstr* ir) {
    if (ir->opcode == OP_RET) {
        // For RET, if no register is specified, default to x30 (LR)
        if (ir->arg_count == 0) {
            ir->args[0] = (AxIrArg){ .type = ARG_REG, .reg_idx = 30, .is_64 = true };
            ir->arg_count = 1;
        }
    }
}

AxOpcode ax_resolveOpcode(AxParsedUnit* unit) {
    // This is fucked
    if (strcmp(unit->instr.mnem, "stp") == 0) {
        if (unit->instr.args[0].is_64 && unit->instr.args[1].is_64) {
            if (unit->instr.is_pre_index) {
                return OP_STP64_PRE;
            } else if (unit->instr.is_post_index) {
                return OP_STP64_POST;
            } else {
                return OP_STP_64;
            }
        } else if (!unit->instr.args[0].is_64 && !unit->instr.args[1].is_64) {
            if (unit->instr.is_pre_index) {
                return OP_STP_PRE;
            } else if (unit->instr.is_post_index) {
                return OP_STP_POST;
            } else {
                return OP_STP;
            }
        } else {
            // Handle error: mixed 32/64-bit registers in STP is invalid
            printf("Error: Mixed 32/64-bit registers in STP instruction\n");
            return OP_COUNT; // Invalid opcode
        }
    } else if (strcmp(unit->instr.mnem, "ldp") == 0) {
        if (unit->instr.args[0].is_64 && unit->instr.args[1].is_64) {
            if (unit->instr.is_pre_index) {
                return OP_LDP64_PRE;
            } else if (unit->instr.is_post_index) {
                return OP_LDP64_POST;
            } else {
                return OP_LDP_64;
            }
        } else if (!unit->instr.args[0].is_64 && !unit->instr.args[1].is_64) {
            if (unit->instr.is_pre_index) {
                return OP_LDP_PRE;
            } else if (unit->instr.is_post_index) {
                return OP_LDP_POST;
            } else {
                return OP_LDP;
            }
        } else {
            // Handle error: mixed 32/64-bit registers in LDP is invalid
            printf("Error: Mixed 32/64-bit registers in LDP instruction\n");
            return OP_COUNT; // Invalid opcode
        }
    } else if (strcmp(unit->instr.mnem, "mov") == 0) {
        if (unit->instr.args[1].type == ARG_IMM) {
            if (unit->instr.args[0].is_64) {
                return OP_MOVZ_64;
            } else {
                return OP_MOVZ_32;
            }
        } else if (unit->instr.args[1].type == ARG_REG) {
            bool has_sp = (unit->instr.args[0].reg_idx == 31) || (unit->instr.args[1].reg_idx == 31);
            if (has_sp) {
                if (unit->instr.args[0].is_64) {
                    // when we need sp, mov is actually add_immediate with constant 0
                    unit->instr.args[2] = (AxIrArg){ .type = ARG_IMM, .val = 0, .is_64 = true };
                    unit->instr.arg_count = 3;
                    return OP_ADD_IMM_64;
                }
            }
            if (unit->instr.args[0].is_64) {
                // when we encounter this, we have to add an extra xzr argument
                if (unit->instr.arg_count == 2) {
                    AxIrArg zero_reg = { .type = ARG_REG, .reg_idx = 31, .is_64 = true };
                    unit->instr.args[2] = unit->instr.args[1]; // move the source register to the new argument slot
                    unit->instr.args[1] = zero_reg; // set the source register to xzr
                    unit->instr.arg_count = 3;
                }
                return OP_ORR_REG_64;
            } else {
                // when we encounter this, we have to add an extra wzr argument
                if (unit->instr.arg_count == 2) {
                    AxIrArg zero_reg = { .type = ARG_REG, .reg_idx = 31, .is_64 = false };
                    unit->instr.args[2] = unit->instr.args[1]; // move the source register to the new argument slot
                    unit->instr.args[1] = zero_reg; // set the source register to wzr
                    unit->instr.arg_count = 3;
                }
                return OP_ORR_REG_32;
            }
        } else {
            // Handle error: unsupported MOV operand type
            printf("Error: Unsupported MOV operand type\n");
            return OP_COUNT; // Invalid opcode
        }
    } else if (strcmp(unit->instr.mnem, "ldr") == 0) {
        return unit->instr.args[0].is_64 ? OP_LDR_64 : OP_LDR;
    } else if (strcmp(unit->instr.mnem, "str") == 0) {
        return unit->instr.args[0].is_64 ? OP_STR_64 : OP_STR;
    } else if (strcmp(unit->instr.mnem, "ldrb") == 0) {
        return OP_LDRB;
    } else if (strcmp(unit->instr.mnem, "cbz") == 0) {
        return unit->instr.args[0].is_64 ? OP_CBZ_64 : OP_CBZ;
    } else if (strcmp(unit->instr.mnem, "sub") == 0) {
        if (unit->instr.args[2].type == ARG_IMM) {
            return unit->instr.args[0].is_64 ? OP_SUB_IMM_64 : OP_SUB_IMM_32;
        } else {
            return unit->instr.args[0].is_64 ? OP_SUB_64 : OP_SUB_32;
        }
    } else if (strcmp(unit->instr.mnem, "subs") == 0) {
        if (unit->instr.args[2].type == ARG_IMM) {
            return unit->instr.args[0].is_64 ? OP_SUBS_IMM_64 : OP_SUBS_IMM_32;
        } else {
            return unit->instr.args[0].is_64 ? OP_SUBS_64 : OP_SUBS_32;
        }
    } else if (strcmp(unit->instr.mnem, "add") == 0) {
        if (unit->instr.args[2].type == ARG_IMM) {
            return unit->instr.args[0].is_64 ? OP_ADD_IMM_64 : OP_ADD_IMM_32;
        } else {
            return unit->instr.args[0].is_64 ? OP_ADD_64 : OP_ADD_32;
        }
    } else if (strcmp(unit->instr.mnem, "adds") == 0) {
        if (unit->instr.args[2].type == ARG_IMM) {
            return unit->instr.args[0].is_64 ? OP_ADDS_IMM_64 : OP_ADDS_IMM_32;
        } else {
            return unit->instr.args[0].is_64 ? OP_ADDS_64 : OP_ADDS_32;
        }
    } else if (strcmp(unit->instr.mnem, "cmp") == 0) {
        if (unit->instr.args[1].type == ARG_IMM) {
            return unit->instr.args[0].is_64 ? OP_SUBS_IMM_64 : OP_SUBS_IMM_32; // CMP is actually a SUBS that updates flags
        } else {
            return unit->instr.args[0].is_64 ? OP_SUBS_64 : OP_SUBS_32; // CMP is actually a SUBS that updates flags
        }
    } else if (strcmp(unit->instr.mnem, "bne") == 0) {
        return OP_BNE;
    }
    else {
        // For other instructions, we can do a simple linear search
        for (int i = 0; i < OP_COUNT; i++) {
            if (strcmp(unit->instr.mnem, ax_opcodeToMnem((AxOpcode)i)) == 0) {
                return (AxOpcode)i;
            }
        }
        // Handle error: unknown instruction mnemonic
        printf("Error: Unknown instruction mnemonic '%s'\n", unit->instr.mnem);
        return OP_COUNT; // Invalid opcode
    }
}

void ax_assemble(AxObject* obj, AxLexer* lexer) {
    AxSection current_section = SECTION_DATA;
    while (true) {
        AxParsedUnit unit = ax_parseUnit(lexer);

        if (unit.type == UNIT_EOF) break;

        switch(unit.type) {
            case UNIT_LABEL:
                if (current_section == SECTION_TEXT) {
                    ax_objectDefineCodeLabel(obj, unit.label);
                } else {
                    ax_objectDefineDataLabel(obj, unit.label);
                }
                break;
            case UNIT_DIRECTIVE: {
                uint8_t* cur_sec = obj->data;
                AxSection sec_before = current_section;
                switch (current_section) {
                    case SECTION_TEXT: cur_sec = (uint8_t*)obj->text; break;
                    case SECTION_DATA: cur_sec = obj->data; break;
                    case SECTION_BSS: cur_sec = obj->data; break;
                    case SECTION_RODATA: cur_sec = obj->data; break;
                    default:
                        printf("Error: Invalid section\n");
                        cur_sec = obj->data; // Fallback to data
                        break;
                }
                if (strcmp(unit.directive.name, "section") == 0) {
                    if (strcmp(unit.directive.value, ".text") == 0) {
                        current_section = SECTION_TEXT;
                    } else if (strcmp(unit.directive.value, ".data") == 0) {
                        current_section = SECTION_DATA;
                    } else if (strcmp(unit.directive.value, ".bss") == 0) {
                        current_section = SECTION_BSS;
                    } else if (strcmp(unit.directive.value, ".rodata") == 0) {
                        current_section = SECTION_RODATA;
                    } else {
                        printf("Error: Unknown section '%s'\n", unit.directive.value);
                    }
                } else if (strcmp(unit.directive.name, "asciz") == 0) {
                    // Handle .asciz directive (null-terminated string)
                    size_t str_len = strlen(unit.directive.value);
                    for (size_t i = 0; i < str_len; i++) {
                        ax_vecPush(cur_sec, unit.directive.value[i]);
                    }
                    ax_vecPush(cur_sec, '\0'); // Null terminator
                } else if (strcmp(unit.directive.name, "byte") == 0) {
                    uint8_t val = (uint8_t)strtoul(unit.directive.value, NULL, 0);
                    ax_vecPush(cur_sec, val);
                } else if (strcmp(unit.directive.name, "2byte") == 0 || strcmp(unit.directive.name, "hword") == 0) {
                    // Handle .2byte/.hword directive (16-bit value)
                    uint16_t val = (uint16_t)strtoul(unit.directive.value, NULL, 0);
                    ax_vecPush(cur_sec, val & 0xFF);
                    ax_vecPush(cur_sec, (val >> 8) & 0xFF);
                } else if (strcmp(unit.directive.name, "4byte") == 0 || strcmp(unit.directive.name, "word") == 0) {
                    // Handle .4byte/.word directive (32-bit value)
                    uint32_t val = (uint32_t)strtoul(unit.directive.value, NULL, 0);
                    ax_vecPush(cur_sec, val & 0xFF);
                    ax_vecPush(cur_sec, (val >> 8) & 0xFF);
                    ax_vecPush(cur_sec, (val >> 16) & 0xFF);
                    ax_vecPush(cur_sec, (val >> 24) & 0xFF);
                } else if (strcmp(unit.directive.name, "8byte") == 0 || strcmp(unit.directive.name, "quad") == 0) {
                    // Handle .8byte/.quad directive (64-bit value)
                    uint64_t val = strtoull(unit.directive.value, NULL, 0);
                    for (int i = 0; i < 8; i++) {
                        ax_vecPush(cur_sec, (val >> (i * 8)) & 0xFF);
                    }
                } else if (strcmp(unit.directive.name, "align") == 0) {
                    // Handle .align directive
                    size_t alignment = strtoul(unit.directive.value, NULL, 0);
                    size_t padding = (alignment - (ax_vecSize(cur_sec) % alignment)) % alignment;
                    for (size_t i = 0; i < padding; i++) {
                        ax_vecPush(cur_sec, 0); // Pad with zeros
                    }
                } else if (strcmp(unit.directive.name, "global") == 0) {
                    ax_objectDeclareGlobal(obj, unit.directive.value);
                } else if (strcmp(unit.directive.name, "extern") == 0) {
                    ax_objectDeclareExternal(obj, unit.directive.value);
                }else if (strcmp(unit.directive.name, "size") == 0) {
                    ax_objectSetSymbolSize(obj, unit.directive.value);
                } else if (strcmp(unit.directive.name, "type") == 0) {
                    ax_objectSetSymbolType(obj, unit.directive.value);
                } else if (strcmp(unit.directive.name, "string") == 0) {
                    // Handle .string directive (non-null-terminated string)
                    size_t str_len = strlen(unit.directive.value);
                    for (size_t i = 0; i < str_len; i++) {
                        ax_vecPush(cur_sec, unit.directive.value[i]);
                    }
                } else {
                    printf("Error: Unknown directive '%s'\n", unit.directive.name);
                }
                // WE NEED to assign cursec back to the section vector, becuase push might have changed the pointer
                switch (sec_before) {
                    case SECTION_TEXT: obj->text = (uint32_t*)cur_sec; break;
                    case SECTION_DATA: obj->data = cur_sec; break;
                    case SECTION_BSS: obj->data = cur_sec; break;
                    case SECTION_RODATA: obj->data = cur_sec; break;
                    default: break; // Should never happen
                }
                break;
            }
            case UNIT_INSTR:
                AxIrInstr ir = {0};
                ir.opcode = ax_resolveOpcode(&unit);
                
                ir.arg_count = unit.instr.arg_count;
                memcpy(ir.args, unit.instr.args, sizeof(AxIrArg) * ir.arg_count);
                ax_resolveSpecialArgs(&ir);
                ax_objectEmit(obj, &ir);
                break;
            case UNIT_EOF:
                break;
            default:
                printf("Error: Unknown unit type\n");
                break;
        }
    }
}