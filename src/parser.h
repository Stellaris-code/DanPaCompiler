/*
parser.h

Copyright (c) 15 Yann BOUCHER (yann)

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
#ifndef PARSER_H
#define PARSER_H

#include "dynarray.h"
#include "types.h"

typedef struct primary_expression_t
{
    enum
    {
        ENCLOSED,
        UNARY_OP_FACTOR,
        IDENT,
        INT_CONSTANT,
        FLOAT_CONSTANT,
        STRING_LITERAL
    } type;
    union
    {
        struct expression_t* expr;
        struct
        {
            int unary_op;
            struct primary_expression_t* unary_value;
        };
        const char* ident;
        int int_constant;
        float flt_constant;
        const char* string_lit;
    };
} primary_expression_t;

typedef struct binop_t binop_t;
typedef struct expression_t
{
    enum
    {
        PRIM_EXPR,
        BINOP
    } type;
    union
    {
        primary_expression_t prim_expr;
        binop_t* binop;
    };
} expression_t;

typedef struct binop_t
{
    expression_t left;
    expression_t right;
    int op;
};

typedef struct assignment_t
{
    const char* var;
    expression_t expr;
} assignment_t;

typedef struct return_statement_t
{
    expression_t expr;
} return_statement_t;

typedef struct declaration_statement_t
{
    type_t type;
    const char* name;
    expression_t* init_value;
} declaration_statement_t;

typedef struct statement_t statement_t;
typedef struct if_statement_t
{
    struct if_cond_pair
    {
        expression_t test;
        statement_t* statement;
    };

    struct if_cond_pair first_pair;
    statement_t* else_statement;
} if_statement_t;

typedef struct statement_t
{
    enum
    {
        RETURN_STATEMENT,
        ASSIGNMENT,
        DECLARATION,
        COMPOUND_STATEMENT,
        IF_STATEMENT
    } type;
    union
    {
        return_statement_t return_statement;
        declaration_statement_t declaration;
        assignment_t assignment;
        DYNARRAY(statement_t) statement_list;
        if_statement_t if_statement;
    };
} statement_t;

typedef struct function_t
{
    type_t type;
    const char* name;
    // TODO : args

    DYNARRAY(statement_t) statement_list;
} function_t;

typedef struct program_t
{
    DYNARRAY(function_t) function_list;
} program_t;

typedef struct token_t token_t;

void set_parser_token_list(token_t* token_list);

type_t get_expression_type(const expression_t* expr);

void parse_program(program_t* program);

#endif // PARSER_H
