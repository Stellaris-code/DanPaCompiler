/*
ast_nodes.h

Copyright (c) 19 Yann BOUCHER (yann)

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
#ifndef AST_NODES_H
#define AST_NODES_H

#include "types.h"
#include "lexer.h"
#include "dynarray.h"
#include "hash_table.h"

typedef struct ident_t
{
    token_t* name;

    // semantic info
    type_t type;
    enum
    {
        IDENT_GLOBAL = (1 << 0),
        IDENT_PROCESSED = (1 << 1)
    } flags;
    union
    {
        int local_id;
        int global_id;
    };
} ident_t;

typedef struct string_literal_t
{
    const char* str;
} string_literal_t;

typedef struct local_variable_t
{
    int temp;
    int nest_depth;
    ident_t ident;
} local_variable_t;

typedef struct global_variable_t
{
    ident_t ident;
} global_variable_t;

typedef struct cast_expression_t
{
    token_t* cast_type_token;
    type_t target_type;
    struct primary_expression_t* expr;
} cast_expression_t;

typedef struct array_subscript_t
{
    token_t* bracket_token;
    struct primary_expression_t* array_expr;
    struct expression_t* subscript_expr;
} array_subscript_t;

typedef struct array_slice_t
{
    token_t* bracket_token;
    struct primary_expression_t* array_expr;
    struct expression_t* left_expr;
    struct expression_t* right_expr;
} array_slice_t;

typedef struct struct_access_t
{
    struct primary_expression_t* struct_expr;
    int indirect_access;
    type_t value_type;
    struct
    {
        token_t* field_name;
        struct structure_field_t* field;
    };
} struct_access_t;

typedef struct ternary_expr_t
{
    struct expression_t* cond_expr;
    struct expression_t* true_branch;
    struct expression_t* false_branch;
} ternary_expr_t;

typedef struct unary_expr_t
{
    token_t* unary_op;
    struct primary_expression_t* unary_value;
} unary_expr_t;

typedef struct deref_expr_t
{
    token_t* asterisk_token;
    int is_optional_access;
    struct primary_expression_t* pointer_expr;
} deref_expr_t;

typedef struct addr_expr_t
{
    function_t* addressed_function; // NULL if it isn't a function
    token_t* addr_token;
    struct primary_expression_t* addr_expr;
} addr_expr_t;

typedef struct asm_expr_t
{
    const char* asm_code;
    DYNARRAY(struct expression_t*) arguments;
    type_t ret_type;
} asm_expr_t;

typedef struct null_expr_t
{
} null_expr_t;


typedef struct match_pattern_t
{
    enum
    {
        PAT_INT_LIT,
        PAT_STR_LIT,
        PAT_IDENT,
        PAT_RANGE
    } type;
    union
    {
        token_t* int_constant;
        token_t* string_lit;
        ident_t ident;
        struct
        {
            token_t* left_bound;
            token_t* right_bound;
        };
    };
    type_t value_type;
    source_location_t loc;
    int length;
} match_pattern_t;

typedef struct match_case_t
{
    int is_wildcard;
    DYNARRAY(match_pattern_t) patterns;
    struct expression_t* expr;
    type_t value_type;
    source_location_t loc;
    int length;

    int test_expr_loc_id;
} match_case_t;

typedef struct match_expr_t
{
    struct expression_t* tested_expr;
    DYNARRAY(match_case_t) cases;

    int test_expr_loc_id;
} match_expr_t;

typedef struct sizeof_expr_t
{
    source_location_t loc;
    int length;

    int is_expr;
    struct expression_t* expr;
    type_t type;
} sizeof_expr_t;

typedef DYNARRAY(struct expression_t*) func_arg_list_t;
typedef struct function_call_t
{
    int length;

    int indirect;
    struct builtin_t* builtin; // NULL if not a builtin
    struct primary_expression_t* call_expr;
    function_signature_t* signature;
    func_arg_list_t arguments;
} function_call_t;

typedef struct new_expr_t
{
    source_location_t loc;
    int length;

    type_t new_type;
} new_expr_t;

typedef struct random_expr_t
{
    union
    {
        primary_expression_t* random_expr;
        struct
        {
            primary_expression_t* left_bound;
            primary_expression_t* right_bound;
        };
    };
    int is_range;
    enum
    {
        RAND_INT,
        RAND_FLT,
        RAND_RNG,
        RAND_ARR
    } type;
} random_expr_t;

typedef struct array_range_expr_t
{
    primary_expression_t* left_bound;
    primary_expression_t* right_bound;
} array_range_expr_t;

typedef struct array_lit_expr_t
{
    source_location_t loc;
    int length;

    DYNARRAY(expression_t) elements;
    type_t type;
} array_lit_expr_t;

typedef struct struct_initializer_t
{
    source_location_t loc;
    int length;

    type_t type;
    DYNARRAY(expression_t) elements;
} struct_initializer_t;

typedef struct primary_expression_t
{
    source_location_t loc;
    int length;
    enum
    {
        ENCLOSED,
        UNARY_OP_FACTOR,
        CAST_EXPRESSION,
        IDENT,
        FUNCTION_CALL,
        ARRAY_SUBSCRIPT,
        ARRAY_SLICE,
        ARRAY_RANGE_GEN,
        STRUCT_ACCESS,
        POINTER_DEREF,
        ADDR_GET,
        ASM_EXPR,
        MATCH_EXPR,
        SIZEOF_EXPR,
        NEW_EXPR,
        RAND_EXPR,
        ARRAY_LIT,
        STRUCT_INIT,
        INT_CONSTANT,
        FLOAT_CONSTANT,
        STRING_LITERAL,
        NULL_LITERAL
    } type;
    union
    {
        struct expression_t* expr;
        struct unary_expr_t unary_expr;
        struct cast_expression_t cast_expr;
        struct function_call_t func_call;
        struct struct_access_t struct_access;
        struct array_subscript_t array_sub;
        struct array_slice_t array_slice;
        struct array_range_expr_t array_range;
        struct deref_expr_t deref;
        struct addr_expr_t addr;
        struct asm_expr_t asm_expr;
        struct match_expr_t match_expr;
        struct sizeof_expr_t sizeof_expr;
        struct new_expr_t new_expr;
        struct random_expr_t rand_expr;
        struct array_lit_expr_t array_lit;
        struct struct_initializer_t struct_init;
        struct null_expr_t null;
        token_t* int_constant;
        token_t* flt_constant;
        token_t* string_lit;
        ident_t ident;
    };
    type_t value_type;
} primary_expression_t;

typedef struct assignment_t
{
    primary_expression_t var;
    expression_t* expr;
    token_t* eq_token;
    int discard_result;
} assignment_t;

typedef struct binop_t binop_t;
typedef struct expression_t
{
    source_location_t loc;
    int length;

    enum
    {
        PRIM_EXPR,
        BINOP,
        ASSIGNMENT,
        TERNARY_EXPR
    } kind;
    enum
    {
        IS_BOOL_EXPR = (1 << 0)
    } flags;
    type_t value_type;
    union
    {
        primary_expression_t prim_expr;
        binop_t* binop;
        assignment_t assignment;
        struct ternary_expr_t ternary;
    };
} expression_t;

typedef struct binop_t
{
    expression_t left;
    expression_t right;
    token_t* op;
} binop_t;

typedef struct return_statement_t
{
    int empty_return;
    token_t* return_token;
    expression_t expr;
} return_statement_t;

typedef struct statement_t statement_t;
typedef struct if_statement_t
{
    expression_t test;
    statement_t* statement;
    statement_t* else_statement;
} if_statement_t;

typedef struct while_statement_t
{
    expression_t test;
    statement_t* statement;
} while_statement_t;

typedef struct do_while_statement_t
{
    expression_t test;
    statement_t* statement;
} do_while_statement_t;

typedef struct for_statement_t
{
    statement_t* init_statement;
    expression_t loop_expr;
    expression_t test;

    statement_t* statement;
} for_statement_t;

typedef struct loop_ctrl_statement_t
{
    token_t* tok;
    enum
    {
        LOOP_BREAK,
        LOOP_CONTINUE
    } type;
} loop_ctrl_statement_t;

typedef struct compound_statement_t
{
    DYNARRAY(statement_t) statement_list;
} compound_statement_t;

typedef struct variable_declaration_t
{
    type_t type;
    token_t* name;
    int var_id;
    int global;
    assignment_t* init_assignment;
} variable_declaration_t;

typedef struct typedef_declaration_t
{
    type_t type;
    token_t* name;
} typedef_declaration_t;

typedef struct struct_declaration_t
{
    DYNARRAY(variable_declaration_t) field_decls;
    structure_t structure;
} struct_declaration_t;


typedef struct foreach_statement_t
{
    type_t* loop_var_type; // optional : can be NULL
    ident_t loop_ident;
    expression_t array_expr;
    statement_t* statement;
    int foreach_ref;
    // semanal
    int counter_var_id;
    variable_declaration_t loop_var_decl;
    assignment_t loop_var_assignment;
} foreach_statement_t;


typedef struct declaration_t
{
    enum
    {
        VARIABLE_DECLARATION = 0,
        TYPEDEF_DECLARATION,
        STRUCT_DECLARATION
    } type;
    union
    {
        variable_declaration_t var;
        typedef_declaration_t typedef_decl;
        struct_declaration_t struct_decl;
    };
} declaration_t;

typedef struct statement_t
{
    enum
    {
        RETURN_STATEMENT = 0,
        DECLARATION,
        COMPOUND_STATEMENT,
        EMPTY_STATEMENT,
        IF_STATEMENT,
        WHILE_STATEMENT,
        DO_WHILE_STATEMENT,
        LOOP_CTRL_STATEMENT,
        DISCARDED_EXPRESSION,
        FOR_STATEMENT,
        FOREACH_STATEMENT,

        END_STATEMENT_TYPES
    } type;
    union
    {
        return_statement_t return_statement;
        declaration_t declaration;
        expression_t expression;
        compound_statement_t compound;
        if_statement_t if_statement;
        while_statement_t while_statement;
        do_while_statement_t do_while_statement;
        loop_ctrl_statement_t loop_ctrl_statement;
        for_statement_t for_statement;
        foreach_statement_t foreach_statement;
    };
} statement_t;

typedef struct parameter_t
{
    type_t type;
    token_t* name;
} parameter_t;

typedef struct function_t
{
    token_t* name;
    function_signature_t signature;
    int is_operator_overload;
    operator_type_t overloaded_op;
    DYNARRAY(parameter_t) args;

    DYNARRAY(statement_t) statement_list;
    DYNARRAY(local_variable_t) locals;
} function_t;

typedef struct program_t
{
    DYNARRAY(function_t) function_list;
    DYNARRAY(declaration_t) global_declarations;
    DYNARRAY(global_variable_t) globals;
    hash_table_t  strings;
} program_t;

#endif // AST_NODES_H
