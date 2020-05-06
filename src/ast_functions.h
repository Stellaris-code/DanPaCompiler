/*
ast_functions.h

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

#include "types.h"
#include "ast_nodes.h"

#define AST_FUNCTION_PROTO_(pass_name, name, ...) \
    void pass_name##_##name(name##_t* arg_##name, ##__VA_ARGS__)

#define AST_FUNCTION_PROTO(pass_name, name, ...) \
    AST_FUNCTION_PROTO_(pass_name, name,  ##__VA_ARGS__)

#define AST_PROGRAM(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, function); \
    AST_FUNCTION_PROTO(AST_PASS_NAME, program)
#define AST_PROGRAM_PROCESS_1() AST_PROGRAM_PROCESS_1_(AST_PASS_NAME)
#define AST_PROGRAM_PROCESS_1_(pass_name) AST_PROGRAM_PROCESS_1__(pass_name)
#define AST_PROGRAM_PROCESS_1__(pass_name) \
    for (int i = 0; i < arg_program->global_declarations.size; ++i) \
    { \
        pass_name##_declaration(&arg_program->global_declarations.ptr[i]); \
    }
#define AST_PROGRAM_PROCESS_2() AST_PROGRAM_PROCESS_2_(AST_PASS_NAME)
#define AST_PROGRAM_PROCESS_2_(pass_name) AST_PROGRAM_PROCESS_2__(pass_name)
#define AST_PROGRAM_PROCESS_2__(pass_name) \
    for (int i = 0; i < arg_program->function_list.size; ++i) \
    { \
        pass_name##_function(&arg_program->function_list.ptr[i]); \
    }

#define AST_FUNCTION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, statement); \
    AST_FUNCTION_PROTO(AST_PASS_NAME, expression); \
    AST_FUNCTION_PROTO(AST_PASS_NAME, function)
#define AST_FUNCTION_PROCESS() AST_FUNCTION_PROCESS_(AST_PASS_NAME)
#define AST_FUNCTION_PROCESS_(pass_name) AST_FUNCTION_PROCESS__(pass_name)
#define AST_FUNCTION_PROCESS__(pass_name) \
    for (int i = 0; i < arg_function->args.size; ++i) \
    { \
        pass_name##_type(&arg_function->args.ptr[i].type); \
    } \
    for (int i = 0; i < arg_function->statement_list.size; ++i) \
    { \
        pass_name##_statement(&arg_function->statement_list.ptr[i]); \
    }

#define DEFINE_STATEMENT_HANDLER_3(pass_name, statement_type) \
{(void(*)(void*))&pass_name##_##statement_type}
#define DEFINE_STATEMENT_HANDLER_2(pass_name, statement_type) \
    DEFINE_STATEMENT_HANDLER_3(pass_name, statement_type)
#define DEFINE_STATEMENT_HANDLER(statement_type) \
    DEFINE_STATEMENT_HANDLER_2(AST_PASS_NAME, statement_type)

        typedef struct statement_handler_t
{
    void (*handler)(void*);
} statement_handler_t;

AST_FUNCTION_PROTO(AST_PASS_NAME, return_statement);
AST_FUNCTION_PROTO(AST_PASS_NAME, declaration);
AST_FUNCTION_PROTO(AST_PASS_NAME, if_statement);
AST_FUNCTION_PROTO(AST_PASS_NAME, while_statement);
AST_FUNCTION_PROTO(AST_PASS_NAME, do_while_statement);
AST_FUNCTION_PROTO(AST_PASS_NAME, loop_ctrl_statement);
AST_FUNCTION_PROTO(AST_PASS_NAME, expression);
AST_FUNCTION_PROTO(AST_PASS_NAME, for_statement);
AST_FUNCTION_PROTO(AST_PASS_NAME, foreach_statement);
static statement_handler_t statement_handlers[END_STATEMENT_TYPES] =
    {
        DEFINE_STATEMENT_HANDLER(return_statement),
        DEFINE_STATEMENT_HANDLER(declaration),
        NULL, // compound
        NULL, // empty
        DEFINE_STATEMENT_HANDLER(if_statement),
        DEFINE_STATEMENT_HANDLER(while_statement),
        DEFINE_STATEMENT_HANDLER(do_while_statement),
        DEFINE_STATEMENT_HANDLER(loop_ctrl_statement),
        DEFINE_STATEMENT_HANDLER(expression),
        DEFINE_STATEMENT_HANDLER(for_statement),
        DEFINE_STATEMENT_HANDLER(foreach_statement)
};

#define AST_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, statement)
#define AST_STATEMENT_PROCESS() AST_STATEMENT_PROCESS_(AST_PASS_NAME)
#define AST_STATEMENT_PROCESS_(pass_name) AST_STATEMENT_PROCESS__(pass_name)
#define AST_STATEMENT_PROCESS__(pass_name) \
        /* handle special cases */ \
        if (arg_statement->type == COMPOUND_STATEMENT) \
        { \
            pass_name##_compound_statement(&arg_statement->compound); \
        } \
        else if (arg_statement->type == EMPTY_STATEMENT) \
            ; /* litteraly do nothing */ \
        void(*handler)(void*) = statement_handlers[arg_statement->type].handler; \
        if (!handler) \
            return; \
        handler(&arg_statement->return_statement); /* the field doesn't matter as it's an union */

#define AST_COMPOUND_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, compound_statement)
#define AST_COMPOUND_STATEMENT_PROCESS() AST_COMPOUND_STATEMENT_PROCESS_(AST_PASS_NAME)
#define AST_COMPOUND_STATEMENT_PROCESS_(pass_name) AST_COMPOUND_STATEMENT_PROCESS__(pass_name)
#define AST_COMPOUND_STATEMENT_PROCESS__(pass_name) \
    for (int i = 0; i < arg_compound_statement->statement_list.size; ++i) \
    { \
        pass_name##_statement(&arg_compound_statement->statement_list.ptr[i]); \
    } \

#define AST_RETURN_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, return_statement)
#define AST_RETURN_STATEMENT_PROCESS() AST_RETURN_STATEMENT_PROCESS_(AST_PASS_NAME)
#define AST_RETURN_STATEMENT_PROCESS_(pass_name) AST_RETURN_STATEMENT_PROCESS__(pass_name)
#define AST_RETURN_STATEMENT_PROCESS__(pass_name) \
    if (!arg_return_statement->empty_return) \
        pass_name##_expression(&arg_return_statement->expr);

#define AST_LOOP_CTRL_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, loop_ctrl_statement)
#define AST_LOOP_CTRL_STATEMENT_PROCESS() AST_LOOP_CTRL_STATEMENT_PROCESS_(AST_PASS_NAME)
#define AST_LOOP_CTRL_STATEMENT_PROCESS_(pass_name) AST_LOOP_CTRL_STATEMENT_PROCESS__(pass_name)
#define AST_LOOP_CTRL_STATEMENT_PROCESS__(pass_name) \

#define AST_TYPE(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, type)
#define AST_TYPE_PROCESS() AST_TYPE_PROCESS_(AST_PASS_NAME)
#define AST_TYPE_PROCESS_(pass_name) AST_TYPE_PROCESS__(pass_name)
#define AST_TYPE_PROCESS__(pass_name) \
    switch (arg_type->kind) \
    { \
    case ARRAY: \
        pass_name##_expression(arg_type->array.initial_size); \
        pass_name##_type(arg_type->array.array_type); \
        break;\
    case POINTER: \
        pass_name##_type(arg_type->pointer.pointed_type); \
    }

#define AST_ASSIGNMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, assignment)
#define AST_ASSIGNMENT_PROCESS_1() AST_ASSIGNMENT_PROCESS_1_(AST_PASS_NAME)
#define AST_ASSIGNMENT_PROCESS_1_(pass_name) AST_ASSIGNMENT_PROCESS_1__(pass_name)
#define AST_ASSIGNMENT_PROCESS_1__(pass_name) \
    pass_name##_primary_expression(&arg_assignment->var);
#define AST_ASSIGNMENT_PROCESS_2() AST_ASSIGNMENT_PROCESS_2_(AST_PASS_NAME)
#define AST_ASSIGNMENT_PROCESS_2_(pass_name) AST_ASSIGNMENT_PROCESS_2__(pass_name)
#define AST_ASSIGNMENT_PROCESS_2__(pass_name) \
    pass_name##_expression(arg_assignment->expr);

#define AST_IF_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, statement); /* forward declare */ \
    AST_FUNCTION_PROTO(AST_PASS_NAME, if_statement)
#define AST_IF_STATEMENT_PROCESS_1() AST_IF_STATEMENT_PROCESS_1_(AST_PASS_NAME)
#define AST_IF_STATEMENT_PROCESS_1_(pass_name) AST_IF_STATEMENT_PROCESS_1__(pass_name)
#define AST_IF_STATEMENT_PROCESS_1__(pass_name) \
    pass_name##_expression(&arg_if_statement->test);
#define AST_IF_STATEMENT_PROCESS_2() AST_IF_STATEMENT_PROCESS_2_(AST_PASS_NAME)
#define AST_IF_STATEMENT_PROCESS_2_(pass_name) AST_IF_STATEMENT_PROCESS_2__(pass_name)
#define AST_IF_STATEMENT_PROCESS_2__(pass_name) \
    pass_name##_statement(arg_if_statement->statement);
#define AST_IF_STATEMENT_PROCESS_3() AST_IF_STATEMENT_PROCESS_3_(AST_PASS_NAME)
#define AST_IF_STATEMENT_PROCESS_3_(pass_name) AST_IF_STATEMENT_PROCESS_3__(pass_name)
#define AST_IF_STATEMENT_PROCESS_3__(pass_name) \
    if (arg_if_statement->else_statement) \
        pass_name##_statement(arg_if_statement->else_statement);

#define AST_DO_WHILE_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, do_while_statement)
#define AST_DO_WHILE_STATEMENT_PROCESS_1() AST_DO_WHILE_STATEMENT_PROCESS_1_(AST_PASS_NAME)
#define AST_DO_WHILE_STATEMENT_PROCESS_1_(pass_name) AST_DO_WHILE_STATEMENT_PROCESS_1__(pass_name)
#define AST_DO_WHILE_STATEMENT_PROCESS_1__(pass_name) \
    pass_name##_statement(arg_do_while_statement->statement);
#define AST_DO_WHILE_STATEMENT_PROCESS_2() AST_DO_WHILE_STATEMENT_PROCESS_2_(AST_PASS_NAME)
#define AST_DO_WHILE_STATEMENT_PROCESS_2_(pass_name) AST_DO_WHILE_STATEMENT_PROCESS_2__(pass_name)
#define AST_DO_WHILE_STATEMENT_PROCESS_2__(pass_name) \
    pass_name##_expression(&arg_do_while_statement->test);

#define AST_FOR_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, for_statement)
#define AST_FOR_STATEMENT_PROCESS_INIT() AST_FOR_STATEMENT_PROCESS_INIT_(AST_PASS_NAME)
#define AST_FOR_STATEMENT_PROCESS_INIT_(pass_name) AST_FOR_STATEMENT_PROCESS_INIT__(pass_name)
#define AST_FOR_STATEMENT_PROCESS_INIT__(pass_name) \
    pass_name##_statement(arg_for_statement->init_statement);
#define AST_FOR_STATEMENT_PROCESS_TEST() AST_FOR_STATEMENT_PROCESS_TEST_(AST_PASS_NAME)
#define AST_FOR_STATEMENT_PROCESS_TEST_(pass_name) AST_FOR_STATEMENT_PROCESS_TEST__(pass_name)
#define AST_FOR_STATEMENT_PROCESS_TEST__(pass_name) \
    pass_name##_expression(&arg_for_statement->test);
#define AST_FOR_STATEMENT_PROCESS_LOOP() AST_FOR_STATEMENT_PROCESS_LOOP_(AST_PASS_NAME)
#define AST_FOR_STATEMENT_PROCESS_LOOP_(pass_name) AST_FOR_STATEMENT_PROCESS_LOOP__(pass_name)
#define AST_FOR_STATEMENT_PROCESS_LOOP__(pass_name) \
    pass_name##_expression(&arg_for_statement->loop_expr);
#define AST_FOR_STATEMENT_PROCESS_BODY() AST_FOR_STATEMENT_PROCESS_BODY_(AST_PASS_NAME)
#define AST_FOR_STATEMENT_PROCESS_BODY_(pass_name) AST_FOR_STATEMENT_PROCESS_BODY__(pass_name)
#define AST_FOR_STATEMENT_PROCESS_BODY__(pass_name) \
    pass_name##_statement(arg_for_statement->statement);

#define AST_FOREACH_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, foreach_statement)
#define AST_FOREACH_STATEMENT_PROCESS_IDENT() AST_FOREACH_STATEMENT_PROCESS_IDENT_(AST_PASS_NAME)
#define AST_FOREACH_STATEMENT_PROCESS_IDENT_(pass_name) AST_FOREACH_STATEMENT_PROCESS_IDENT__(pass_name)
#define AST_FOREACH_STATEMENT_PROCESS_IDENT__(pass_name) \
    pass_name##_ident(&arg_foreach_statement->loop_ident);
#define AST_FOREACH_STATEMENT_PROCESS_ARRAY() AST_FOREACH_STATEMENT_PROCESS_ARRAY_(AST_PASS_NAME)
#define AST_FOREACH_STATEMENT_PROCESS_ARRAY_(pass_name) AST_FOREACH_STATEMENT_PROCESS_ARRAY__(pass_name)
#define AST_FOREACH_STATEMENT_PROCESS_ARRAY__(pass_name) \
    pass_name##_expression(&arg_foreach_statement->array_expr);
#define AST_FOREACH_STATEMENT_PROCESS_BODY() AST_FOREACH_STATEMENT_PROCESS_BODY_(AST_PASS_NAME)
#define AST_FOREACH_STATEMENT_PROCESS_BODY_(pass_name) AST_FOREACH_STATEMENT_PROCESS_BODY__(pass_name)
#define AST_FOREACH_STATEMENT_PROCESS_BODY__(pass_name) \
    pass_name##_statement(arg_foreach_statement->statement);

#define AST_WHILE_STATEMENT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, while_statement)
#define AST_WHILE_STATEMENT_PROCESS_1() AST_WHILE_STATEMENT_PROCESS_1_(AST_PASS_NAME)
#define AST_WHILE_STATEMENT_PROCESS_1_(pass_name) AST_WHILE_STATEMENT_PROCESS_1__(pass_name)
#define AST_WHILE_STATEMENT_PROCESS_1__(pass_name) \
    pass_name##_expression(&arg_while_statement->test);
#define AST_WHILE_STATEMENT_PROCESS_2() AST_WHILE_STATEMENT_PROCESS_2_(AST_PASS_NAME)
#define AST_WHILE_STATEMENT_PROCESS_2_(pass_name) AST_WHILE_STATEMENT_PROCESS_2__(pass_name)
#define AST_WHILE_STATEMENT_PROCESS_2__(pass_name) \
    pass_name##_statement(arg_while_statement->statement);

#define AST_VARIABLE_DECLARATION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, variable_declaration)
#define AST_VARIABLE_DECLARATION_PROCESS() AST_VARIABLE_DECLARATION_PROCESS_(AST_PASS_NAME)
#define AST_VARIABLE_DECLARATION_PROCESS_(pass_name) AST_VARIABLE_DECLARATION_PROCESS__(pass_name)
#define AST_VARIABLE_DECLARATION_PROCESS__(pass_name) \
    pass_name##_type(&arg_variable_declaration->type); \
    if (arg_variable_declaration->init_assignment) \
        pass_name##_assignment(arg_variable_declaration->init_assignment);

#define AST_TYPEDEF_DECLARATION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, typedef_declaration)
#define AST_TYPEDEF_DECLARATION_PROCESS() AST_TYPEDEF_DECLARATION_PROCESS_(AST_PASS_NAME)
#define AST_TYPEDEF_DECLARATION_PROCESS_(pass_name) AST_TYPEDEF_DECLARATION_PROCESS__(pass_name)
#define AST_TYPEDEF_DECLARATION_PROCESS__(pass_name) \
    pass_name##_type(&arg_typedef_declaration->type);

#define AST_STRUCT_DECLARATION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, struct_declaration)
#define AST_STRUCT_DECLARATION_PROCESS() AST_STRUCT_DECLARATION_PROCESS_(AST_PASS_NAME)
#define AST_STRUCT_DECLARATION_PROCESS_(pass_name) AST_STRUCT_DECLARATION_PROCESS__(pass_name)
#define AST_STRUCT_DECLARATION_PROCESS__(pass_name) \

#define AST_DECLARATION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, declaration)
#define AST_DECLARATION_PROCESS() AST_DECLARATION_PROCESS_(AST_PASS_NAME)
#define AST_DECLARATION_PROCESS_(pass_name) AST_DECLARATION_PROCESS__(pass_name)
#define AST_DECLARATION_PROCESS__(pass_name) \
    switch(arg_declaration->type) \
    { \
        case VARIABLE_DECLARATION: \
            pass_name##_variable_declaration(&arg_declaration->var); \
            break; \
        case TYPEDEF_DECLARATION: \
            pass_name##_typedef_declaration(&arg_declaration->typedef_decl); \
            break; \
        case STRUCT_DECLARATION: \
            pass_name##_struct_declaration(&arg_declaration->struct_decl); \
    }

#define AST_ARRAY_SUBSCRIPT(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, array_subscript)
#define AST_ARRAY_SUBSCRIPT_PROCESS_1() AST_ARRAY_SUBSCRIPT_PROCESS_1_(AST_PASS_NAME)
#define AST_ARRAY_SUBSCRIPT_PROCESS_1_(pass_name) AST_ARRAY_SUBSCRIPT_PROCESS_1__(pass_name)
#define AST_ARRAY_SUBSCRIPT_PROCESS_1__(pass_name) \
    pass_name##_primary_expression(arg_array_subscript->array_expr);
#define AST_ARRAY_SUBSCRIPT_PROCESS_2() AST_ARRAY_SUBSCRIPT_PROCESS_2_(AST_PASS_NAME)
#define AST_ARRAY_SUBSCRIPT_PROCESS_2_(pass_name) AST_ARRAY_SUBSCRIPT_PROCESS_2__(pass_name)
#define AST_ARRAY_SUBSCRIPT_PROCESS_2__(pass_name) \
    pass_name##_expression(arg_array_subscript->subscript_expr);

#define AST_ARRAY_SLICE(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, array_slice)
#define AST_ARRAY_SLICE_PROCESS_1() AST_ARRAY_SLICE_PROCESS_1_(AST_PASS_NAME)
#define AST_ARRAY_SLICE_PROCESS_1_(pass_name) AST_ARRAY_SLICE_PROCESS_1__(pass_name)
#define AST_ARRAY_SLICE_PROCESS_1__(pass_name) \
    pass_name##_primary_expression(arg_array_slice->array_expr);
#define AST_ARRAY_SLICE_PROCESS_2() AST_ARRAY_SLICE_PROCESS_2_(AST_PASS_NAME)
#define AST_ARRAY_SLICE_PROCESS_2_(pass_name) AST_ARRAY_SLICE_PROCESS_2__(pass_name)
#define AST_ARRAY_SLICE_PROCESS_2__(pass_name) \
    pass_name##_expression(arg_array_slice->left_expr);
#define AST_ARRAY_SLICE_PROCESS_3() AST_ARRAY_SLICE_PROCESS_3_(AST_PASS_NAME)
#define AST_ARRAY_SLICE_PROCESS_3_(pass_name) AST_ARRAY_SLICE_PROCESS_3__(pass_name)
#define AST_ARRAY_SLICE_PROCESS_3__(pass_name) \
    pass_name##_expression(arg_array_slice->right_expr);

#define AST_STRUCT_ACCESS(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, struct_access)
#define AST_STRUCT_ACCESS_PROCESS() AST_STRUCT_ACCESS_PROCESS_(AST_PASS_NAME)
#define AST_STRUCT_ACCESS_PROCESS_(pass_name) AST_STRUCT_ACCESS_PROCESS__(pass_name)
#define AST_STRUCT_ACCESS_PROCESS__(pass_name) \
    pass_name##_primary_expression(arg_struct_access->struct_expr);

#define AST_EXPRESSION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, expression)
#define AST_EXPRESSION_PROCESS() AST_EXPRESSION_PROCESS_(AST_PASS_NAME)
#define AST_EXPRESSION_PROCESS_(pass_name) AST_EXPRESSION_PROCESS__(pass_name)
#define AST_EXPRESSION_PROCESS__(pass_name) \
    if (arg_expression->kind == PRIM_EXPR) \
        pass_name##_primary_expression(&arg_expression->prim_expr); \
    else if (arg_expression->kind == BINOP) \
        pass_name##_binop(arg_expression->binop); \
    else if (arg_expression->kind == ASSIGNMENT) \
        pass_name##_assignment(&arg_expression->assignment); \
    else if (arg_expression->kind == TERNARY_EXPR) \
        pass_name##_ternary_expr(&arg_expression->ternary); \

#define AST_DEREF_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, deref_expr)
#define AST_DEREF_EXPR_PROCESS() AST_DEREF_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_DEREF_EXPR_PROCESS_(pass_name) AST_DEREF_EXPR_PROCESS__(pass_name)
#define AST_DEREF_EXPR_PROCESS__(pass_name) \
    pass_name##_primary_expression(arg_deref_expr->pointer_expr);

#define AST_ADDR_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, addr_expr)
#define AST_ADDR_EXPR_PROCESS() AST_ADDR_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_ADDR_EXPR_PROCESS_(pass_name) AST_ADDR_EXPR_PROCESS__(pass_name)
#define AST_ADDR_EXPR_PROCESS__(pass_name) \
    pass_name##_primary_expression(arg_addr_expr->addr_expr);

#define AST_ASM_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, asm_expr)
#define AST_ASM_EXPR_PROCESS() AST_ASM_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_ASM_EXPR_PROCESS_(pass_name) AST_ASM_EXPR_PROCESS__(pass_name)
#define AST_ASM_EXPR_PROCESS__(pass_name) \
    for (int i = 0; i < arg_asm_expr->arguments.size; ++i) \
        pass_name##_expression(arg_asm_expr->arguments.ptr[i]);

// no process() for this one because it is quite complicated to handle
#define AST_MATCH_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, match_expr)
#define AST_MATCH_CASE(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, match_case)
#define AST_MATCH_PATTERN(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, match_pattern)

#define AST_SIZEOF_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, sizeof_expr)
#define AST_SIZEOF_EXPR_PROCESS() AST_SIZEOF_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_SIZEOF_EXPR_PROCESS_(pass_name) AST_SIZEOF_EXPR_PROCESS__(pass_name)
#define AST_SIZEOF_EXPR_PROCESS__(pass_name) \
    if (arg_sizeof_expr->is_expr) \
        pass_name##_expression(arg_sizeof_expr->expr); \
    else \
        pass_name##_type(&arg_sizeof_expr->type);


#define AST_NEW_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, new_expr)
#define AST_NEW_EXPR_PROCESS() AST_NEW_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_NEW_EXPR_PROCESS_(pass_name) AST_NEW_EXPR_PROCESS__(pass_name)
#define AST_NEW_EXPR_PROCESS__(pass_name) \
        pass_name##_type(&arg_new_expr->new_type);

#define AST_RAND_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, random_expr)
#define AST_RAND_EXPR_PROCESS() AST_RAND_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_RAND_EXPR_PROCESS_(pass_name) AST_RAND_EXPR_PROCESS__(pass_name)
#define AST_RAND_EXPR_PROCESS__(pass_name) \
    if (arg_random_expr->is_range) \
    { \
        pass_name##_primary_expression(arg_random_expr->left_bound); \
        pass_name##_primary_expression(arg_random_expr->right_bound); \
    } \
    else \
        pass_name##_primary_expression(arg_random_expr->random_expr);

#define AST_ARRAY_LIT_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, array_lit_expr)
#define AST_ARRAY_LIT_EXPR_PROCESS() AST_ARRAY_LIT_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_ARRAY_LIT_EXPR_PROCESS_(pass_name) AST_ARRAY_LIT_EXPR_PROCESS__(pass_name)
#define AST_ARRAY_LIT_EXPR_PROCESS__(pass_name) \
    /* process in FIFO order */ \
    for (int i = 0; i < arg_array_lit_expr->elements.size; ++i) \
        pass_name##_expression(&arg_array_lit_expr->elements.ptr[arg_array_lit_expr->elements.size-1 - i]);

#define AST_ARRAY_RANGE_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, array_range_expr)
#define AST_ARRAY_RANGE_EXPR_PROCESS_1() AST_ARRAY_RANGE_EXPR_PROCESS_1_(AST_PASS_NAME)
#define AST_ARRAY_RANGE_EXPR_PROCESS_1_(pass_name) AST_ARRAY_RANGE_EXPR_PROCESS_1__(pass_name)
#define AST_ARRAY_RANGE_EXPR_PROCESS_1__(pass_name) \
        pass_name##_primary_expression(arg_array_range_expr->left_bound);
#define AST_ARRAY_RANGE_EXPR_PROCESS_2() AST_ARRAY_RANGE_EXPR_PROCESS_2_(AST_PASS_NAME)
#define AST_ARRAY_RANGE_EXPR_PROCESS_2_(pass_name) AST_ARRAY_RANGE_EXPR_PROCESS_2__(pass_name)
#define AST_ARRAY_RANGE_EXPR_PROCESS_2__(pass_name) \
        pass_name##_primary_expression(arg_array_range_expr->right_bound);

#define AST_STRUCT_INIT_EXPR(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, struct_initializer)
#define AST_STRUCT_INIT_EXPR_PROCESS() AST_STRUCT_INIT_EXPR_PROCESS_(AST_PASS_NAME)
#define AST_STRUCT_INIT_EXPR_PROCESS_(pass_name) AST_STRUCT_INIT_EXPR_PROCESS__(pass_name)
#define AST_STRUCT_INIT_EXPR_PROCESS__(pass_name) \
    for (int i = 0; i < arg_struct_initializer->elements.size; ++i) \
        pass_name##_expression(&arg_struct_initializer->elements.ptr[i]);

#define AST_PRIM_EXPRESSION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, primary_expression)
#define AST_PRIM_EXPRESSION_PROCESS() AST_PRIM_EXPRESSION_PROCESS_(AST_PASS_NAME)
#define AST_PRIM_EXPRESSION_PROCESS_(pass_name) AST_PRIM_EXPRESSION_PROCESS__(pass_name)
#define AST_PRIM_EXPRESSION_PROCESS__(pass_name) \
    switch (arg_primary_expression->type) \
    { \
        case ENCLOSED: \
            pass_name##_expression(arg_primary_expression->expr);\
            break; \
        case UNARY_OP_FACTOR: \
            pass_name##_unary_expr(&arg_primary_expression->unary_expr); \
            break; \
        case CAST_EXPRESSION: \
            pass_name##_cast_expression(&arg_primary_expression->cast_expr); \
            break; \
        case IDENT: \
            pass_name##_ident(&arg_primary_expression->ident); \
            break; \
        case ARRAY_SUBSCRIPT: \
            pass_name##_array_subscript(&arg_primary_expression->array_sub); \
            break; \
        case ARRAY_SLICE: \
            pass_name##_array_slice(&arg_primary_expression->array_slice); \
            break; \
        case ARRAY_RANGE_GEN: \
            pass_name##_array_range_expr(&arg_primary_expression->array_range); \
            break; \
        case STRUCT_ACCESS: \
            pass_name##_struct_access(&arg_primary_expression->struct_access); \
            break; \
        case POINTER_DEREF: \
            pass_name##_deref_expr(&arg_primary_expression->deref); \
            break; \
        case ADDR_GET: \
            pass_name##_addr_expr(&arg_primary_expression->addr); \
            break; \
        case MATCH_EXPR: \
            pass_name##_match_expr(&arg_primary_expression->match_expr); \
            break; \
        case FUNCTION_CALL: \
            pass_name##_function_call(&arg_primary_expression->func_call); \
            break; \
        case ASM_EXPR: \
            pass_name##_asm_expr(&arg_primary_expression->asm_expr); \
            break; \
        case SIZEOF_EXPR: \
            pass_name##_sizeof_expr(&arg_primary_expression->sizeof_expr); \
            break; \
        case NEW_EXPR: \
            pass_name##_new_expr(&arg_primary_expression->new_expr); \
            break; \
        case RAND_EXPR: \
            pass_name##_random_expr(&arg_primary_expression->rand_expr); \
            break; \
        case ARRAY_LIT: \
            pass_name##_array_lit_expr(&arg_primary_expression->array_lit); \
            break; \
        case STRUCT_INIT: \
            pass_name##_struct_initializer(&arg_primary_expression->struct_init); \
            break; \
        case INT_CONSTANT: \
            pass_name##_int_constant(arg_primary_expression->int_constant); \
            break; \
        case FLOAT_CONSTANT: \
            pass_name##_float_constant(arg_primary_expression->flt_constant); \
            break; \
        case STRING_LITERAL:\
            pass_name##_string_literal(arg_primary_expression->string_lit);\
            break; \
        case NULL_LITERAL:\
            pass_name##_null_expr(&arg_primary_expression->null);\
            break; \
     }

#define AST_UNARY_EXPRESSION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, unary_expr)
#define AST_UNARY_EXPRESSION_PROCESS() AST_UNARY_EXPRESSION_PROCESS_(AST_PASS_NAME)
#define AST_UNARY_EXPRESSION_PROCESS_(pass_name) AST_UNARY_EXPRESSION_PROCESS__(pass_name)
#define AST_UNARY_EXPRESSION_PROCESS__(pass_name) \
    pass_name##_primary_expression(arg_unary_expr->unary_value);

#define AST_TERNARY_EXPRESSION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, ternary_expr)
#define AST_TERNARY_EXPRESSION_PROCESS_1() AST_TERNARY_EXPRESSION_PROCESS_1_(AST_PASS_NAME)
#define AST_TERNARY_EXPRESSION_PROCESS_1_(pass_name) AST_TERNARY_EXPRESSION_PROCESS_1__(pass_name)
#define AST_TERNARY_EXPRESSION_PROCESS_1__(pass_name) \
    pass_name##_expression(arg_ternary_expr->cond_expr);
#define AST_TERNARY_EXPRESSION_PROCESS_2() AST_TERNARY_EXPRESSION_PROCESS_2_(AST_PASS_NAME)
#define AST_TERNARY_EXPRESSION_PROCESS_2_(pass_name) AST_TERNARY_EXPRESSION_PROCESS_2__(pass_name)
#define AST_TERNARY_EXPRESSION_PROCESS_2__(pass_name) \
    pass_name##_expression(arg_ternary_expr->true_branch);
#define AST_TERNARY_EXPRESSION_PROCESS_3() AST_TERNARY_EXPRESSION_PROCESS_3_(AST_PASS_NAME)
#define AST_TERNARY_EXPRESSION_PROCESS_3_(pass_name) AST_TERNARY_EXPRESSION_PROCESS_3__(pass_name)
#define AST_TERNARY_EXPRESSION_PROCESS_3__(pass_name) \
    pass_name##_expression(arg_ternary_expr->false_branch);

#define AST_FUNC_CALL_EXPRESSION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, function_call)
#define AST_FUNC_CALL_EXPRESSION_PROCESS_1() AST_FUNC_CALL_EXPRESSION_PROCESS_1_(AST_PASS_NAME)
#define AST_FUNC_CALL_EXPRESSION_PROCESS_1_(pass_name) AST_FUNC_CALL_EXPRESSION_PROCESS_1__(pass_name)
#define AST_FUNC_CALL_EXPRESSION_PROCESS_1__(pass_name) \
        pass_name##_primary_expression(arg_function_call->call_expr);
#define AST_FUNC_CALL_EXPRESSION_PROCESS_2() AST_FUNC_CALL_EXPRESSION_PROCESS_2_(AST_PASS_NAME)
#define AST_FUNC_CALL_EXPRESSION_PROCESS_2_(pass_name) AST_FUNC_CALL_EXPRESSION_PROCESS_2__(pass_name)
#define AST_FUNC_CALL_EXPRESSION_PROCESS_2__(pass_name) \
    for (int i = 0; i < arg_function_call->arguments.size; ++i) \
        pass_name##_expression(arg_function_call->arguments.ptr[i]);

#define AST_CAST_EXPRESSION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, cast_expression)
#define AST_CAST_EXPRESSION_PROCESS() AST_CAST_EXPRESSION_PROCESS_(AST_PASS_NAME)
#define AST_CAST_EXPRESSION_PROCESS_(pass_name) AST_CAST_EXPRESSION_PROCESS__(pass_name)
#define AST_CAST_EXPRESSION_PROCESS__(pass_name) \
    pass_name##_type(&arg_cast_expression->target_type);\
    pass_name##_primary_expression(arg_cast_expression->expr);

#define AST_NULL_EXPRESSION(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, null_expr)

#define AST_BINOP(...) \
    AST_FUNCTION_PROTO(AST_PASS_NAME, binop)
#define AST_BINOP_PROCESS_1() AST_BINOP_PROCESS_1_(AST_PASS_NAME)
#define AST_BINOP_PROCESS_1_(pass_name) AST_BINOP_PROCESS_1__(pass_name)
#define AST_BINOP_PROCESS_1__(pass_name) \
    pass_name##_expression(&arg_binop->left);
#define AST_BINOP_PROCESS_2() AST_BINOP_PROCESS_2_(AST_PASS_NAME)
#define AST_BINOP_PROCESS_2_(pass_name) AST_BINOP_PROCESS_2__(pass_name)
#define AST_BINOP_PROCESS_2__(pass_name) \
    pass_name##_expression(&arg_binop->right);
