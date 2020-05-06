/*
ast_printer.c

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

#include <stdio.h>
#include <stdarg.h>

#include "ast_nodes.h"
#include "ast_printer.h"
#include "lexer.h"

        static int tab = 0;

static void printf_tab(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    printf("%*s", 2*tab, ""); // print tab spaces
    vprintf(fmt, args);

    va_end(args);
}

#define AST_PASS_NAME print
#include "ast_functions.h"

// atoms
static void print_ident(ident_t* ident)
{
    printf_tab("Identifier %s <%s, id %d>\n", ident->name->data.str, type_to_str(&ident->type), ident->local_id);
}

static void print_int_constant(token_t* value)
{
    printf_tab("Integer %d\n", value->data.integer);
}

static void print_float_constant(token_t* value)
{
    printf_tab("Float %f\n", value->data.fp);
}

static void print_string_literal(token_t* value)
{
    printf_tab("String \"%s\"\n", value->data.str);
}

AST_PROGRAM()
{
    printf_tab("Program :\n", "");
    AST_PROGRAM_PROCESS_1();
    AST_PROGRAM_PROCESS_2();
}

AST_TYPE()
{
    ++tab;

    printf_tab("Type %s \n", type_to_str(arg_type));

    --tab;
}

AST_FUNCTION()
{
    ++tab;
    printf_tab("Function %s %s(", type_to_str(&arg_function->signature.ret_type), arg_function->name->data.str);
    for (int i = 0; i < arg_function->signature.parameter_types.size; ++i)
    {
        printf("%s", type_to_str(&arg_function->signature.parameter_types.ptr[i]));
        if (i != (arg_function->signature.parameter_types.size - 1))
            printf(", ");
    }
    printf(")\n");
    ++tab;
    for (int i = 0; i < arg_function->args.size; ++i)
    {
        parameter_t param = arg_function->args.ptr[i];
        printf_tab("Argument %s %s\n", type_to_str(&param.type), param.name->data.str);
    }
    --tab;

    AST_FUNCTION_PROCESS();

    --tab;
}

AST_RETURN_STATEMENT()
{
    ++tab;

    printf_tab("Return statement :\n");
    AST_RETURN_STATEMENT_PROCESS();

    --tab;
}

AST_WHILE_STATEMENT()
{
    ++tab;

    printf_tab("While statement :\n");
    AST_WHILE_STATEMENT_PROCESS_1();
    AST_WHILE_STATEMENT_PROCESS_2();

    --tab;
}

AST_DO_WHILE_STATEMENT()
{
    ++tab;

    printf_tab("Do-while statement :\n");
    AST_DO_WHILE_STATEMENT_PROCESS_1();
    AST_DO_WHILE_STATEMENT_PROCESS_2();

    --tab;
}

AST_IF_STATEMENT()
{
    ++tab;

    printf_tab("If :\n");
    AST_IF_STATEMENT_PROCESS_1();
    printf_tab("Then :\n");
    AST_IF_STATEMENT_PROCESS_2();

    if (arg_if_statement->else_statement)
    {
        printf_tab("Else:\n");
        AST_IF_STATEMENT_PROCESS_3();
    }

    --tab;
}

AST_ASM_EXPR()
{
    ++tab;

    printf_tab("Asm code : \n");
    printf_tab("---\n");
    printf("%s\n", arg_asm_expr->asm_code);
    printf_tab("---\n");
    printf_tab("Arguments :\n");
    AST_ASM_EXPR_PROCESS();

    --tab;
}

AST_NEW_EXPR()
{
    ++tab;

    printf_tab("New expression :\n");
    AST_NEW_EXPR_PROCESS();

    --tab;
}

AST_TYPEDEF_DECLARATION()
{
    ++tab;

    printf_tab("Typedef %s = %s\n", arg_typedef_declaration->name->data.str, type_to_str(&arg_typedef_declaration->type));
    AST_TYPEDEF_DECLARATION_PROCESS();

    --tab;
}

AST_VARIABLE_DECLARATION()
{
    ++tab;

    printf_tab("Declaration %s of type %s\n", arg_variable_declaration->name->data.str, type_to_str(&arg_variable_declaration->type));
    AST_VARIABLE_DECLARATION_PROCESS();

    --tab;
}

AST_STRUCT_DECLARATION()
{
    ++tab;
    printf_tab("Structure %s (size : %d)\n", arg_struct_declaration->structure.name->data.str, arg_struct_declaration->structure.byte_size);
    ++tab;

    for (int i = 0; i < arg_struct_declaration->structure.fields.size; ++i)
    {
        structure_field_t field = arg_struct_declaration->structure.fields.ptr[i];
        printf_tab("Field %s %s (size %d, offset %d)\n",type_to_str(&field.type), field.name->data.str, field.byte_size, field.offset);
    }

    --tab;
    AST_STRUCT_DECLARATION_PROCESS();
    --tab;
}

AST_DECLARATION()
{
    ++tab;

    AST_DECLARATION_PROCESS();

    --tab;
}

AST_FOR_STATEMENT()
{
    ++tab;

    printf_tab("For statement :\n");

    AST_FOR_STATEMENT_PROCESS_INIT();
    AST_FOR_STATEMENT_PROCESS_TEST();
    AST_FOR_STATEMENT_PROCESS_LOOP();

    AST_FOR_STATEMENT_PROCESS_BODY();
    --tab;
}

AST_FOREACH_STATEMENT()
{
    ++tab;

    printf_tab("Foreach statement :\n");

    AST_FOREACH_STATEMENT_PROCESS_IDENT();
    AST_FOREACH_STATEMENT_PROCESS_ARRAY();

    AST_FOREACH_STATEMENT_PROCESS_BODY();
    --tab;
}


AST_ASSIGNMENT()
{
    AST_ASSIGNMENT_PROCESS_1();
    ++tab;
    printf_tab("=\n");
    --tab;
    AST_ASSIGNMENT_PROCESS_2();
}

AST_LOOP_CTRL_STATEMENT()
{
    ++tab;
    printf_tab("%s\n", (arg_loop_ctrl_statement->type == LOOP_BREAK ? "Break" : "Continue"));
    AST_LOOP_CTRL_STATEMENT_PROCESS();
    --tab;
}

AST_COMPOUND_STATEMENT()
{
    AST_COMPOUND_STATEMENT_PROCESS();
}

AST_STATEMENT()
{
    ++tab;
    AST_STATEMENT_PROCESS();
    --tab;
}

AST_BINOP()
{
    ++tab;

    printf_tab("Binop :\n");
    AST_BINOP_PROCESS_1();
    printf_tab("'%s' \n", operators[arg_binop->op->data.op].str);
    AST_BINOP_PROCESS_2();

    --tab;
}

AST_ARRAY_SUBSCRIPT()
{
    ++tab;

    printf_tab("Array access : \n");
    AST_ARRAY_SUBSCRIPT_PROCESS_1();
    printf_tab("Index :\n");
    AST_ARRAY_SUBSCRIPT_PROCESS_2();

    --tab;
}

AST_ARRAY_SLICE()
{
    ++tab;

    printf_tab("Array access : \n");
    AST_ARRAY_SLICE_PROCESS_1();
    printf_tab("Left bound :\n");
    AST_ARRAY_SLICE_PROCESS_2();
    printf_tab("Right bound :\n");
    AST_ARRAY_SLICE_PROCESS_3();

    --tab;
}

AST_STRUCT_ACCESS()
{
    ++tab;

    printf_tab("Field access '%s' :\n", arg_struct_access->field_name->data.str);
    AST_STRUCT_ACCESS_PROCESS();

    --tab;
}

AST_STRUCT_INIT_EXPR()
{
    ++tab;

    printf_tab("Struct initializer for <%s>\n", type_to_str(&arg_struct_initializer->type));
    AST_STRUCT_INIT_EXPR_PROCESS();

    --tab;
}

AST_DEREF_EXPR()
{
    ++tab;

    printf_tab("Pointer dereference :\n");
    AST_DEREF_EXPR_PROCESS();

    --tab;
}

AST_ADDR_EXPR()
{
    ++tab;

    if (arg_addr_expr->addressed_function)
    {
        printf_tab("Address of function %s\n", arg_addr_expr->addressed_function->name->data.str);
    }
    else
    {
        printf_tab("Address get :\n");
        AST_ADDR_EXPR_PROCESS();
    }

    --tab;
}

AST_SIZEOF_EXPR()
{
    ++tab;

    printf_tab("Sizeof :\n");
    AST_SIZEOF_EXPR_PROCESS();

    --tab;
}

AST_UNARY_EXPRESSION()
{
    ++tab;

    printf_tab("Unary expr :\n");
    if (arg_unary_expr->unary_op->type == TOK_QUESTION)
        printf_tab("'?'\n");
    else
        printf_tab("'%s'\n", operators[arg_unary_expr->unary_op->data.op].str);

    AST_UNARY_EXPRESSION_PROCESS();

    --tab;
}

AST_FUNC_CALL_EXPRESSION()
{
    ++tab;

    if (arg_function_call->indirect)
    {
        printf_tab("Indirect function call :\n");
        AST_FUNC_CALL_EXPRESSION_PROCESS_1();
    }
    else if (arg_function_call->builtin)
    {
        printf_tab("Builtin call '%s' :\n", arg_function_call->call_expr->ident.name->data.str);
    }
    else
    {
        printf_tab("Function call : %s\n", arg_function_call->call_expr->ident.name->data.str);
    }
    if (arg_function_call->arguments.size != 0)
    {
        printf_tab("Arguments :\n");
        AST_FUNC_CALL_EXPRESSION_PROCESS_2();
    }

    --tab;
}

AST_CAST_EXPRESSION()
{
    ++tab;

    printf_tab("Casting to type '<%s>'\n", type_to_str(&arg_cast_expression->target_type));
    AST_CAST_EXPRESSION_PROCESS();

    --tab;
}

AST_MATCH_PATTERN()
{
    ++tab;

    switch (arg_match_pattern->type)
    {
        case PAT_INT_LIT:
            print_int_constant(arg_match_pattern->int_constant);
            break;
        case PAT_STR_LIT:
            print_string_literal(arg_match_pattern->string_lit);
            break;
        case PAT_IDENT:
            print_ident(&arg_match_pattern->ident);
            break;
        case PAT_RANGE:
            print_int_constant(arg_match_pattern->left_bound);
            printf_tab("..\n");
            print_int_constant(arg_match_pattern->right_bound);
            break;
            break;
    }

    --tab;
}

AST_MATCH_CASE()
{
    ++tab;

    if (!arg_match_case->is_wildcard)
    {
        printf_tab("Case patterns :\n");
        for (int i = 0; i < arg_match_case->patterns.size; ++i)
            print_match_pattern(&arg_match_case->patterns.ptr[i]);
    }
    else
    {
        printf_tab("Pattern wildcard\n");
    }

    printf_tab("Expr :\n");
    print_expression(arg_match_case->expr);

    --tab;
}

AST_MATCH_EXPR()
{
    ++tab;

    printf_tab("Match expression :\n");
    print_expression(arg_match_expr->tested_expr);

    for (int i = 0; i < arg_match_expr->cases.size; ++i)
        print_match_case(&arg_match_expr->cases.ptr[i]);
    --tab;
}

AST_RAND_EXPR()
{
    ++tab;

    printf_tab("Random expression of type %s\n", arg_random_expr->type == RAND_ARR ? "array" :
                                                 arg_random_expr->type == RAND_FLT ? "real"  :
                                                 arg_random_expr->type == RAND_RNG ? "range" : "int");
    AST_RAND_EXPR_PROCESS();

    --tab;
}

AST_ARRAY_LIT_EXPR()
{
    ++tab;

    printf_tab("Array literal:\n");
    AST_ARRAY_LIT_EXPR_PROCESS();

    --tab;
}

AST_ARRAY_RANGE_EXPR()
{
    ++tab;

    printf_tab("Array range generator:\n");
    AST_ARRAY_RANGE_EXPR_PROCESS_1();
    printf_tab("..");
    AST_ARRAY_RANGE_EXPR_PROCESS_2();

    --tab;
}

AST_NULL_EXPRESSION()
{
    ++tab;

    printf_tab("Null\n");

    --tab;
}

AST_TERNARY_EXPRESSION()
{
    ++tab;

    printf_tab("Ternary expr :\n");
    AST_TERNARY_EXPRESSION_PROCESS_1();
    printf_tab("Ternary True :\n");
    AST_TERNARY_EXPRESSION_PROCESS_2();
    printf_tab("Ternary False :\n");
    AST_TERNARY_EXPRESSION_PROCESS_3();

    --tab;
}

AST_PRIM_EXPRESSION()
{
    ++tab;

    AST_PRIM_EXPRESSION_PROCESS();

    --tab;
}

AST_EXPRESSION()
{
    ++tab;

    printf_tab("Expr of type %s\n", type_to_str(&arg_expression->value_type));
    AST_EXPRESSION_PROCESS();

    --tab;
}

