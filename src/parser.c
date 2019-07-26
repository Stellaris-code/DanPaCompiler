/*
parser.c

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

#include "parser.h"
#include "lexer.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static token_t* tokens;

static int precedence[OP_ENUM_END] =
{
    /* ADD : */ 30,
    /* SUB : */ 30,
    /* MUL : */ 40,
    /* DIV : */ 40,
    /* EQ  : */ 20,
    /* GT  : */ 10,
    /* GE  : */ 10,
    /* LT  : */ 10,
    /* LE  : */ 10,
};

static inline int token_is_type(token_t* ident)
{
    if (ident->type != TOK_IDENTIFIER)
        return 0;

    for (int i = 0; i < TYPES_END; ++i)
    {
        if (strcmp(ident->data.str, types_str[i]) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static inline type_t token_type_or_fail(token_t* ident)
{
    assert(ident->type == TOK_IDENTIFIER);

    for (int i = 0; i < TYPES_END; ++i)
    {
        if (strcmp(ident->data.str, types_str[i]) == 0)
        {
            return (type_t)i;
        }
    }

    error(ident->row, ident->col, ident->filename, "Invalid type '%s'\n", ident->data.str);
}

static inline type_t token_variable_type_or_fail(token_t* ident)
{
    type_t type = token_type_or_fail(ident);

    if (type == VOID)
        error(ident->row, ident->col, ident->filename, "a variable cannot have the type 'void'\n", ident->data.str);

    return type;
}

type_t get_expression_type(const expression_t* expr);

type_t get_prim_expr_type(const primary_expression_t* prim_expr)
{
    switch (prim_expr->type)
    {
        case ENCLOSED:
            return get_expression_type(prim_expr->expr);
        case UNARY_OP_FACTOR:
            return get_prim_expr_type(prim_expr->unary_value);
        case IDENT:
            return VOID;
        case INT_CONSTANT:
            return INT;
        case FLOAT_CONSTANT:
            return REAL;
        case TOK_STRING_LITERAL:
            return STR;
        default:
            assert(0);
    }
}

type_t get_binop_type(const binop_t* binop)
{
    type_t lhs_type = get_expression_type(&binop->left);
    type_t rhs_type = get_expression_type(&binop->left);

    return lhs_type;
}

type_t get_expression_type(const expression_t* expr)
{
    if (expr->type == PRIM_EXPR)
        return get_prim_expr_type(&expr->prim_expr);
    if (expr->type == BINOP)
        return get_binop_type(expr->binop);
}

int is_binop(int op)
{
    return op < OP_INC;
}

void set_parser_token_list(token_t *token_list)
{
    tokens = token_list;
}

token_t* next_token()
{
    return tokens;
}

token_t* consume_token()
{
    return tokens++;
}

token_t* accept(token_type_t type)
{
    token_t* cur_tok = next_token();
    if (cur_tok->type != type)
    {
        return NULL;
    }

    consume_token();

    return cur_tok;
}

token_t* expect(token_type_t type)
{
    token_t* cur_tok = next_token();
    if (cur_tok->type != type)
    {
        error(cur_tok->row, cur_tok->col, cur_tok->filename, "expected '%s', got '%s'\n", tokens_str[type], tokens_str[cur_tok->type]);
    }

    consume_token();

    return cur_tok;
}

void parse_expr(expression_t* expr, int p);

void parse_prim_expr(primary_expression_t* value)
{
    token_t* tok;
    if (accept(TOK_OPEN_PARENTHESIS))
    {
        expression_t expr;
        parse_expr(&expr, 0);
        expect(TOK_CLOSE_PARENTHESIS);

        value->type = ENCLOSED;
        expression_t* factor_expr = (expression_t*)malloc(sizeof(expression_t));
        *factor_expr = expr;
        value->expr = factor_expr;
    }
    else if (next_token()->type == TOK_OPERATOR &&
             (next_token()->data.op == '+' || next_token()->data.op == '-'))
    {
        token_t* op = consume_token();

        primary_expression_t* unary_op_factor = (primary_expression_t*)malloc(sizeof(primary_expression_t));
        parse_prim_expr(unary_op_factor);

        value->type = UNARY_OP_FACTOR;
        value->unary_op = op->data.op;
        value->unary_value = unary_op_factor;
    }
    else if ((tok = accept(TOK_IDENTIFIER)))
    {
        value->type = IDENT;
        value->ident = tok->data.str;
    }
    else if ((tok = accept(TOK_INTEGER_LITERAL)))
    {
        value->type = INT_CONSTANT;
        value->int_constant = tok->data.integer;
    }
    else if ((tok = accept(TOK_FLOAT_LITERAL)))
    {
        value->type = FLOAT_CONSTANT;
        value->flt_constant = tok->data.fp;
    }
    else if ((tok = accept(TOK_STRING_LITERAL)))
    {
        value->type = STRING_LITERAL;
        value->string_lit = tok->data.str;
    }
    else
    {
        tok = next_token();
        error(tok->row, tok->col, tok->filename, "expected expression got '%s'\n", tokens_str[tok->type]);
    }
}

void parse_return_statement(return_statement_t* ret_statement)
{
    expression_t expr;

    expect(KEYWORD_RETURN);
    parse_expr(&expr, 0);

    ret_statement->expr = expr;

    expect(TOK_SEMICOLON);
}

void parse_statement(statement_t* statement);

void parse_if_statement(if_statement_t* if_statement)
{
    if_statement->first_pair.statement = (statement_t*)malloc(sizeof(statement_t));

    expect(KEYWORD_IF);
    expect(TOK_OPEN_PARENTHESIS);
    parse_expr(&if_statement->first_pair.test, 0);
    expect(TOK_CLOSE_PARENTHESIS);
    parse_statement(if_statement->first_pair.statement);

    if (accept(KEYWORD_ELSE))
    {
        statement_t* stat = (statement_t*)(malloc(sizeof(statement_t)));

        parse_statement(stat);

        if_statement->else_statement = stat;
    }
    else
    {
        if_statement->else_statement = NULL;
    }
}

void parse_assignment(assignment_t* assignment)
{
    expression_t expr;
    token_t* ident = expect(TOK_IDENTIFIER);
    expect(TOK_ASSIGNMENT_OP);
    parse_expr(&expr, 0);

    assignment->var = ident->data.str;
    assignment->expr = expr;

    expect(TOK_SEMICOLON);
}

void parse_declaration(declaration_statement_t* decl)
{
    decl->type = token_variable_type_or_fail(consume_token());
    decl->name = expect(TOK_IDENTIFIER)->data.str;
    if (accept(TOK_ASSIGNMENT_OP))
    {
        expression_t* expr = (expression_t*)malloc(sizeof(expression_t));
        parse_expr(expr, 0);

        decl->init_value = expr;
    }
    expect(TOK_SEMICOLON);
}

void parse_statement(statement_t* statement)
{
    if (token_is_type(next_token()))
    {

        declaration_statement_t decl;
        parse_declaration(&decl);
        statement->type = DECLARATION;
        statement->declaration = decl;
    }
    else if (next_token()->type == KEYWORD_RETURN)
    {
        return_statement_t ret;
        parse_return_statement(&ret);
        statement->type = RETURN_STATEMENT;
        statement->return_statement = ret;
    }
    else if (next_token()->type == KEYWORD_IF)
    {
        if_statement_t if_statement;
        parse_if_statement(&if_statement);
        statement->type = IF_STATEMENT;
        statement->if_statement = if_statement;
    }
    else if (next_token()->type == TOK_OPEN_BRACE)
    {
        consume_token();

        DYNARRAY_INIT(statement->statement_list, 8);

        while (!accept(TOK_CLOSE_BRACE))
        {
            statement_t st;
            parse_statement(&st);
            DYNARRAY_ADD(statement->statement_list, st);
        }

        statement->type = COMPOUND_STATEMENT;
    }
    else
    {
        assignment_t assignment;
        parse_assignment(&assignment);
        statement->type = ASSIGNMENT;
        statement->assignment = assignment;
    }
}

void parse_function(function_t* func)
{
    DYNARRAY_INIT(func->statement_list, 256);

    token_t* type = expect(TOK_IDENTIFIER);
    token_t* name = expect(TOK_IDENTIFIER);
    expect(TOK_OPEN_PARENTHESIS);
    expect(TOK_CLOSE_PARENTHESIS);
    expect(TOK_OPEN_BRACE);

    func->type = token_type_or_fail(type);
    func->name = name->data.str;

    while (next_token()->type != TOK_CLOSE_BRACE)
    {
        statement_t statement;
        parse_statement(&statement);
        DYNARRAY_ADD(func->statement_list, statement);
    }

    expect(TOK_CLOSE_BRACE);
}

void parse_program(program_t* program)
{
    DYNARRAY_INIT(program->function_list, 32);

    while (next_token()->type != TOKEN_EOF)
    {
        function_t func;
        parse_function(&func);
        DYNARRAY_ADD(program->function_list, func);
    }
}

void parse_expr(expression_t* expr, int p)
{
    expression_t* lhs = (expression_t*)malloc(sizeof(expression_t));

    primary_expression_t val;
    parse_prim_expr(&val);
    lhs->type = PRIM_EXPR;
    lhs->prim_expr = val;

    token_t* op = next_token();
    while (op->type == TOK_OPERATOR && is_binop(op->data.op)
           && precedence[op->data.op] >= p)
    {
        consume_token();

        expression_t rhs;
        parse_expr(&rhs, precedence[op->data.op] + 1);

        expression_t* new_node = (expression_t*)malloc(sizeof(expression_t));

        new_node->type = BINOP;
        new_node->binop = (binop_t*)malloc(sizeof(binop_t));
        new_node->binop->left = *lhs;
        new_node->binop->right = rhs;
        new_node->binop->op = op->data.op;

        lhs = new_node;

        op = next_token();
    }

    *expr = *lhs;
}
