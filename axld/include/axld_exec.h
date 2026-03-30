#ifndef AXLD_EXECUTABLE_H
#define AXLD_EXECUTABLE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <elf.h>
#include <ax_vec.h>
#include <ax_ir.h>
#include <axas_object.h>
#include <stdlib.h>

typedef struct {
    Elf64_Ehdr ehdr;
    
    // Segments
    Elf64_Phdr phdr_text; // PT_LOAD: Read/Execute
    Elf64_Phdr phdr_data; // PT_LOAD: Read/Write
    
    // Final monolithic buffers
    uint8_t* text_payload; 
    uint64_t text_payload_size;
    
    uint8_t* data_payload;
    uint64_t data_payload_size;

    uint64_t entry_point; // VAddr of _start

    // Global symbol table for cross-file linking:
    // parallel arrays mapping names -> resolved virtual addresses
    char**    global_sym_names;   // owns the strings (strdup'd)
    uint64_t* global_sym_vaddrs;
} AxExecutable;

void ax_execInit(AxExecutable* exec);

void ax_execFree(AxExecutable* exec);

void ax_execLink(AxExecutable* exec, AxObject* obj);

bool ax_execWrite(AxExecutable* exec, const char* filename);

#endif
