/*
code_generator.h

Copyright (c) 24 Yann BOUCHER (yann)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "ast_nodes.h"

#include <stdio.h>

typedef DYNARRAY(const char*) label_list_t;

typedef struct instruction_t
{
    label_list_t labels;
    const char* opcode;
    const char* operand;
    const char* comment;
    struct instruction_t* prev;
    struct instruction_t* next;
} instruction_t;

instruction_t* instruction_list;

extern const char binop_opcodes[POD_TYPES_END][OP_BIN_END][8];
extern const char unary_opcodes[POD_TYPES_END][OP_UNARY_END - OP_BIN_END][8];

void generate_program(program_t* program);
void generate_expression(expression_t* expr);

void generate(const char* op, const char* operand, ...);

#endif // CODE_GENERATOR_H
