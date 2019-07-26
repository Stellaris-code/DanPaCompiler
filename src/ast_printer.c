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

#include "ast_printer.h"
#include "lexer.h"

static int tab = 0;

void printf_tab(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    printf("%*s", 2*tab, ""); // print tab spaces
    vprintf(fmt, args);

    va_end(args);
}

void print_program(program_t *prgm)
{
    printf_tab("%*sProgram :\n", tab, "");
    for (int i = 0; i < prgm->function_list.size; ++i)
    {
        print_function(&prgm->function_list.ptr[i]);
    }
}



void print_function(function_t *func)
{
    ++tab;

    printf_tab("Function '%s %s()' :\n", types_str[func->type], func->name);
    for (int i = 0; i < func->statement_list.size; ++i)
    {
        print_statement(&func->statement_list.ptr[i]);
    }

    --tab;
}

void print_statement(statement_t *statement)
{
    ++tab;

    if (statement->type == RETURN_STATEMENT)
    {
        print_return_statement(&statement->return_statement);
    }
    else if (statement->type == IF_STATEMENT)
    {
        print_if_statement(&statement->if_statement);
    }
    else if (statement->type == ASSIGNMENT)
    {
        print_assignment(&statement->assignment);
    }
    else if (statement->type == COMPOUND_STATEMENT)
    {
        for (int i = 0; i < statement->statement_list.size; ++i)
        {
            print_statement(&statement->statement_list.ptr[i]);
        }
    }

    --tab;
}

void print_return_statement(return_statement_t *statement)
{
    ++tab;

    printf_tab("Return statement :\n");
    print_expression(&statement->expr);

    --tab;
}

void print_assignment(assignment_t *assignment)
{
    ++tab;

    printf_tab("%s =\n", assignment->var);
    print_expression(&assignment->expr);

    --tab;
}

void print_if_statement(if_statement_t *if_statement)
{
    ++tab;

    printf_tab("If :\n");
        print_expression(&if_statement->first_pair.test);
    printf_tab("Then :\n");
        print_statement(if_statement->first_pair.statement);

    if (if_statement->else_statement)
    {
        printf_tab("Else:\n");
            print_statement(if_statement->else_statement);
    }

    --tab;
}

void print_prim_expr(primary_expression_t* value);

void print_expression(expression_t *expr)
{
    //++tab;

    //printf_tab("Expr\n");
    printf_tab("Expr of type %s\n", types_str[get_expression_type(expr)]);
    if (expr->type == PRIM_EXPR)
    {
        print_prim_expr(&expr->prim_expr);
    }
    else if (expr->type == BINOP)
    {
        print_binop(expr->binop);
    }

    //--tab;
}

void print_prim_expr(primary_expression_t* value)
{
    ++tab;

    if (value->type == ENCLOSED)
    {
        print_expression(value->expr);
    }
    else if (value->type == UNARY_OP_FACTOR)
    {
        printf_tab("Unary value %s\n", operators_str[value->unary_op]);
        print_prim_expr(value->unary_value);
    }
    else if (value->type == IDENT)
    {
        printf_tab("Identifier %s\n", value->ident);
    }
    else if (value->type == INT_CONSTANT)
    {
        printf_tab("Integer %d\n", value->int_constant);
    }
    else if (value->type == FLOAT_CONSTANT)
    {
        printf_tab("Float %f\n", value->flt_constant);
    }
    else if (value->type == STRING_LITERAL)
    {
        printf_tab("String \"%s\"\n", value->string_lit);
    }

    --tab;
}

void print_binop(binop_t *binop)
{
    ++tab;

    printf_tab("Binop :\n");
    print_expression(&binop->left);
    printf_tab("'%s' \n", operators_str[binop->op]);
    print_expression(&binop->right);

    --tab;
}
