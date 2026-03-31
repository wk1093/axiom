#include "axld_exec.h"

void ax_execInit(AxExecutable* exec) {
    memset(exec, 0, sizeof(AxExecutable));
    // Set up AArch64 Defaults
    memcpy(exec->ehdr.e_ident, ELFMAG, SELFMAG);
    exec->ehdr.e_ident[EI_CLASS]   = ELFCLASS64;
    exec->ehdr.e_ident[EI_DATA]    = ELFDATA2LSB;
    exec->ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    
    exec->ehdr.e_type    = ET_EXEC;
    exec->ehdr.e_machine = EM_AARCH64;
    exec->ehdr.e_version = EV_CURRENT;

    exec->global_sym_names  = ax_vecNew(char*);
    exec->global_sym_vaddrs = ax_vecNew(uint64_t);
}

void ax_execFree(AxExecutable* exec) {
    if (exec->text_payload) free(exec->text_payload);
    if (exec->data_payload) free(exec->data_payload);
    for (size_t i = 0; i < ax_vecSize(exec->global_sym_names); i++)
        free(exec->global_sym_names[i]);
    ax_vecFree(exec->global_sym_names);
    ax_vecFree(exec->global_sym_vaddrs);
}

// Look up a symbol by name in the global symbol table.
// Returns its resolved virtual address, or 0 if not found.
static uint64_t ax_execLookupGlobal(AxExecutable* exec, const char* name) {
    for (size_t i = 0; i < ax_vecSize(exec->global_sym_names); i++) {
        if (strcmp(exec->global_sym_names[i], name) == 0)
            return exec->global_sym_vaddrs[i];
    }
    return 0;
}

#define TEXT_VADDR        0x400000
#define PAGE_SIZE_EXEC    0x10000
#define TOTAL_HEADER_SIZE (64 + (2 * 56))  // ELF64 ehdr + 2 phdrs

// Helper: apply all relocation patches for obj's .text.
// text_offset / data_offset are the object's positions in the monolithic buffers.
static void patch_relocations(AxExecutable* exec, AxObject* obj,
                               uint64_t text_offset, uint64_t data_offset) {
    uint64_t text_segment_base = TEXT_VADDR;
    uint64_t data_segment_base = TEXT_VADDR + PAGE_SIZE_EXEC;

    uint32_t rel_count = ax_vecSize(obj->reltab);
    for (uint32_t i = 0; i < rel_count; i++) {
        Elf64_Rela* rel = &obj->reltab[i];
        uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
        uint32_t type    = ELF64_R_TYPE(rel->r_info);

        Elf64_Sym* target_sym = &obj->symtab[sym_idx];

        uint64_t S; // Final Symbol VAddr
        if (target_sym->st_shndx == SHN_UNDEF) {
            const char* sym_name = obj->strtab + target_sym->st_name;
            S = ax_execLookupGlobal(exec, sym_name);
            if (S == 0) {
                printf("Warning: undefined symbol '%s' — relocation skipped\n", sym_name);
                continue;
            }
        } else if (target_sym->st_shndx == obj->data_shndx) {
            S = data_segment_base + data_offset + target_sym->st_value;
        } else {
            S = text_segment_base + TOTAL_HEADER_SIZE + text_offset + target_sym->st_value;
        }

        uint64_t P = text_segment_base + TOTAL_HEADER_SIZE + text_offset + rel->r_offset;
        int64_t  A = rel->r_addend;

        uint32_t* instr = (uint32_t*)(exec->text_payload + text_offset + rel->r_offset);

        if (type == R_AARCH64_CALL26 || type == R_AARCH64_JUMP26) {
            int32_t imm26 = (int32_t)((S + A - P) / 4);
            *instr = (*instr & 0xFC000000) | (imm26 & 0x03FFFFFF);
        } else if (type == R_AARCH64_ADR_PREL_LO21) {
            int64_t offset = (int64_t)(S + A - P);
            uint32_t immlo = (uint32_t)(offset & 3);
            uint32_t immhi = (uint32_t)((offset >> 2) & 0x7FFFF);
            *instr = (*instr & 0x9F00001F) | (immlo << 29) | (immhi << 5);
        } else if (type == R_AARCH64_CONDBR19) {
            int32_t imm19 = (int32_t)((S + A - P) / 4);
            *instr = (*instr & 0xFF00001F) | (imm19 << 5);
        } else if (type == R_AARCH64_LDST8_ABS_LO12_NC  || type == R_AARCH64_LDST16_ABS_LO12_NC ||
                   type == R_AARCH64_LDST32_ABS_LO12_NC || type == R_AARCH64_LDST64_ABS_LO12_NC ||
                   type == R_AARCH64_LDST128_ABS_LO12_NC) {
            uint32_t imm12 = (uint32_t)(S + A) & 0xFFF;
            *instr = (*instr & 0xFFC003FF) | (imm12 << 10);
        } else if (type == R_AARCH64_ADR_GOT_PAGE || type == R_AARCH64_ADR_PREL_PG_HI21) {
            int64_t offset = (int64_t)(S + A - P);
            uint32_t immlo = (uint32_t)((offset >> 12) & 3);
            uint32_t immhi = (uint32_t)((offset >> 14) & 0x7FFFF);
            *instr = (*instr & 0x9F00001F) | (immlo << 29) | (immhi << 5);
        } else if (type == R_AARCH64_ADD_ABS_LO12_NC) {
            uint32_t imm12 = (uint32_t)(S + A) & 0xFFF;
            *instr = (*instr & 0xFFC003FF) | (imm12 << 10);
        } else if (type == R_AARCH64_ABS64) {
            uint64_t* addr_ptr = (uint64_t*)(exec->data_payload + data_offset + rel->r_offset);
            *addr_ptr = S + A;
        } else if (type == R_AARCH64_RELATIVE) {
            uint64_t* addr_ptr = (uint64_t*)(exec->data_payload + data_offset + rel->r_offset);
            *addr_ptr = text_segment_base + TOTAL_HEADER_SIZE + text_offset + rel->r_offset + A;
        } else if (type == R_AARCH64_PREL32) {
            uint32_t* addr_ptr = (uint32_t*)(exec->data_payload + data_offset + rel->r_offset);
            *addr_ptr = (uint32_t)(S + A - P);
        } else if (type == R_AARCH64_PREL64) {
            uint64_t* addr_ptr = (uint64_t*)(exec->data_payload + data_offset + rel->r_offset);
            *addr_ptr = S + A - P;
        } else if (type == R_AARCH64_ABS32) {
            uint32_t* addr_ptr = (uint32_t*)(exec->data_payload + data_offset + rel->r_offset);
            *addr_ptr = (uint32_t)(S + A);
        } else if (type == R_AARCH64_ABS16) {
            uint16_t* addr_ptr = (uint16_t*)(exec->data_payload + data_offset + rel->r_offset);
            *addr_ptr = (uint16_t)(S + A);
        } else {
            printf("Warning: Unsupported relocation type %u\n", type);
        }
    }
}

// Pass 1: record this object's layout offsets and register all defined symbols.
// Updates exec->text/data_payload_size to account for this object's contribution.
void ax_execRegisterSymbols(AxExecutable* exec, AxObject* obj) {
    uint64_t text_offset = exec->text_payload_size;
    uint64_t data_offset = exec->data_payload_size;

    obj->link_text_offset = text_offset;
    obj->link_data_offset = data_offset;

    uint64_t text_segment_base = TEXT_VADDR;
    uint64_t data_segment_base = TEXT_VADDR + PAGE_SIZE_EXEC;

    uint32_t sym_count = ax_vecSize(obj->symtab);
    for (uint32_t i = 1; i < sym_count; i++) {
        Elf64_Sym* sym = &obj->symtab[i];
        if (sym->st_shndx == SHN_UNDEF) continue;
        if (sym->st_name == 0) continue;
        const char* name = obj->strtab + sym->st_name;
        if (name[0] == '\0') continue;

        uint64_t sym_vaddr;
        if (sym->st_shndx == obj->data_shndx)
            sym_vaddr = data_segment_base + data_offset + sym->st_value;
        else
            sym_vaddr = text_segment_base + TOTAL_HEADER_SIZE + text_offset + sym->st_value;

        char* name_copy = strdup(name);
        ax_vecPush(exec->global_sym_names,  name_copy);
        ax_vecPush(exec->global_sym_vaddrs, sym_vaddr);

        if (strcmp(name, "_start") == 0)
            exec->entry_point = sym_vaddr;
    }

    exec->text_payload_size += ax_vecSize(obj->text) * sizeof(uint32_t);
    exec->data_payload_size += ax_vecSize(obj->data);
}

// Pass 2: copy code/data into the executable buffers and patch all relocations.
// ax_execRegisterSymbols must have been called for ALL objects before this.
void ax_execCopyAndPatch(AxExecutable* exec, AxObject* obj) {
    uint64_t text_offset = obj->link_text_offset;
    uint64_t data_offset = obj->link_data_offset;

    uint64_t obj_text_bytes = ax_vecSize(obj->text) * sizeof(uint32_t);
    uint64_t obj_data_bytes = ax_vecSize(obj->data);

    if (obj_text_bytes > 0) {
        exec->text_payload = realloc(exec->text_payload, text_offset + obj_text_bytes);
        memcpy(exec->text_payload + text_offset, obj->text, obj_text_bytes);
    }
    if (obj_data_bytes > 0) {
        exec->data_payload = realloc(exec->data_payload, data_offset + obj_data_bytes);
        memcpy(exec->data_payload + data_offset, obj->data, obj_data_bytes);
    }

    patch_relocations(exec, obj, text_offset, data_offset);
}

// Convenience wrapper for single-object use.
void ax_execLink(AxExecutable* exec, AxObject* obj) {
    ax_execRegisterSymbols(exec, obj);
    ax_execCopyAndPatch(exec, obj);
}

bool ax_execWrite(AxExecutable* exec, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    const uint32_t PAGE_SIZE = 0x10000; // 64KB Page Alignment
    uint32_t ehdr_size = sizeof(Elf64_Ehdr);
    uint32_t phdr_size = sizeof(Elf64_Phdr);
    uint32_t header_total = ehdr_size + (2 * phdr_size);

    // 1. Setup ELF Header (Crucial for "Valid Image")
    memset(&exec->ehdr, 0, sizeof(Elf64_Ehdr));
    memcpy(exec->ehdr.e_ident, ELFMAG, SELFMAG);
    exec->ehdr.e_ident[EI_CLASS]   = ELFCLASS64;
    exec->ehdr.e_ident[EI_DATA]    = ELFDATA2LSB;
    exec->ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    
    exec->ehdr.e_type      = ET_EXEC;
    exec->ehdr.e_machine   = EM_AARCH64;
    exec->ehdr.e_version   = EV_CURRENT;
    exec->ehdr.e_entry     = exec->entry_point;
    exec->ehdr.e_phoff     = ehdr_size;
    exec->ehdr.e_ehsize    = ehdr_size;
    exec->ehdr.e_phentsize = phdr_size;
    exec->ehdr.e_phnum     = 2;

    // 2. Calculate Alignment
    uint64_t text_end_in_file = header_total + exec->text_payload_size;
    // Align the data start to the next 64KB boundary
    uint64_t data_file_offset = (text_end_in_file + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
    uint32_t padding_size = (uint32_t)(data_file_offset - text_end_in_file);

    // 3. Setup Program Headers
    exec->phdr_text.p_type   = PT_LOAD;
    exec->phdr_text.p_flags  = PF_R | PF_X;
    exec->phdr_text.p_offset = 0;
    exec->phdr_text.p_vaddr  = TEXT_VADDR;
    exec->phdr_text.p_paddr  = TEXT_VADDR;
    exec->phdr_text.p_filesz = text_end_in_file;
    exec->phdr_text.p_memsz  = text_end_in_file;
    exec->phdr_text.p_align  = PAGE_SIZE;

    exec->phdr_data.p_type   = PT_LOAD;
    exec->phdr_data.p_flags  = PF_R | PF_W;
    exec->phdr_data.p_offset = data_file_offset;
    exec->phdr_data.p_vaddr  = TEXT_VADDR + data_file_offset;
    exec->phdr_data.p_paddr  = TEXT_VADDR + data_file_offset;
    exec->phdr_data.p_filesz = exec->data_payload_size;
    exec->phdr_data.p_memsz  = exec->data_payload_size;
    exec->phdr_data.p_align  = PAGE_SIZE;

    // 4. Physical Write
    fwrite(&exec->ehdr, 1, ehdr_size, f);
    fwrite(&exec->phdr_text, 1, phdr_size, f);
    fwrite(&exec->phdr_data, 1, phdr_size, f);
    fwrite(exec->text_payload, 1, exec->text_payload_size, f);

    if (padding_size > 0) {
        uint8_t* pad = calloc(1, padding_size);
        fwrite(pad, 1, padding_size, f);
        free(pad);
    }

    fwrite(exec->data_payload, 1, exec->data_payload_size, f);
    fclose(f);
    return true;
}