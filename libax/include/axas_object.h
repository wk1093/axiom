#ifndef AXAS_OBJECT_H
#define AXAS_OBJECT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <elf.h>
#include <ax_vec.h>
#include <ax_ir.h>

typedef struct {
    uint32_t* text;    // Instructions
    uint8_t* data;    // Global variables/constants
    char* strtab;  // String table
    Elf64_Sym* symtab; // Symbol table
    Elf64_Rela* reltab; // Relocation table

    Elf64_Ehdr ehdr;
} AxObject;

// Helper to add a string to the strtab and return its offset
uint32_t ax_objectAddString(AxObject* obj, const char* str);

void ax_objectInit(AxObject* obj);
void ax_objectFree(AxObject* obj);

void ax_objectEmit(AxObject* obj, AxIrInstr* instr);

void ax_objectDefineDataLabel(AxObject* obj, const char* name);
void ax_objectDefineCodeLabel(AxObject* obj, const char* name);

// Helper to add a global function symbol (like _start)
void ax_objectAddSymbol(AxObject* obj, const char* name, uint64_t value, uint8_t type);
void ax_objectAddSymbolFull(AxObject* obj, const char* name, uint64_t value, uint8_t type, uint16_t shndx);
void ax_objectAddReloc(AxObject* obj, uint64_t offset, const char* symbol, uint32_t type);
uint32_t ax_objectGetSymbolIndex(AxObject* obj, const char* name);
void ax_objectWrite(AxObject* obj, const char* filename);
bool ax_objectLoad(AxObject* obj, const char* filename);

void ax_printObjectInfo(AxObject* obj);

#endif