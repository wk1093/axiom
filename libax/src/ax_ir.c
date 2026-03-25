#include "ax_ir.h"

static const char* ax_mnemonics[] = {
    #define X(name, mnem, arg_count, base_opcode, flags, encoder) [OP_##name] = mnem,
    #include "ax_instr_def.h"
    #undef X
};

static const uint32_t ax_flags[] = {
    #define X(name, mnem, arg_count, base_opcode, flags, encoder) [OP_##name] = flags,
    #include "ax_instr_def.h"
    #undef X
};

uint32_t ax_ir_to_bytecode(AxIrInstr* instr) {
    switch (instr->opcode) {
        #define X(name, mnem, arg_count, base_opcode, flags, encoder) \
            case OP_##name: return encoder(base_opcode, instr);
        #include "ax_instr_def.h"
        #undef X
        default: return 0;
    }
}

bool ax_ir_to_asm(AxIrInstr* instr, char* buf, size_t sz) {
    const char* mnem = ax_mnemonics[instr->opcode];
    uint32_t flags = ax_flags[instr->opcode];
    snprintf(buf, sz, "%s ", mnem);
    for (uint8_t i = 0; i < instr->arg_count; i++) {
        AxIrArg* arg = &instr->args[i];
        if (arg->type == ARG_REG) {
            char reg_buf[8];
            snprintf(reg_buf, sizeof(reg_buf), "%c%d", arg->is_64 ? 'x' : 'w', arg->reg_idx);
            strncat(buf, reg_buf, sz - strlen(buf) - 1);
        } else if (arg->type == ARG_IMM) {
            char imm_buf[32];
            snprintf(imm_buf, sizeof(imm_buf), "#%llu", arg->val);
            strncat(buf, imm_buf, sz - strlen(buf) - 1);
        } else if (arg->type == ARG_SYM) {
            strncat(buf, arg->label, sz - strlen(buf) - 1);
        } else if (arg->type == ARG_REG_IMM) {
            // depents on flags for formatting, display [reg, #imm] normally, but for pre [reg, #imm]! and post [reg], #imm
            char reg_buf[8];
            snprintf(reg_buf, sizeof(reg_buf), "%c%d", arg->is_64 ? 'x' : 'w', arg->reg_idx);
            char imm_buf[32];
            snprintf(imm_buf, sizeof(imm_buf), "#%lld", arg->val);
            if (flags & F_PRE) {
                char combined[64];
                snprintf(combined, sizeof(combined), "[%s, %s]!", reg_buf, imm_buf);
                strncat(buf, combined, sz - strlen(buf) - 1);
            } else if (flags & F_POST) {
                char combined[64];
                snprintf(combined, sizeof(combined), "[%s], %s", reg_buf, imm_buf);
                strncat(buf, combined, sz - strlen(buf) - 1);
            } else {
                char combined[64];
                snprintf(combined, sizeof(combined), "[%s, %s]", reg_buf, imm_buf);
                strncat(buf, combined, sz - strlen(buf) - 1);
            }
        }
        if (i < instr->arg_count - 1) {
            strncat(buf, ", ", sz - strlen(buf) - 1);
        }
    }
    return true;
}

const char* ax_opcodeToMnem(AxOpcode opcode) {
    if (opcode < OP_COUNT) {
        return ax_mnemonics[opcode];
    }
    return "unknown";
}

uint32_t encode_mov_wide(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* rd = &instr->args[0];
    AxIrArg* imm = &instr->args[1];
    opcode |= (rd->reg_idx & 0x1F);
    opcode |= ((imm->val & 0xFFFF) << 5);
    opcode |= ((imm->shift / 16) << 21);
    return opcode;
}

uint32_t encode_logical_reg(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* rd = &instr->args[0];
    AxIrArg* rn = &instr->args[1];
    AxIrArg* rm = &instr->args[2];
    opcode |= (rd->reg_idx & 0x1F);
    opcode |= ((rn->reg_idx & 0x1F) << 5);
    opcode |= ((rm->reg_idx & 0x1F) << 16);
    return opcode;
}

uint32_t encode_add_imm(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* rd = &instr->args[0];
    AxIrArg* rn = &instr->args[1];
    AxIrArg* imm = &instr->args[2];
    opcode |= (rd->reg_idx & 0x1F);
    opcode |= ((rn->reg_idx & 0x1F) << 5);
    opcode |= ((imm->val & 0xFFF) << 10);
    opcode |= ((imm->shift / 12) << 22);
    return opcode;
}

uint32_t encode_branch_imm(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* imm = &instr->args[0];
    opcode |= ((imm->val >> 2) & 0x3FFFFFF);
    return opcode;
}

uint32_t encode_branch_cond(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* imm = &instr->args[0];
    opcode |= ((imm->val >> 2) & 0xFFFFFF);
    return opcode;
}

uint32_t encode_ret(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* rn = &instr->args[0];
    opcode |= ((rn->reg_idx & 0x1F) << 5);
    return opcode;
}

uint32_t encode_ldst_imm(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* rt = &instr->args[0];
    AxIrArg* rn = &instr->args[1];
    AxIrArg* imm = &instr->args[2];
    opcode |= (rt->reg_idx & 0x1F);
    opcode |= ((rn->reg_idx & 0x1F) << 5);
    opcode |= ((imm->val & 0xFFF) << 10);
    return opcode;
}

uint32_t encode_adr(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* rd = &instr->args[0];
    AxIrArg* imm = &instr->args[1];
    opcode |= (rd->reg_idx & 0x1F);
    uint32_t immlo = (imm->val & 0x3);
    opcode |= (immlo << 29);
    uint32_t immhi = ((imm->val >> 2) & 0x7FFFF);
    opcode |= (immhi << 5);
    return opcode;
}

uint32_t encode_svc(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* imm = &instr->args[0];
    opcode |= (imm->val & 0xFFFF);
    return opcode;
}

uint32_t encode_ldst_pair(uint32_t base_opcode, AxIrInstr* instr) {
    uint32_t opcode = base_opcode;
    AxIrArg* rt1 = &instr->args[0];
    AxIrArg* rt2 = &instr->args[1];
    AxIrArg* rn = &instr->args[2];
    int32_t offset = (int32_t)instr->args[2].val;
    int32_t imm;
    if (rt1->is_64) {
        imm = offset / 8;
    } else {
        imm = offset / 4;
    }
    opcode |= (rt1->reg_idx & 0x1F);
    opcode |= ((rt2->reg_idx & 0x1F) << 10);
    opcode |= ((rn->reg_idx & 0x1F) << 5);
    opcode |= ((imm & 0x7F) << 15);
    return opcode;
}