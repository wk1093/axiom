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
}

void ax_execFree(AxExecutable* exec) {
    if (exec->text_payload) free(exec->text_payload);
    if (exec->data_payload) free(exec->data_payload);
}

#define TEXT_VADDR 0x400000

void ax_execLink(AxExecutable* exec, AxObject* obj) {
    const uint32_t PAGE_SIZE = 0x10000;
    const uint32_t total_header_size = 64 + (2 * 56);

    // 1. Calculate current offsets in the monolithic payloads
    uint64_t text_offset = exec->text_payload_size;
    uint64_t data_offset = exec->data_payload_size;

    // 2. Expand Payloads
    uint64_t obj_text_bytes = ax_vecSize(obj->text) * sizeof(uint32_t);
    uint64_t obj_data_bytes = ax_vecSize(obj->data);

    exec->text_payload = realloc(exec->text_payload, text_offset + obj_text_bytes);
    exec->data_payload = realloc(exec->data_payload, data_offset + obj_data_bytes);

    // 3. Copy Data
    memcpy(exec->text_payload + text_offset, obj->text, obj_text_bytes);
    memcpy(exec->data_payload + data_offset, obj->data, obj_data_bytes);

    // 4. Calculate Segment Bases
    // According to your readelf:
    // TEXT Segment starts at TEXT_VADDR (0x400000)
    // DATA Segment starts at TEXT_VADDR + PAGE_SIZE (0x410000)
    uint64_t text_segment_base = TEXT_VADDR;
    uint64_t data_segment_base = TEXT_VADDR + PAGE_SIZE;

    // 5. Patch Relocations
    uint32_t rel_count = ax_vecSize(obj->reltab);
    for (uint32_t i = 0; i < rel_count; i++) {
        Elf64_Rela* rel = &obj->reltab[i];
        uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
        uint32_t type    = ELF64_R_TYPE(rel->r_info);

        Elf64_Sym* target_sym = &obj->symtab[sym_idx];
        
        uint64_t S; // Final Symbol VAddr
        if (target_sym->st_shndx == 2) { // .data
            // Base of data segment + offset of existing data + offset of this symbol
            S = data_segment_base + data_offset + target_sym->st_value;
        } else { // .text
            // Base of text segment + header size + offset of existing code + offset of symbol
            S = text_segment_base + total_header_size + text_offset + target_sym->st_value;
        }

        // P is the VAddr of the instruction being patched
        uint64_t P = text_segment_base + total_header_size + text_offset + rel->r_offset;
        int64_t  A = rel->r_addend;

        uint32_t* instr = (uint32_t*)(exec->text_payload + text_offset + rel->r_offset);

        if (type == R_AARCH64_CALL26 || type == R_AARCH64_JUMP26) {
            int32_t imm26 = (int32_t)((S + A - P) / 4);
            *instr = (*instr & 0xFC000000) | (imm26 & 0x03FFFFFF);
        } 
        else if (type == R_AARCH64_ADR_PREL_LO21) {
            int64_t offset = (int64_t)(S + A - P);
            uint32_t immlo = (uint32_t)(offset & 3);
            uint32_t immhi = (uint32_t)((offset >> 2) & 0x7FFFF);
            // Patch: [30:29] = immlo, [23:5] = immhi
            *instr = (*instr & 0x9F00001F) | (immlo << 29) | (immhi << 5);
        } else if (type == R_AARCH64_CONDBR19) {
            int32_t imm19 = (int32_t)((S + A - P) / 4);
            *instr = (*instr & 0xFF00001F) | (imm19 << 5);
        } else if (type == R_AARCH64_LDST8_ABS_LO12_NC || type == R_AARCH64_LDST16_ABS_LO12_NC ||
                   type == R_AARCH64_LDST32_ABS_LO12_NC || type == R_AARCH64_LDST64_ABS_LO12_NC) {
            uint32_t imm12 = (uint32_t)(S + A - P) & 0xFFF;
            *instr = (*instr & 0xFFFFF000) | imm12;
        } else if (type == R_AARCH64_LDST128_ABS_LO12_NC) {
            uint32_t imm12 = (uint32_t)(S + A - P) & 0xFFF;
            *instr = (*instr & 0xFFFFF000) | imm12;
        } else if (type == R_AARCH64_ADR_GOT_PAGE) {
            int64_t offset = (int64_t)(S + A - P);
            uint32_t immlo = (uint32_t)((offset >> 12) & 3);
            uint32_t immhi = (uint32_t)((offset >> 14) & 0x7FFFF);
            *instr = (*instr & 0x9F00001F) | (immlo << 29) | (immhi << 5);
        } else if (type == R_AARCH64_ABS64) {
            uint64_t* addr_ptr = (uint64_t*)(exec->data_payload + data_offset);
            *addr_ptr = S + A; // Write the absolute address into the data segment
        } else if (type == R_AARCH64_RELATIVE) {
            uint64_t* addr_ptr = (uint64_t*)(exec->data_payload + data_offset);
            *addr_ptr = text_segment_base + total_header_size + text_offset + rel->r_offset + A; // Relative to the load address
        } else if (type == R_AARCH64_PREL32) {
            uint32_t* addr_ptr = (uint32_t*)(exec->data_payload + data_offset);
            *addr_ptr = (uint32_t)(S + A - P); // 32-bit PC-relative offset
        } else if (type == R_AARCH64_PREL64) {
            uint64_t* addr_ptr = (uint64_t*)(exec->data_payload + data_offset);
            *addr_ptr = S + A - P; // 64-bit PC-relative offset
        } else if (type == R_AARCH64_ABS32) {
            uint32_t* addr_ptr = (uint32_t*)(exec->data_payload + data_offset);
            *addr_ptr = (uint32_t)(S + A); // Absolute 32-bit address
        } else if (type == R_AARCH64_ABS16) {
            uint16_t* addr_ptr = (uint16_t*)(exec->data_payload + data_offset);
            *addr_ptr = (uint16_t)(S + A); // Absolute 16-bit address
        } else if (type == R_AARCH64_ADR_PREL_PG_HI21) {
            int64_t offset = (int64_t)(S + A - P);
            uint32_t immlo = (uint32_t)((offset >> 12) & 3);
            uint32_t immhi = (uint32_t)((offset >> 14) & 0x7FFFF);
            *instr = (*instr & 0x9F00001F) | (immlo << 29) | (immhi << 5);
        } else if (type == R_AARCH64_ADD_ABS_LO12_NC) {
            uint32_t imm12 = (uint32_t)(S + A) & 0xFFF;
            *instr = (*instr & 0xFFFFF000) | imm12;
        }
        else {
            printf("Warning: Unsupported relocation type %u\n", type);
        }
    }

    // Update global sizes for the next object to be linked
    exec->text_payload_size += obj_text_bytes;
    exec->data_payload_size += obj_data_bytes;

    // 6. Find _start for Entry Point
    uint32_t sym_count = ax_vecSize(obj->symtab);
    for (uint32_t i = 0; i < sym_count; i++) {
        char* name = obj->strtab + obj->symtab[i].st_name;
        if (strcmp(name, "_start") == 0) {
            exec->entry_point = text_segment_base + total_header_size + text_offset + obj->symtab[i].st_value;
        }
    }
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