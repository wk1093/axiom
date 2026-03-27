#ifndef AX_IR_H
#define AX_IR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    F_32 = (1 << 0),
    F_64 = (1 << 1),
    F_IMM = (1 << 2),
    F_REG = (1 << 3),
    F_MEM = (1 << 4),
    F_UNI = (1 << 5), // Unconditional (e.g. ret, svc)
    F_PRE = (1 << 6), // Pre-indexed addressing (for load/store pair)
    F_POST = (1 << 7), // Post-indexed addressing (for load/store pair)
} AxInstrFlags;


typedef struct {
    uint8_t idx;
    bool is_64;
} AxReg;

typedef enum {
#define X(name, mnem, arg_count, base_opcode, flags, encoder) OP_##name,
#include "ax_instr_def.h"
#undef X
    OP_COUNT
} AxOpcode;

typedef enum : uint8_t {
    ARG_NONE,
    ARG_REG,
    ARG_IMM,
    ARG_SYM,
    ARG_REG_IMM, // For instructions that combine a register and immediate (like LDR with a large offset)
} AxIrArgType;

typedef struct {
    AxIrArgType type;
    uint8_t reg_idx;
    bool is_64;
    uint8_t shift;
    uint64_t val;
    const char* label;
} AxIrArg;

typedef struct {
    AxOpcode opcode;
    AxIrArg args[3];
    uint8_t arg_count;
} AxIrInstr;

uint32_t ax_ir_to_bytecode(AxIrInstr* instr);

bool ax_ir_to_asm(AxIrInstr* instr, char* out_buf, size_t buf_size);

const char* ax_opcodeToMnem(AxOpcode opcode);

// TODO
// bool ax_bytecode_to_ir(uint32_t bytecode, AxIrInstr* out_instr);

// encoders
uint32_t encode_mov_wide(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_logical_reg(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_add_imm(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_branch_imm(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_branch_cond(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_ret(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_ldst_imm(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_adr(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_svc(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_ldst_pair(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_cbz(uint32_t base_opcode, AxIrInstr* instr);
uint32_t encode_adrp(uint32_t base_opcode, AxIrInstr* instr);

#endif