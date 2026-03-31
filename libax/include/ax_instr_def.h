// Format: X(internal_name, mnemonic, arg_count, base_opcode, flags, encoder_func)

// --- Move Wide ---
X(MOVZ_64,    "movz", 2, 0xD2800000, F_64 | F_IMM, encode_mov_wide)
X(MOVZ_32,    "movz", 2, 0x52800000, F_32 | F_IMM, encode_mov_wide)
X(MOVK_64,    "movk", 2, 0xF2800000, F_64 | F_IMM, encode_mov_wide)
X(MOVK_32,    "movk", 2, 0x72800000, F_32 | F_IMM, encode_mov_wide)

// --- Logical ---
X(AND_REG_64, "and",  3, 0x8A000000, F_64 | F_REG, encode_logical_reg)
X(AND_REG_32, "and",  3, 0x0A000000, F_32 | F_REG, encode_logical_reg)
X(ORR_REG_64, "orr",  3, 0xAA000000, F_64 | F_REG, encode_logical_reg)
X(ORR_REG_32, "orr",  3, 0x2A000000, F_32 | F_REG, encode_logical_reg)

// --- Arithmetic ---
X(ADD_IMM_64,  "add",  3, 0x91000000, F_64 | F_IMM, encode_add_imm)
X(ADD_IMM_32,  "add",  3, 0x11000000, F_32 | F_IMM, encode_add_imm)
X(ADDS_IMM_64, "adds", 3, 0xB1000000, F_64 | F_IMM, encode_add_imm)
X(ADDS_IMM_32, "adds", 3, 0x31000000, F_32 | F_IMM, encode_add_imm)
X(SUB_IMM_64,  "sub",  3, 0xD1000000, F_64 | F_IMM, encode_add_imm)
X(SUB_IMM_32,  "sub",  3, 0x51000000, F_32 | F_IMM, encode_add_imm)
X(SUBS_IMM_64, "subs", 3, 0xF1000000, F_64 | F_IMM, encode_add_imm)
X(SUBS_IMM_32, "subs", 3, 0x71000000, F_32 | F_IMM, encode_add_imm)

X(ADD_64, "add",  3, 0x8B000000, F_64 | F_REG, encode_logical_reg)
X(ADD_32, "add",  3, 0x0B000000, F_32 | F_REG, encode_logical_reg)
X(ADDS_64, "adds", 3, 0xAB000000, F_64 | F_REG, encode_logical_reg)
X(ADDS_32, "adds", 3, 0x2B000000, F_32 | F_REG, encode_logical_reg)
X(SUB_64, "sub",  3, 0xCB000000, F_64 | F_REG, encode_logical_reg)
X(SUB_32, "sub",  3, 0x4B000000, F_32 | F_REG, encode_logical_reg)
X(SUBS_64, "subs", 3, 0xEB000000, F_64 | F_REG, encode_logical_reg)
X(SUBS_32, "subs", 3, 0x6B000000, F_32 | F_REG, encode_logical_reg)

// --- Branching (Control Flow) ---
X(B,           "b",    1, 0x14000000, F_UNI | F_IMM, encode_branch_imm)
X(BL,          "bl",   1, 0x94000000, F_UNI | F_IMM, encode_branch_imm)
X(B_COND,      "b.",   1, 0x54000000, F_UNI | F_IMM, encode_branch_cond)
X(RET,         "ret",  1, 0xD65F0000, F_UNI | F_REG, encode_ret)

X(LDR_64, "ldr", 2, 0xF9400000, F_64 | F_MEM, encode_ldst_imm)
X(STR_64, "str", 2, 0xF9000000, F_64 | F_MEM, encode_ldst_imm)
X(LDR, "ldr", 2, 0xB9400000, F_32 | F_MEM, encode_ldst_imm)
X(STR, "str", 2, 0xB9000000, F_32 | F_MEM, encode_ldst_imm)
X(LDRB, "ldrb", 2, 0x39400000, F_32 | F_MEM, encode_ldst_imm)

X(ADR, "adr", 2, 0x10000000, F_64 | F_IMM, encode_adr)

X(SVC, "svc", 1, 0xD4000001, F_UNI | F_IMM, encode_svc)

// TODO: make sure these opcodes are correct.
X(STP_64, "stp", 3, 0xA9000000, F_64 | F_MEM, encode_ldst_pair)
X(LDP_64, "ldp", 3, 0xA8C00000, F_64 | F_MEM, encode_ldst_pair)
X(STP64_PRE, "stp", 3, 0xA9800000, F_64 | F_MEM | F_PRE, encode_ldst_pair)
X(LDP64_PRE, "ldp", 3, 0xA9C00000, F_64 | F_MEM | F_PRE, encode_ldst_pair)
X(STP64_POST,"stp", 3, 0xA8800000, F_64 | F_MEM | F_POST, encode_ldst_pair)
X(LDP64_POST,"ldp", 3, 0xA8C00000, F_64 | F_MEM | F_POST, encode_ldst_pair)

X(STP, "stp", 3, 0x29000000, F_32 | F_MEM, encode_ldst_pair)
X(LDP, "ldp", 3, 0x28800000, F_32 | F_MEM, encode_ldst_pair)
X(STP_PRE, "stp", 3, 0x29800000, F_32 | F_MEM | F_PRE, encode_ldst_pair)
X(LDP_PRE, "ldp", 3, 0x29800000, F_32 | F_MEM | F_PRE, encode_ldst_pair)
X(STP_POST, "stp", 3, 0x28800000, F_32 | F_MEM | F_POST, encode_ldst_pair)
X(LDP_POST, "ldp", 3, 0x28800000, F_32 | F_MEM | F_POST, encode_ldst_pair)

X(CBZ, "cbz", 2, 0x34000000, F_32 | F_IMM, encode_cbz)
X(CBZ_64, "cbz", 2, 0xB4000000, F_64 | F_IMM, encode_cbz)

X(ADRP, "adrp", 2, 0x90000000, F_64 | F_IMM, encode_adrp)

X(BNE, "b.ne", 1, 0x54000000, F_UNI | F_IMM, encode_branch_cond)