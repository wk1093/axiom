#include "axas_assembler.h"

typedef enum {
    SECTION_TEXT,
    SECTION_DATA,
    SECTION_BSS,
    SECTION_RODATA
} AxSection;

void ax_resolveSpecialArgs(AxIrInstr* ir, AxParsedUnit unit) {
    if (ir->opcode == OP_RET) {
        // For RET, if no register is specified, default to x30 (LR)
        if (ir->arg_count == 0) {
            ir->args[0] = (AxIrArg){ .type = ARG_REG, .reg_idx = 30, .is_64 = true };
            ir->arg_count = 1;
        }
    }
}

AxOpcode ax_resolveOpcode(AxParsedUnit unit) {
    // This is fucked
    if (strcmp(unit.instr.mnem, "stp") == 0) {
        if (unit.instr.args[0].is_64 && unit.instr.args[1].is_64) {
            if (unit.instr.is_pre_index) {
                return OP_STP64_PRE;
            } else if (unit.instr.is_post_index) {
                return OP_STP64_POST;
            } else {
                return OP_STP_64;
            }
        } else if (!unit.instr.args[0].is_64 && !unit.instr.args[1].is_64) {
            if (unit.instr.is_pre_index) {
                return OP_STP_PRE;
            } else if (unit.instr.is_post_index) {
                return OP_STP_POST;
            } else {
                return OP_STP;
            }
        } else {
            // Handle error: mixed 32/64-bit registers in STP is invalid
            printf("Error: Mixed 32/64-bit registers in STP instruction\n");
            return OP_COUNT; // Invalid opcode
        }
    } else if (strcmp(unit.instr.mnem, "ldp") == 0) {
        if (unit.instr.args[0].is_64 && unit.instr.args[1].is_64) {
            if (unit.instr.is_pre_index) {
                return OP_LDP64_PRE;
            } else if (unit.instr.is_post_index) {
                return OP_LDP64_POST;
            } else {
                return OP_LDP_64;
            }
        } else if (!unit.instr.args[0].is_64 && !unit.instr.args[1].is_64) {
            if (unit.instr.is_pre_index) {
                return OP_LDP_PRE;
            } else if (unit.instr.is_post_index) {
                return OP_LDP_POST;
            } else {
                return OP_LDP;
            }
        } else {
            // Handle error: mixed 32/64-bit registers in LDP is invalid
            printf("Error: Mixed 32/64-bit registers in LDP instruction\n");
            return OP_COUNT; // Invalid opcode
        }
    } else if (strcmp(unit.instr.mnem, "mov") == 0) {
        if (unit.instr.args[1].type == ARG_IMM) {
            if (unit.instr.args[0].is_64) {
                return OP_MOVZ_64;
            } else {
                return OP_MOVZ_32;
            }
        } else if (unit.instr.args[1].type == ARG_REG) {
            if (unit.instr.args[0].is_64) {
                // when we encounter this, we have to add an extra xzr argument
                if (unit.instr.arg_count == 2) {
                    unit.instr.args[2] = (AxIrArg){ .type = ARG_REG, .reg_idx = 31, .is_64 = true };
                    unit.instr.arg_count = 3;
                }
                return OP_ORR_REG_64;
            } else {
                // when we encounter this, we have to add an extra wzr argument
                if (unit.instr.arg_count == 2) {
                    unit.instr.args[2] = (AxIrArg){ .type = ARG_REG, .reg_idx = 31, .is_64 = false };
                    unit.instr.arg_count = 3;
                }
                return OP_ORR_REG_32;
            }
        } else {
            // Handle error: unsupported MOV operand type
            printf("Error: Unsupported MOV operand type\n");
            return OP_COUNT; // Invalid opcode
        }
    } else {
        // For other instructions, we can do a simple linear search
        for (int i = 0; i < OP_COUNT; i++) {
            if (strcmp(unit.instr.mnem, ax_opcodeToMnem((AxOpcode)i)) == 0) {
                return (AxOpcode)i;
            }
        }
        // Handle error: unknown instruction mnemonic
        printf("Error: Unknown instruction mnemonic '%s'\n", unit.instr.mnem);
        return OP_COUNT; // Invalid opcode
    }
}

void ax_assemble(AxObject* obj, AxLexer* lexer) {
    AxSection current_section = SECTION_DATA; // Default to .data for simplicity
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
            case UNIT_DIRECTIVE:
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
                        ax_vecPush(obj->data, unit.directive.value[i]);
                    }
                    ax_vecPush(obj->data, '\0'); // Null terminator
                } else {
                    printf("Error: Unknown directive '%s'\n", unit.directive.name);
                }
                break;
            case UNIT_INSTR:
                AxIrInstr ir = {0};
                ir.opcode = ax_resolveOpcode(unit);

                ir.arg_count = unit.instr.arg_count;
                memcpy(ir.args, unit.instr.args, sizeof(AxIrArg) * ir.arg_count);
                ax_resolveSpecialArgs(&ir, unit);
                ax_objectEmit(obj, &ir);
                break;
        }
    }
}