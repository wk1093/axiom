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
    obj->text_shndx = 1; // axas always puts .text at section 1
    obj->data_shndx = 2; // axas always puts .data at section 2

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
            if (instr->opcode == OP_ADRP) reloc_type = R_AARCH64_ADR_PREL_PG_HI21;
            if (instr->opcode == OP_BNE) reloc_type = R_AARCH64_CONDBR19; // BNE is also a conditional branch, so it can use the same relocation type as CBZ/CBNZ

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
        // sym->st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
        // don't default to global anymore, now that we have .global directive to control that
        sym->st_info  = ELF64_ST_INFO(ELF64_ST_BIND(sym->st_info), STT_OBJECT);
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
    // sym.st_info  = ELF64_ST_INFO(STB_GLOBAL, type); // e.g., STT_FUNC or STT_OBJECT
    // don't default to global anymore
    sym.st_info  = ELF64_ST_INFO(ELF64_ST_BIND(type), ELF64_ST_TYPE(type)); // allow caller to specify bind/type separately
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
        // sym->st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
        // don't default to global anymore, now that we have .global directive to control that
        sym->st_info  = ELF64_ST_INFO(ELF64_ST_BIND(sym->st_info), STT_FUNC);
    } else {
        // Brand new symbol
        ax_objectAddSymbolFull(obj, name, offset, STT_FUNC, 1);
    }
}

// .global: ensure symbol has global binding; creates a placeholder if not yet defined.
void ax_objectDeclareGlobal(AxObject* obj, const char* name) {
    uint32_t idx = ax_objectGetSymbolIndex(obj, name);
    if (idx != 0) {
        Elf64_Sym* sym = &obj->symtab[idx];
        uint8_t type = ELF64_ST_TYPE(sym->st_info);
        sym->st_info = ELF64_ST_INFO(STB_GLOBAL, type);
    } else {
        // Placeholder; filled in when the label is actually defined.
        uint32_t name_off = ax_objectAddString(obj, name);
        Elf64_Sym sym = {0};
        sym.st_name  = name_off;
        sym.st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
        sym.st_shndx = SHN_UNDEF;
        ax_vecPush(obj->symtab, sym);
    }
}

// .extern: declare an external (undefined) symbol so it can be referenced before linking.
void ax_objectDeclareExternal(AxObject* obj, const char* name) {
    if (ax_objectGetSymbolIndex(obj, name) == 0) {
        uint32_t name_off = ax_objectAddString(obj, name);
        Elf64_Sym sym = {0};
        sym.st_name  = name_off;
        sym.st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
        sym.st_shndx = SHN_UNDEF;
        ax_vecPush(obj->symtab, sym);
    }
}

char nextNonWhitespace(const char* str, size_t* start) {
    size_t i = *start;
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n') {
        i++;
    }
    *start = i;
    return str[i];
}

void ax_objectSetSymbolSize(AxObject* obj, const char* value) {
    // first separate sym from N
    char name[256] = {0};
    size_t i = 0;
    while (value[i] != ',' && value[i] != '\0' && i < sizeof(name) - 1) {
        name[i] = value[i];
        i++;
    }
    name[i] = '\0';
    if (value[i] == ',') {
        i++; // skip comma
        size_t size_start = i;
        char next = nextNonWhitespace(value, &size_start);
        if (next == '\0') return; // No size provided
        if (next == '.') {
            if (nextNonWhitespace(value, &size_start) == '-') {
                // we have .-sym
                char sym_name[256] = {0};
                size_t j = 0;
                while (value[size_start] != '\0' && value[size_start] != ' ' && value[size_start] != '\t' && j < sizeof(sym_name) - 1) {
                    sym_name[j] = value[size_start];
                    size_start++;
                    j++;
                }
                sym_name[j] = '\0';
                uint32_t sym_idx = ax_objectGetSymbolIndex(obj, sym_name);
                if (sym_idx != 0) {
                    Elf64_Sym* sym = &obj->symtab[sym_idx];
                    uint64_t sym_end = sym->st_value + sym->st_size;
                    uint32_t target_idx = ax_objectGetSymbolIndex(obj, name);
                    if (target_idx != 0) {
                        obj->symtab[target_idx].st_size = sym_end - obj->symtab[target_idx].st_value;
                    }
                }
            }
        } else {
            // we have a direct size value
            uint64_t size = strtoull(&value[size_start], NULL, 0);
            uint32_t target_idx = ax_objectGetSymbolIndex(obj, name);
            if (target_idx != 0) {
                obj->symtab[target_idx].st_size = size;
            }
        }
    }
}

// .type sym, @type — value format after parser fix: "sym, function" or "sym, object".
// The '@' prefix is stripped by the parser (TOK_ERROR) so only the type word remains.
void ax_objectSetSymbolType(AxObject* obj, const char* value) {
    char name[256] = {0};
    char type_str[64] = {0};
    if (sscanf(value, " %255[^,], %63s", name, type_str) == 2) {
        uint32_t idx = ax_objectGetSymbolIndex(obj, name);
        if (idx != 0) {
            Elf64_Sym* sym = &obj->symtab[idx];
            uint8_t bind = ELF64_ST_BIND(sym->st_info);
            uint8_t type = STT_NOTYPE;
            if (strcmp(type_str, "function") == 0) type = STT_FUNC;
            else if (strcmp(type_str, "object") == 0)  type = STT_OBJECT;
            sym->st_info = ELF64_ST_INFO(bind, type);
        }
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

    // ELF requires all STB_LOCAL symbols to precede STB_GLOBAL/STB_WEAK in
    // the symtab, with sh_info set to the index of the first non-local.
    // Partition the symtab in-place (keeping null sym at index 0).
    size_t nsyms = ax_vecSize(obj->symtab);
    Elf64_Sym* sorted = malloc(nsyms * sizeof(Elf64_Sym));
    uint32_t*  old_to_new = malloc(nsyms * sizeof(uint32_t));

    // Index 0 is always the null sym (local)
    sorted[0] = obj->symtab[0];
    old_to_new[0] = 0;
    uint32_t local_count = 1;
    uint32_t global_count = 0;

    // Count locals and globals (skip null at 0)
    for (size_t i = 1; i < nsyms; i++) {
        if (ELF64_ST_BIND(obj->symtab[i].st_info) == STB_LOCAL) local_count++;
        else global_count++;
    }

    // Fill locals then globals
    uint32_t li = 1, gi = local_count;
    for (size_t i = 1; i < nsyms; i++) {
        if (ELF64_ST_BIND(obj->symtab[i].st_info) == STB_LOCAL) {
            sorted[li] = obj->symtab[i];
            old_to_new[i] = li++;
        } else {
            sorted[gi] = obj->symtab[i];
            old_to_new[i] = gi++;
        }
    }

    // Apply the remapping to the relocation table
    for (size_t i = 0; i < ax_vecSize(obj->reltab); i++) {
        uint32_t old_sym = (uint32_t)ELF64_R_SYM(obj->reltab[i].r_info);
        uint32_t type    = (uint32_t)ELF64_R_TYPE(obj->reltab[i].r_info);
        obj->reltab[i].r_info = ELF64_R_INFO(old_to_new[old_sym], type);
    }

    // Replace symtab contents with sorted version
    memcpy(obj->symtab, sorted, nsyms * sizeof(Elf64_Sym));
    free(sorted);
    free(old_to_new);

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
    shdr[3].sh_info      = local_count; // index of first global symbol
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

    // 1. Read ELF header
    Elf64_Ehdr temp_ehdr;
    memset(&temp_ehdr, 0, sizeof(temp_ehdr));
    if (fread(&temp_ehdr, 1, 64, f) != 64) {
        printf("Failed to read ELF header\n");
        fclose(f);
        return false;
    }

    obj->text   = ax_vecNew(uint32_t);
    obj->data   = ax_vecNew(uint8_t);
    obj->strtab = ax_vecNew(char);
    obj->symtab = ax_vecNew(Elf64_Sym);
    obj->reltab = ax_vecNew(Elf64_Rela);
    obj->ehdr   = temp_ehdr;
    // Defaults (axas-style); overridden below if we find different indices
    obj->text_shndx = 1;
    obj->data_shndx = 2;

    if (obj->ehdr.e_shoff == 0) {
        printf("No section header table found\n");
        fclose(f);
        return false;
    }

    // 2. Load section header table
    fseek(f, obj->ehdr.e_shoff, SEEK_SET);
    Elf64_Shdr* shdrs = malloc(sizeof(Elf64_Shdr) * obj->ehdr.e_shnum);
    fread(shdrs, sizeof(Elf64_Shdr), obj->ehdr.e_shnum, f);

    // 3. First pass: identify the symbol string table index and the
    //    actual section indices for .text and .data.  GCC separates
    //    .strtab (symbol names, sh_link of .symtab) from .shstrtab
    //    (section names, e_shstrndx).  Using e_shstrndx for symbol
    //    lookups is the root cause of symbols like _start not being found.
    int strtab_idx = obj->ehdr.e_shstrndx; // safe fallback
    int text_shndx = -1, data_shndx = -1;
    for (int i = 0; i < obj->ehdr.e_shnum; i++) {
        Elf64_Shdr* s = &shdrs[i];
        if (s->sh_type == SHT_SYMTAB) {
            strtab_idx = (int)s->sh_link;
        } else if (s->sh_type == SHT_PROGBITS && (s->sh_flags & SHF_ALLOC)) {
            if ((s->sh_flags & SHF_EXECINSTR) && text_shndx == -1)
                text_shndx = i;
            else if ((s->sh_flags & SHF_WRITE) && data_shndx == -1)
                data_shndx = i;
        }
    }
    if (text_shndx != -1) obj->text_shndx = (uint16_t)text_shndx;
    if (data_shndx != -1) obj->data_shndx = (uint16_t)data_shndx;

    // 4. Second pass: load section data
    for (int i = 0; i < obj->ehdr.e_shnum; i++) {
        Elf64_Shdr s = shdrs[i];
        if (s.sh_size == 0) continue;

        fseek(f, s.sh_offset, SEEK_SET);

        if (s.sh_type == SHT_PROGBITS && (s.sh_flags & SHF_ALLOC)) {
            if (s.sh_flags & SHF_EXECINSTR) {
                // .text: load as 32-bit instructions
                size_t count = s.sh_size / 4;
                for (size_t j = 0; j < count; j++) {
                    uint32_t val; fread(&val, 4, 1, f);
                    ax_vecPush(obj->text, val);
                }
            } else if (s.sh_flags & SHF_WRITE) {
                // .data: writable, allocated (skip .rodata and debug sections)
                for (size_t j = 0; j < s.sh_size; j++) {
                    uint8_t val; fread(&val, 1, 1, f);
                    ax_vecPush(obj->data, val);
                }
            }
            // .rodata (allocatable, non-writable, non-exec) intentionally skipped for now
        } else if (s.sh_type == SHT_SYMTAB) {
            size_t count = s.sh_size / sizeof(Elf64_Sym);
            for (size_t j = 0; j < count; j++) {
                Elf64_Sym sym; fread(&sym, sizeof(Elf64_Sym), 1, f);
                ax_vecPush(obj->symtab, sym);
            }
        } else if (s.sh_type == SHT_STRTAB && i == strtab_idx) {
            // Symbol-name string table (.strtab, not .shstrtab)
            for (size_t j = 0; j < s.sh_size; j++) {
                char c; fread(&c, 1, 1, f);
                ax_vecPush(obj->strtab, c);
            }
        } else if (s.sh_type == SHT_RELA && (int)s.sh_info == text_shndx) {
            // Only load relocations that apply to .text; skip debug relocations
            size_t count = s.sh_size / sizeof(Elf64_Rela);
            for (size_t j = 0; j < count; j++) {
                Elf64_Rela rela; fread(&rela, sizeof(Elf64_Rela), 1, f);
                ax_vecPush(obj->reltab, rela);
            }
        }
    }

    free(shdrs);
    fclose(f);
    return true;
}

void ax_printObjectInfo(AxObject* obj) {
    printf("Object Info:\n");
    printf("  .text: %lu bytes\n", ax_vecSize(obj->text) * sizeof(uint32_t));
    printf("  .data: %lu bytes\n", ax_vecSize(obj->data));
    printf("  .strtab: %lu bytes\n", ax_vecSize(obj->strtab));
    printf("  .symtab: %lu entries\n", ax_vecSize(obj->symtab));
    printf("  .reltab: %lu entries\n", ax_vecSize(obj->reltab));
}