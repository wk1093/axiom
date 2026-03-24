#ifndef AXAS_PARSER_H
#define AXAS_PARSER_H

#include "axas_lexer.h"
#include "ax_ir.h"

typedef enum {
    UNIT_LABEL,
    UNIT_INSTR,
    UNIT_DIRECTIVE,
    UNIT_EOF
} AxUnitType;

typedef struct {
    AxUnitType type;
    union {
        char* label;
        struct {
            char mnem[16];
            AxIrArg args[3];
            int arg_count;
            bool is_pre_index;
            bool is_post_index;
        } instr;
        struct {
            char* name;
            char* value;
        } directive;
    };
} AxParsedUnit;

AxParsedUnit ax_parseUnit(AxLexer* l);
void ax_debugUnit(AxParsedUnit unit);

#endif