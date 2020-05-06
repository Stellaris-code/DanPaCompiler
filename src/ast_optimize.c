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

#define AST_PASS_NAME ast_optimize
#include "ast_functions.h"

#include <assert.h>
#include <math.h>

        static inline int int_log2(uint32_t x)
{
#if defined(__clang__) || defined(__GNUC__)
    return __builtin_ctz(x);
#else
    int n = 1;
    if ((x & 0x0000FFFF) == 0) {n = n +16; x = x >>16;}
    if ((x & 0x000000FF) == 0) {n = n + 8; x = x >> 8;}
    if ((x & 0x0000000F) == 0) {n = n + 4; x = x >> 4;}
    if ((x & 0x00000003) == 0) {n = n + 2; x = x >> 2;}
    return n - (x & 1);
#endif
}

static void peephole_modulo(binop_t* binop)
{
    assert(binop->op->data.op == OP_MOD);

    if (binop->right.kind == PRIM_EXPR && binop->right.prim_expr.type == INT_CONSTANT)
    {
        uint32_t arg = binop->right.prim_expr.int_constant->data.integer;
        if (((arg & (arg-1)) == 0) && arg != 0) // power of two
        {
            // change it into a AND
            binop->op->data.op = OP_BITAND;
            binop->right.prim_expr.int_constant->data.integer = arg-1; // mask
        }
    }
}

static void peephole_div_shift(binop_t* binop)
{
    assert(binop->op->data.op == OP_DIV);

    type_t int_type = mk_type(INT);
    if (cmp_types(&int_type, &binop->left.value_type) && binop->right.kind == PRIM_EXPR && binop->right.prim_expr.type == INT_CONSTANT)
    {
        uint32_t arg = binop->right.prim_expr.int_constant->data.integer;
        if (((arg & (arg-1)) == 0) && arg != 0) // power of two
        {
            // change it into a shift
            binop->op->data.op = OP_SHR;
            binop->right.prim_expr.int_constant->data.integer = int_log2(arg);
        }
    }
}

static void peephole_mul_shift(binop_t* binop)
{
    assert(binop->op->data.op == OP_MUL);

    type_t int_type = mk_type(INT);
    if (!cmp_types(&int_type, &binop->left.value_type) || !cmp_types(&int_type, &binop->right.value_type))
        return;

    if (binop->right.kind == PRIM_EXPR && binop->right.prim_expr.type == INT_CONSTANT)
    {
        uint32_t arg = binop->right.prim_expr.int_constant->data.integer;
        if (((arg & (arg-1)) == 0) && arg != 0) // power of two
        {
            // change it into a shift
            binop->op->data.op = OP_SHL;
            binop->right.prim_expr.int_constant->data.integer = int_log2(arg);
        }
    }
    else if (binop->left.kind == PRIM_EXPR && binop->left.prim_expr.type == INT_CONSTANT)
    {
        uint32_t arg = binop->left.prim_expr.int_constant->data.integer;
        if (((arg & (arg-1)) == 0) && arg != 0) // power of two
        {
            // change it into a shift
            binop->op->data.op = OP_SHL;
            // swap the operands so the shift factor is to the right
            expression_t tmp = binop->left;
            binop->left = binop->right;
            binop->right = tmp;

            binop->right.prim_expr.int_constant->data.integer = int_log2(arg);
        }
    }
}

static void peephole_constant_cast(primary_expression_t* prim_expr)
{
    type_t flt_type = mk_type(REAL);
    type_t int_type = mk_type(INT);

    assert(prim_expr->type == CAST_EXPRESSION);

    if (prim_expr->cast_expr.expr->type == INT_CONSTANT)
    {
        if (cmp_types(&prim_expr->cast_expr.target_type, &flt_type))
        {
            float val = (float)prim_expr->cast_expr.expr->int_constant->data.integer;
            prim_expr->type = FLOAT_CONSTANT;
            prim_expr->flt_constant = (token_t*)danpa_alloc(sizeof(token_t));
            prim_expr->flt_constant->data.fp = val;
        }
    }
    else if (prim_expr->cast_expr.expr->type == FLOAT_CONSTANT)
    {
        if (cmp_types(&prim_expr->cast_expr.target_type, &int_type))
        {
            int32_t val = (int32_t)prim_expr->cast_expr.expr->int_constant->data.fp;
            prim_expr->type = INT_CONSTANT;
            prim_expr->int_constant = (token_t*)danpa_alloc(sizeof(token_t));
            prim_expr->int_constant->data.integer = val;
        }
    }
}

static void peephole_integer_constant_eval_binop(expression_t* expr)
{
    if (expr->kind != BINOP)
        return;
    binop_t* binop = expr->binop;

    if ((binop->left.kind != PRIM_EXPR || binop->left.prim_expr.type != INT_CONSTANT) ||
        (binop->right.kind != PRIM_EXPR || binop->right.prim_expr.type != INT_CONSTANT))
        return;

    int ileft = binop->left.prim_expr.int_constant->data.integer;
    int iright = binop->right.prim_expr.int_constant->data.integer;

    int iprocess = 0;
    int ival;
    iprocess = 1;
    ival = eval_int_binop(binop->op->data.op, ileft, iright);

    if (iprocess)
    {
        expr->kind = PRIM_EXPR;
        expr->prim_expr.type = INT_CONSTANT;
        expr->prim_expr.int_constant = (token_t*)danpa_alloc(sizeof(token_t));
        expr->prim_expr.int_constant->data.integer = ival;
    }
}

static void peephole_float_constant_eval_binop(expression_t* expr)
{
    if (expr->kind != BINOP)
        return;
    binop_t* binop = expr->binop;

    if ((binop->left.kind != PRIM_EXPR || binop->left.prim_expr.type != FLOAT_CONSTANT) ||
        (binop->right.kind != PRIM_EXPR || binop->right.prim_expr.type != FLOAT_CONSTANT))
        return;

    float fleft = binop->left.prim_expr.flt_constant->data.fp;
    float fright = binop->right.prim_expr.flt_constant->data.fp;

    int fprocess = 0;
    float fval;
    switch (binop->op->data.op)
    {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_EQUAL:
        case OP_DIFF:
        case OP_GT:
        case OP_GE:
        case OP_LT:
        case OP_LE:
            fprocess = 1;
            fval = eval_float_binop(binop->op->data.op, fleft, fright);
        default:
            break;
    }

    if (fprocess)
    {
        expr->kind = PRIM_EXPR;
        expr->prim_expr.type = FLOAT_CONSTANT;
        expr->prim_expr.flt_constant = (token_t*)danpa_alloc(sizeof(token_t));
        expr->prim_expr.flt_constant->data.fp = fval;
    }
}

static void peephole_integer_constant_eval_unary(primary_expression_t* expr)
{
    if (expr->type != UNARY_OP_FACTOR)
        return;
    unary_expr_t* unary = &expr->unary_expr;
    if (unary->unary_op->type != TOK_OPERATOR)
        return;

    // only int supported right now
    if (unary->unary_value->type != INT_CONSTANT)
        return;

    int cst = unary->unary_value->int_constant->data.integer;

    int ival;
    ival = eval_int_unop(unary->unary_op->data.op, cst);

    expr->type = INT_CONSTANT;
    expr->int_constant = (token_t*)danpa_alloc(sizeof(token_t));
    expr->int_constant->data.integer = ival;
}

static void peephole_float_constant_eval_unary(primary_expression_t* expr)
{
    if (expr->type != UNARY_OP_FACTOR)
        return;
    unary_expr_t* unary = &expr->unary_expr;

    // only int supported right now
    if (unary->unary_value->type != INT_CONSTANT)
        return;

    float cst = unary->unary_value->int_constant->data.integer;

    float fval;
    fval = eval_float_unop(unary->unary_op->data.op, cst);

    expr->type = FLOAT_CONSTANT;
    expr->flt_constant = (token_t*)danpa_alloc(sizeof(token_t));
    expr->flt_constant->data.fp = fval;
}

static void ast_optimize_ident(ident_t* ident)
{
}
static void ast_optimize_int_constant(token_t* val)
{
}
static void ast_optimize_float_constant(token_t* val)
{
}
static void ast_optimize_string_literal(token_t* name)
{
}

AST_PROGRAM()
{
    AST_PROGRAM_PROCESS_1();
    AST_PROGRAM_PROCESS_2();
}

AST_FUNCTION()
{
    AST_FUNCTION_PROCESS();
}

AST_TYPE()
{
    AST_TYPE_PROCESS();
}

AST_RETURN_STATEMENT()
{
    AST_RETURN_STATEMENT_PROCESS();
}

AST_ASSIGNMENT()
{
    AST_ASSIGNMENT_PROCESS_1();
    AST_ASSIGNMENT_PROCESS_2();
}

AST_IF_STATEMENT()
{
    AST_IF_STATEMENT_PROCESS_1();
    AST_IF_STATEMENT_PROCESS_2();
    AST_IF_STATEMENT_PROCESS_3();
}

AST_WHILE_STATEMENT()
{
    AST_WHILE_STATEMENT_PROCESS_1();
    AST_WHILE_STATEMENT_PROCESS_2();
}

AST_FOR_STATEMENT()
{
    AST_FOR_STATEMENT_PROCESS_INIT();
    AST_FOR_STATEMENT_PROCESS_TEST();
    AST_FOR_STATEMENT_PROCESS_BODY();
    AST_FOR_STATEMENT_PROCESS_LOOP();
}

AST_FOREACH_STATEMENT()
{
    AST_FOREACH_STATEMENT_PROCESS_IDENT();
    AST_FOREACH_STATEMENT_PROCESS_ARRAY();
    AST_FOREACH_STATEMENT_PROCESS_BODY();
}

AST_DO_WHILE_STATEMENT()
{
    AST_DO_WHILE_STATEMENT_PROCESS_1();
    AST_DO_WHILE_STATEMENT_PROCESS_2();
}

AST_LOOP_CTRL_STATEMENT()
{
    AST_LOOP_CTRL_STATEMENT();
}

AST_COMPOUND_STATEMENT()
{
    AST_COMPOUND_STATEMENT_PROCESS();
}

AST_ASM_EXPR()
{
    AST_ASM_EXPR_PROCESS();
}

AST_RAND_EXPR()
{
    AST_RAND_EXPR_PROCESS();
}

AST_ARRAY_LIT_EXPR()
{
    AST_ARRAY_LIT_EXPR_PROCESS();
}

AST_STATEMENT()
{
    AST_STATEMENT_PROCESS();
}

AST_TYPEDEF_DECLARATION()
{
    AST_TYPEDEF_DECLARATION_PROCESS();
}

AST_VARIABLE_DECLARATION()
{
    AST_VARIABLE_DECLARATION_PROCESS();
}

AST_STRUCT_DECLARATION()
{
    AST_STRUCT_DECLARATION_PROCESS();
}

AST_DECLARATION()
{
    AST_DECLARATION_PROCESS();
}

AST_BINOP()
{
    AST_BINOP_PROCESS_1();
    AST_BINOP_PROCESS_2();

    if (arg_binop->op->data.op == OP_MOD)
    {
        peephole_modulo(arg_binop);
    }
    if (arg_binop->op->data.op == OP_DIV)
    {
        peephole_div_shift(arg_binop);
    }
    if (arg_binop->op->data.op == OP_MUL)
    {
        peephole_mul_shift(arg_binop);
    }
}

AST_FUNC_CALL_EXPRESSION()
{
    AST_FUNC_CALL_EXPRESSION_PROCESS_1();
    AST_FUNC_CALL_EXPRESSION_PROCESS_2();
}

AST_ARRAY_SUBSCRIPT()
{
    AST_ARRAY_SUBSCRIPT_PROCESS_1();
}

AST_ARRAY_SLICE()
{
    AST_ARRAY_SLICE_PROCESS_1();
    AST_ARRAY_SLICE_PROCESS_2();
    AST_ARRAY_SLICE_PROCESS_3();
}

AST_ARRAY_RANGE_EXPR()
{
    AST_ARRAY_RANGE_EXPR_PROCESS_1();
    AST_ARRAY_RANGE_EXPR_PROCESS_2();
}

AST_STRUCT_ACCESS()
{
    AST_STRUCT_ACCESS_PROCESS();
}

AST_STRUCT_INIT_EXPR()
{
    AST_STRUCT_INIT_EXPR_PROCESS();
}

AST_DEREF_EXPR()
{
    AST_DEREF_EXPR_PROCESS();
}

AST_ADDR_EXPR()
{
    AST_ADDR_EXPR_PROCESS();
}


AST_MATCH_PATTERN()
{

}

AST_MATCH_CASE()
{

}

AST_MATCH_EXPR()
{

}

AST_NEW_EXPR()
{
    AST_NEW_EXPR_PROCESS();
}

AST_NULL_EXPRESSION()
{

}

AST_SIZEOF_EXPR()
{
    AST_SIZEOF_EXPR_PROCESS();
}

AST_UNARY_EXPRESSION()
{
    AST_UNARY_EXPRESSION_PROCESS();
}

AST_CAST_EXPRESSION()
{
    AST_CAST_EXPRESSION_PROCESS();
}

AST_TERNARY_EXPRESSION()
{
    AST_TERNARY_EXPRESSION_PROCESS_1();
    AST_TERNARY_EXPRESSION_PROCESS_2();
    AST_TERNARY_EXPRESSION_PROCESS_3();
}

AST_PRIM_EXPRESSION()
{
    AST_PRIM_EXPRESSION_PROCESS();

    // shorten the expression chain
    while (arg_primary_expression->type == ENCLOSED && arg_primary_expression->expr->kind == PRIM_EXPR)
        *arg_primary_expression = arg_primary_expression->expr->prim_expr;

    if (arg_primary_expression->type == CAST_EXPRESSION)
        peephole_constant_cast(arg_primary_expression);

    peephole_integer_constant_eval_unary(arg_primary_expression);
    peephole_float_constant_eval_unary(arg_primary_expression);
}

AST_EXPRESSION()
{
    AST_EXPRESSION_PROCESS();
    peephole_integer_constant_eval_binop(arg_expression);
    peephole_float_constant_eval_binop(arg_expression);
}
