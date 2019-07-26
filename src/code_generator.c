/*
code_generator.c

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

#include "code_generator.h"
#include "lexer.h"

#include <stdio.h>
#include <string.h>

#define LABEL_MAX_LEN 16

static int label_counter = 0;
static inline void generate_label(char* buf)
{
    snprintf(buf, LABEL_MAX_LEN, "L%x", label_counter++);
}

void generate_function(function_t *func);

void generate_program(program_t *prgm)
{
    for (int i = 0; i < prgm->function_list.size; ++i)
    {
        generate_function(&prgm->function_list.ptr[i]);
    }
}

void generate_expression(expression_t *expr);
void generate_statement(statement_t *statement);

void generate_function(function_t *func)
{
    for (int i = 0; i < func->statement_list.size; ++i)
    {
        generate_statement(&func->statement_list.ptr[i]);
    }
}

void generate_return_statement(return_statement_t *statement)
{
    generate_expression(&statement->expr);
    printf("ret\n");
}

void generate_assignment(assignment_t *assignment)
{
    generate_expression(&assignment->expr);
    printf("mov %s\n", assignment->var);
}

void generate_if_statement(if_statement_t *if_statement)
{
    char else_label[LABEL_MAX_LEN];
    char out_label [LABEL_MAX_LEN];
    generate_label(else_label);
    generate_label(out_label );

    generate_expression(&if_statement->first_pair.test);
    printf("jf .%s\n", else_label);

    generate_statement(if_statement->first_pair.statement);
    printf("jmp .%s\n", out_label);

    printf(".%s:\n", else_label);
    if (if_statement->else_statement)
    {
        generate_statement(if_statement->else_statement);
    }

    printf(".%s:\n", out_label);
}

void generate_statement(statement_t *statement)
{
    if (statement->type == RETURN_STATEMENT)
    {
        generate_return_statement(&statement->return_statement);
    }
    else if (statement->type == IF_STATEMENT)
    {
        generate_if_statement(&statement->if_statement);
    }
    else if (statement->type == ASSIGNMENT)
    {
        generate_assignment(&statement->assignment);
    }
    else if (statement->type == COMPOUND_STATEMENT)
    {
        for (int i = 0; i < statement->statement_list.size; ++i)
        {
            generate_statement(&statement->statement_list.ptr[i]);
        }
    }
}

void generate_prim_expr(primary_expression_t* value);
void generate_binop(binop_t *binop);

void generate_expression(expression_t *expr)
{
    //++tab;


    if (expr->type == PRIM_EXPR)
    {
        generate_prim_expr(&expr->prim_expr);
    }
    else if (expr->type == BINOP)
    {
        generate_binop(expr->binop);
    }

    //--tab;
}

void generate_prim_expr(primary_expression_t* value)
{
    if (value->type == ENCLOSED)
    {
        generate_expression(value->expr);
    }
    else if (value->type == UNARY_OP_FACTOR)
    {
        generate_prim_expr(value->unary_value);
    }
    else if (value->type == IDENT)
    {
        printf("push %s\n", value->ident);
    }
    else if (value->type == INT_CONSTANT)
    {
        printf("pushi %d\n", value->int_constant);
    }
    else if (value->type == FLOAT_CONSTANT)
    {
        printf("pushf %f\n", value->flt_constant);
    }
    else if (value->type == STRING_LITERAL)
    {
        printf("pushs \"%s\"\n", value->string_lit);
    }

}

void generate_binop(binop_t *binop)
{
    generate_expression(&binop->left);
    generate_expression(&binop->right);

    printf("op '%s'\n", operators_str[binop->op]);
}

