#ifndef AXAS_ASSEMBLER_H
#define AXAS_ASSEMBLER_H

#include "axas_lexer.h"
#include "axas_parser.h"
#include "axas_object.h"

void ax_assemble(AxObject* obj, AxLexer* lexer);

#endif