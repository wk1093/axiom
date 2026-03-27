#include "axas_object.h"

// Helper to add a string to the strtab and return its offset
uint32_t ax_objectAddString(AxObject* obj, const char* str) {
    uint32_t offset = ax_vecSize(obj->strtab);
    size_t len = strlen(str) + 1; // Include null terminator
    for (size_t i = 0; i < len; i++) {
        ax_vecPush(obj->strtab, str[i]);
    }
    return offset;
}

void ax_objectInit(AxObject* obj) {
    obj->text = ax_vecNew(uint32_t);
    obj->data = ax_vecNew(uint8_t);
    obj->strtab = ax_vecNew(char);
    obj->symtab = ax_vecNew(Elf64_Sym);
    obj->reltab = ax_vecNew(Elf64_Rela);

    // ELF requires index 0 of strtab to be a null byte
    ax_vecPush(obj->strtab, '\0');

    // ELF requires index 0 of symtab to be a NULL symbol
    Elf64_Sym null_sym = {0};
    ax_vecPush(obj->symtab, null_sym);

    obj->ehdr = (Elf64_Ehdr){
        .e_ident = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_NONE},
        .e_type = ET_REL, // MUST be ET_REL for .o files
        .e_machine = EM_AARCH64,
        .e_version = EV_CURRENT,
        .e_ehsize = sizeof(Elf64_Ehdr),
        .e_shentsize = sizeof(Elf64_Shdr),
    };
}
void ax_objectFree(AxObject* obj) {
    ax_vecFree(obj->text);
    ax_vecFree(obj->data);
    ax_vecFree(obj->strtab);
    ax_vecFree(obj->symtab);
    ax_vecFree(obj->reltab);
}

void ax_objectEmit(AxObject* obj, AxIrInstr* instr) {
    uint64_t current_pc = ax_vecSize(obj->text) * 4;

    // 1. Scan arguments for labels to automate Relocations
    for (int i = 0; i < instr->arg_count; i++) {
        if (instr->args[i].type == ARG_SYM) {
            const char* label = instr->args[i].label;
            
            // Map Opcode to ELF Relocation Type
            uint32_t reloc_type = 0;
            if (instr->opcode == OP_ADR)  reloc_type = R_AARCH64_ADR_PREL_LO21;
            if (instr->opcode == OP_BL)   reloc_type = R_AARCH64_CALL26;
            if (instr->opcode == OP_B)    reloc_type = R_AARCH64_JUMP26;
            if (instr->opcode == OP_CBZ || instr->opcode == OP_CBZ_64) reloc_type = R_AARCH64_CONDBR19;
            // Add more as needed...

            if (reloc_type != 0) {
                ax_objectAddReloc(obj, current_pc, label, reloc_type);
            }
        }
    }

    // 2. Push the bytecode (encoder should treat ARG_SYM as 0)
    ax_vecPush(obj->text, ax_ir_to_bytecode(instr));
}

void ax_objectDefineDataLabel(AxObject* obj, const char* name) {
    uint64_t offset = ax_vecSize(obj->data);
    uint32_t existing_idx = ax_objectGetSymbolIndex(obj, name);

    if (existing_idx != 0) {
        Elf64_Sym* sym = &obj->symtab[existing_idx];
        sym->st_value = offset;
        sym->st_shndx = 2; // Point to .data
        sym->st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
    } else {
        ax_objectAddSymbolFull(obj, name, offset, STT_OBJECT, 2); 
    }
}

uint32_t ax_objectGetSymbolIndex(AxObject* obj, const char* name) {
    // Start at 1 because index 0 is always the NULL symbol
    for (uint32_t i = 1; i < ax_vecSize(obj->symtab); i++) {
        Elf64_Sym* sym = &obj->symtab[i];
        const char* sym_name = (const char*)&obj->strtab[sym->st_name];
        if (strcmp(sym_name, name) == 0) {
            return i;
        }
    }
    return 0; // Not found
}

void ax_objectAddReloc(AxObject* obj, uint64_t offset, const char* sym_name, uint32_t type) {
    uint32_t sym_idx = ax_objectGetSymbolIndex(obj, sym_name);

    // If the symbol doesn't exist yet, add it as an EXTERNAL/UNDEFINED symbol
    if (sym_idx == 0) {
        uint32_t name_off = ax_objectAddString(obj, sym_name);
        Elf64_Sym sym = {0};
        sym.st_name  = name_off;
        sym.st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
        sym.st_shndx = SHN_UNDEF; // 0 means it's defined elsewhere (like libc)
        ax_vecPush(obj->symtab, sym);
        sym_idx = ax_vecSize(obj->symtab) - 1;
    }

    Elf64_Rela rela = {0};
    rela.r_offset = offset; // Where in the .text section the patch happens
    rela.r_info   = ELF64_R_INFO(sym_idx, type);
    rela.r_addend = 0; // Usually 0 for AArch64 standard relocs

    ax_vecPush(obj->reltab, rela);
}

// Helper to add a global function symbol (like _start)
void ax_objectAddSymbol(AxObject* obj, const char* name, uint64_t value, uint8_t type) {
    uint32_t name_idx = ax_objectAddString(obj, name);
    Elf64_Sym sym = {0};
    sym.st_name  = name_idx;
    sym.st_info  = ELF64_ST_INFO(STB_GLOBAL, type); // e.g., STT_FUNC
    sym.st_shndx = 1; // Hardcoded to .text for now
    sym.st_value = value;
    ax_vecPush(obj->symtab, sym);
}

void ax_objectAddSymbolFull(AxObject* obj, const char* name, uint64_t value, uint8_t type, uint16_t shndx) {
    uint32_t name_idx = ax_objectAddString(obj, name);
    Elf64_Sym sym = {0};
    sym.st_name  = name_idx;
    sym.st_info  = ELF64_ST_INFO(STB_GLOBAL, type); // e.g., STT_FUNC or STT_OBJECT
    sym.st_shndx = shndx; // Section index (e.g., 1 for .text, 2 for .data)
    sym.st_value = value;
    ax_vecPush(obj->symtab, sym);
}

void ax_objectDefineCodeLabel(AxObject* obj, const char* name) {
    uint64_t offset = ax_vecSize(obj->text) * 4;
    uint32_t existing_idx = ax_objectGetSymbolIndex(obj, name);

    if (existing_idx != 0) {
        // We found the placeholder! Patch the existing symbol instead of adding a new one.
        Elf64_Sym* sym = &obj->symtab[existing_idx];
        sym->st_value = offset;
        sym->st_shndx = 1; // Move from UNDEF to .text
        sym->st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
    } else {
        // Brand new symbol
        ax_objectAddSymbolFull(obj, name, offset, STT_FUNC, 1);
    }
}

size_t align_to(size_t current, size_t alignment) {
    if (alignment == 0) return current;
    return (current + alignment - 1) & ~(alignment - 1);
}

void pad_file(FILE* f, size_t target_off) {
    size_t current = ftell(f);
    while (current < target_off) {
        fputc(0, f);
        current++;
    }
}

void ax_objectWrite(AxObject* obj, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;

    // We'll use 6 sections: NULL, .text, .data, .symtab, .strtab, .reloc
    obj->ehdr.e_shnum = 6;
    obj->ehdr.e_shstrndx = 4; // .shstrtab index (we'll put section names in .strtab for simplicity)

    uint32_t dot_text   = ax_objectAddString(obj, ".text");
    uint32_t dot_data   = ax_objectAddString(obj, ".data");
    uint32_t dot_symtab = ax_objectAddString(obj, ".symtab");
    uint32_t dot_strtab = ax_objectAddString(obj, ".strtab");
    uint32_t dot_reloc  = ax_objectAddString(obj, ".reloc");

    // Calculate offsets based on actual vector sizes
    size_t text_sz   = ax_vecSize(obj->text) * sizeof(uint32_t);
    size_t data_sz   = ax_vecSize(obj->data);
    size_t strtab_sz = ax_vecSize(obj->strtab);
    size_t symtab_sz = ax_vecSize(obj->symtab) * sizeof(Elf64_Sym);
    size_t reloc_sz  = ax_vecSize(obj->reltab) * sizeof(Elf64_Rela);

    size_t current_off = sizeof(Elf64_Ehdr);

    current_off = align_to(current_off, 4);
    size_t text_off = current_off;
    current_off += text_sz;

    current_off = align_to(current_off, 8);
    size_t data_off = current_off;
    current_off += data_sz;

    current_off = align_to(current_off, 1);
    size_t str_off = current_off;
    current_off += strtab_sz;

    current_off = align_to(current_off, 8);
    size_t sym_off = current_off;
    current_off += symtab_sz;

    current_off = align_to(current_off, 8);
    size_t reloc_off = current_off;
    current_off += reloc_sz;

    
    obj->ehdr.e_shoff = current_off;

    Elf64_Shdr shdr[6] = {0};

    // .text
    shdr[1].sh_name      = dot_text;
    shdr[1].sh_type      = SHT_PROGBITS;
    shdr[1].sh_flags     = SHF_ALLOC | SHF_EXECINSTR;
    shdr[1].sh_offset    = text_off;
    shdr[1].sh_size      = text_sz;
    shdr[1].sh_addralign = 4;

    // .data
    shdr[2].sh_name      = dot_data;
    shdr[2].sh_type      = SHT_PROGBITS;
    shdr[2].sh_flags     = SHF_ALLOC | SHF_WRITE;
    shdr[2].sh_offset    = data_off;
    shdr[2].sh_size      = data_sz;
    shdr[2].sh_addralign = 8;

    // .symtab
    shdr[3].sh_name      = dot_symtab;
    shdr[3].sh_type      = SHT_SYMTAB;
    shdr[3].sh_offset    = sym_off;
    shdr[3].sh_size      = symtab_sz;
    shdr[3].sh_link      = 4; // Link to .strtab
    shdr[3].sh_info      = 1; // First non-local symbol index
    shdr[3].sh_entsize   = sizeof(Elf64_Sym);
    shdr[3].sh_addralign = 8;

    // .strtab (Section Header String Table)
    shdr[4].sh_name      = dot_strtab;
    shdr[4].sh_type      = SHT_STRTAB;
    shdr[4].sh_offset    = str_off;
    shdr[4].sh_size      = ax_vecSize(obj->strtab); // Use final size after adding section names
    shdr[4].sh_addralign = 1;

    shdr[5].sh_name      = dot_reloc;
    shdr[5].sh_type      = SHT_RELA;
    shdr[5].sh_offset    = reloc_off;
    shdr[5].sh_size      = reloc_sz;
    shdr[5].sh_link      = 3; // Link to .symtab
    shdr[5].sh_info      = 1; // Section index of the section to which the relocations apply (.text)
    shdr[5].sh_entsize   = sizeof(Elf64_Rela);
    shdr[5].sh_addralign = 8;

    // Write all pieces
    fwrite(&obj->ehdr, 1, sizeof(obj->ehdr), f);
    pad_file(f, text_off);
    fwrite(obj->text,   1, text_sz, f);
    pad_file(f, data_off);
    fwrite(obj->data,   1, data_sz, f);
    pad_file(f, str_off);
    fwrite(obj->strtab, 1, ax_vecSize(obj->strtab), f); // Updated size
    pad_file(f, sym_off);
    fwrite(obj->symtab, 1, symtab_sz, f);
    pad_file(f, reloc_off);
    fwrite(obj->reltab, 1, reloc_sz, f);
    pad_file(f, obj->ehdr.e_shoff);
    fwrite(shdr,        1, sizeof(shdr), f);
    
    fclose(f);
}

bool ax_objectLoad(AxObject* obj, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return false;

    // 1. Initialize the object (creates empty vectors)
    ax_objectInit(obj);

    // 2. Read the ELF Header
    if (fread(&obj->ehdr, 1, sizeof(Elf64_Ehdr), f) != sizeof(Elf64_Ehdr)) {
        fclose(f);
        return false;
    }

    // 3. Jump to and read the Section Header Table
    uint16_t shnum = obj->ehdr.e_shnum;
    Elf64_Shdr* shdrs = malloc(sizeof(Elf64_Shdr) * shnum);
    fseek(f, obj->ehdr.e_shoff, SEEK_SET);
    fread(shdrs, sizeof(Elf64_Shdr), shnum, f);

    // 4. Populate vectors based on section headers
    // We follow the indices used in your write function:
    // 1: .text, 2: .data, 3: .symtab, 4: .strtab, 5: .reloc
    for (int i = 1; i < shnum; i++) {
        Elf64_Shdr s = shdrs[i];
        if (s.sh_size == 0) continue;

        fseek(f, s.sh_offset, SEEK_SET);

        switch (i) {
            case 1: // .text
                {
                    uint32_t count = s.sh_size / sizeof(uint32_t);
                    for (uint32_t j = 0; j < count; j++) {
                        uint32_t val;
                        fread(&val, sizeof(uint32_t), 1, f);
                        ax_vecPush(obj->text, val);
                    }
                }
                break;
            case 2: // .data
                {
                    for (uint32_t j = 0; j < s.sh_size; j++) {
                        uint8_t val;
                        fread(&val, 1, 1, f);
                        ax_vecPush(obj->data, val);
                    }
                }
                break;
            case 3: // .symtab
                {
                    // Clear the NULL symbol added by ax_objectInit to avoid duplicates
                    ax_vecFree(obj->symtab);
                    obj->symtab = ax_vecNew(Elf64_Sym);
                    
                    uint32_t count = s.sh_size / sizeof(Elf64_Sym);
                    for (uint32_t j = 0; j < count; j++) {
                        Elf64_Sym sym;
                        fread(&sym, sizeof(Elf64_Sym), 1, f);
                        ax_vecPush(obj->symtab, sym);
                    }
                }
                break;
            case 4: // .strtab
                {
                    // Clear the null byte added by ax_objectInit
                    ax_vecFree(obj->strtab);
                    obj->strtab = ax_vecNew(char);

                    for (uint32_t j = 0; j < s.sh_size; j++) {
                        char c;
                        fread(&c, 1, 1, f);
                        ax_vecPush(obj->strtab, c);
                    }
                }
                break;
            case 5: // .reloc
                {
                    uint32_t count = s.sh_size / sizeof(Elf64_Rela);
                    for (uint32_t j = 0; j < count; j++) {
                        Elf64_Rela rela;
                        fread(&rela, sizeof(Elf64_Rela), 1, f);
                        ax_vecPush(obj->reltab, rela);
                    }
                }
                break;
        }
    }

    free(shdrs);
    fclose(f);
    return true;
}

void ax_printObjectInfo(AxObject* obj) {
    printf("Object Info:\n");
    printf("  .text: %u bytes\n", ax_vecSize(obj->text) * sizeof(uint32_t));
    printf("  .data: %u bytes\n", ax_vecSize(obj->data));
    printf("  .strtab: %u bytes\n", ax_vecSize(obj->strtab));
    printf("  .symtab: %u entries\n", ax_vecSize(obj->symtab));
    printf("  .reltab: %u entries\n", ax_vecSize(obj->reltab));
}