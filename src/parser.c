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
#include "alloc.h"
#include "builtin.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

        static token_t* tokens;
static token_t* prev_token_val = NULL;
static program_t*  current_program  = NULL;

#define REWIND_BEGIN(scope_name) \
    int scope_name##_rewind_status = 0;\
    DYNARRAY_ADD(rewind_stack, (rewind_data_t){}); \
    rewind_stack.ptr[rewind_stack.size-1].tokens = tokens; \
    rewind_stack.ptr[rewind_stack.size-1].prev_token = prev_token_val; \
    if (setjmp(rewind_stack.ptr[rewind_stack.size-1].jmp_restore) != 0) \
    { \
        scope_name##_rewind_status = 1; \
        goto scope_name##_rewind_out; \
    } \

#define REWIND_END(scope_name) \
    scope_name##_rewind_out: \
    assert(rewind_stack.size > 0); \
    tokens = rewind_stack.ptr[rewind_stack.size-1].tokens; \
    prev_token_val = rewind_stack.ptr[rewind_stack.size-1].prev_token; \
    DYNARRAY_POP(rewind_stack);

#define REWIND_FAILED(scope_name) scope_name##_rewind_status

typedef struct rewind_data_t
{
    token_t* tokens;
    token_t* prev_token;
    jmp_buf  jmp_restore;
} rewind_data_t;

static DYNARRAY(rewind_data_t) rewind_stack;

int is_binop(int op)
{
    return operators[op].category == OPC_BINARY;
}

void set_parser_token_list(token_t *token_list)
{
    tokens = token_list;
}

token_t* next_token()
{
    return tokens;
}

token_t* prev_token()
{
    return prev_token_val;
}

token_t* forward(int n)
{
    for (int i = 0; i < n; ++i)
    {
        if (tokens[i].type == TOKEN_EOF)
            return &tokens[i];
    }

    return &tokens[n];
}

token_t* consume_token()
{
    prev_token_val = tokens;
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

token_t* accept_op(operator_type_t op)
{
    token_t* cur_tok = next_token();
    if (cur_tok->type != TOK_OPERATOR || cur_tok->data.op != op)
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
        if (rewind_stack.size) // rewind
            longjmp(rewind_stack.ptr[rewind_stack.size-1].jmp_restore, 1);
        else
            error(cur_tok->location, cur_tok->length, "expected '%s', got '%s'\n", tokens_str[type], tokens_str[cur_tok->type]);
    }

    consume_token();

    return cur_tok;
}

token_t* expect_op(operator_type_t op)
{
    token_t* cur_tok = next_token();
    if (cur_tok->type != TOK_OPERATOR || cur_tok->data.op != op)
    {
        if (rewind_stack.size) // rewind
            longjmp(rewind_stack.ptr[rewind_stack.size-1].jmp_restore, 1);
        else
            error(cur_tok->location, cur_tok->length, "expected '%s', got '%s'\n", operators[op].str, tokens_str[cur_tok->type]);
    }

    consume_token();

    return cur_tok;
}


static inline int token_is_type(token_t* ident)
{
    if (ident->type != TOK_IDENTIFIER)
        return 0;

    return get_type(ident->data.str).base_type != INVALID_TYPE;
}

static int has_function(const char* str)
{
    if (find_builtin(str))
        return 1;

    for (int i = 0; i < current_program->function_list.size; ++i)
    {
        function_t* item = &current_program->function_list.ptr[i];
        if (strcmp(item->name->data.str, str) == 0)
        {
            return 1;
        }
    }
    return 0;
}

void parse_expr(expression_t* expr, int p);

void parse_type(type_t* type)
{
    token_t* base_type_tok = expect(TOK_IDENTIFIER);
    *type = get_type(base_type_tok->data.str);  // base type
    type->token = base_type_tok;

    if (type->base_type == INVALID_TYPE)
        error(base_type_tok->location, strlen(base_type_tok->data.str), "Invalid type '%s'\n", base_type_tok->data.str);

    // the fun stuff : interpret pointer and array declarators
    for(;;)
    {
        token_t* tok;
        // is a pointer
        if ((tok = accept_op(OP_MUL))) // '*'
        {
            type_t* base_type = (type_t*)danpa_alloc(sizeof(type_t));
            *base_type = *type;
            type->kind = POINTER;
            type->pointer.pointed_type = base_type;
            type->token = tok;
        }
        // is an optional
        else if ((tok = accept(TOK_QUESTION))) // '?'
        {
            type_t* base_type = (type_t*)danpa_alloc(sizeof(type_t));
            *base_type = *type;
            type->kind = OPTIONAL;
            type->opt.opt_type = base_type;
            type->token = tok;
        }
        // is an array
        else if ((tok = accept(TOK_OPEN_BRACKET)))
        {
            type_t* base_type = (type_t*)danpa_alloc(sizeof(type_t));
            *base_type = *type;
            type->kind = ARRAY;
            type->token = tok;
            type->array.array_type = base_type;
            type->array.initial_size = NULL;
            if (next_token()->type != TOK_CLOSE_BRACKET)
            {
                expression_t* expr = (expression_t*)danpa_alloc(sizeof(expression_t));
                parse_expr(expr, 0);
                type->array.initial_size = expr;
                type->array.is_empty = 0;
            }
            else // no immediate size : simply set the initial size to zero
            {
                expression_t* expr = (expression_t*)danpa_alloc(sizeof(expression_t));
                expr->value_type = mk_type(INT);
                expr->kind = PRIM_EXPR;
                expr->prim_expr.type = INT_CONSTANT;
                expr->prim_expr.int_constant = danpa_alloc(sizeof(token_t));
                expr->prim_expr.int_constant->data.integer = 0;

                type->array.initial_size = expr;
                type->array.is_empty = 1;
            }
            expect(TOK_CLOSE_BRACKET);
        }
        // end of declarators
        else
            break;
    }

    // is it a function ?
    if (accept(TOK_OPEN_PARENTHESIS))
    {
        type_t* ret_type = (type_t*)danpa_alloc(sizeof(type_t));
        *ret_type = *type;

        function_signature_t* sig = danpa_alloc(sizeof(function_signature_t));

        sig->ret_type = *ret_type;

        DYNARRAY_INIT(sig->parameter_types, 4);
        do
        {
            type_t param;
            parse_type(&param);

            DYNARRAY_ADD(sig->parameter_types, param);

            // another parameter
            if (next_token()->type != TOK_CLOSE_PARENTHESIS)
                expect(TOK_COMMA);
            else
            {
                expect(TOK_CLOSE_PARENTHESIS);
                break;
            }
        } while (1);

        type->kind = FUNCTION;
        type->function.signature = sig;
    }
}

int maybe_parse_type()
{
    REWIND_BEGIN(maybe_type);

    type_t type;
    parse_type(&type);

    REWIND_END(maybe_type);

    return REWIND_FAILED(maybe_type) == 0;
}

void parse_variable_type(type_t* type)
{
    token_t* base_type_tok = next_token();
    parse_type(type);

    type_t void_type = mk_type(VOID);
    if (cmp_types(type, &void_type))
        error(base_type_tok->location, strlen(base_type_tok->data.str), "a variable cannot have the type 'void'\n", base_type_tok->data.str);
}

void parse_func_parameters(function_call_t* func_call)
{
    expect(TOK_OPEN_PARENTHESIS);
    DYNARRAY_INIT(func_call->arguments, 4);

    if (next_token()->type != TOK_CLOSE_PARENTHESIS)
    {
        expression_t* expr = (expression_t*)danpa_alloc(sizeof(expression_t));
        parse_expr(expr, 0);
        DYNARRAY_ADD(func_call->arguments, expr);
        while (accept(TOK_COMMA))
        {
            expr = (expression_t*)danpa_alloc(sizeof(expression_t));
            parse_expr(expr, 0);
            DYNARRAY_ADD(func_call->arguments, expr);
        }
    }

    token_t* tok = expect(TOK_CLOSE_PARENTHESIS);
    func_call->length = tok->location.ptr - func_call->call_expr->loc.ptr;
}

void parse_match_pattern(match_pattern_t* match_pattern)
{
    token_t* tok;
    match_pattern->loc = next_token()->location;

    if ((tok = accept(TOK_INTEGER_LITERAL)))
    {
        token_t* tok2;
        if ((tok2 = accept(TOK_SLICE_DOTS)))
        {
            tok2 = expect(TOK_INTEGER_LITERAL);

            match_pattern->left_bound = tok;
            match_pattern->right_bound = tok2;
            match_pattern->type = PAT_RANGE;
        }
        else
        {
            match_pattern->int_constant = tok;
            match_pattern->type = PAT_INT_LIT;
        }
    }
    else if ((tok = accept(TOK_STRING_LITERAL)))
    {
        match_pattern->string_lit = tok;
        match_pattern->type = PAT_STR_LIT;
    }
    else if ((tok = accept(TOK_IDENTIFIER)))
    {
        match_pattern->ident.name = tok;
        match_pattern->ident.flags = 0;
        match_pattern->type = PAT_IDENT;
    }
    else
    {
        error(next_token()->location, next_token()->length, "expected pattern\n");
    }

    match_pattern->length = prev_token()->location.ptr + prev_token()->length - match_pattern->loc.ptr;
}

void parse_match_case(match_case_t* match_case)
{
    DYNARRAY_INIT(match_case->patterns, 4);
    match_case->expr = danpa_alloc(sizeof(expression_t));

    if (next_token()->type == TOK_IDENTIFIER && strcmp(next_token()->data.str, "_"))
    {
        consume_token();
        match_case->is_wildcard = 1;
    }
    else
    {
        match_case->is_wildcard = 0;
        do
        {
            match_pattern_t pattern;
            parse_match_pattern(&pattern);

            DYNARRAY_ADD(match_case->patterns, pattern);
        } while (accept_op(OP_BITOR));
    }

    match_case->loc = match_case->patterns.ptr[0].loc;

    expect(TOK_MATCH_OP);

    parse_expr(match_case->expr, 0);

    match_case->length = prev_token()->location.ptr + prev_token()->length -  match_case->loc.ptr;
}

void parse_prim_expr(primary_expression_t* value);
void parse_incdec(token_t* tok, primary_expression_t* value)
{
    primary_expression_t* assigned_expr = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
    parse_prim_expr(assigned_expr);

    assignment_t* assignment = (assignment_t*)danpa_alloc(sizeof(assignment_t));
    assignment->eq_token = tok;
    assignment->var = *assigned_expr;

    expression_t* right = (expression_t*)danpa_alloc(sizeof(expression_t));
    right->kind = PRIM_EXPR;
    right->flags = 0; right->loc = assigned_expr->loc; right->length = 0;
    right->prim_expr.loc = right->loc; right->prim_expr.length = 0;
    right->prim_expr.type = INT_CONSTANT;
    right->prim_expr.int_constant = (token_t*)danpa_alloc(sizeof(token_t));
    right->prim_expr.int_constant->data.integer = 1;

    binop_t* binop = (binop_t*)danpa_alloc(sizeof(binop_t));
    binop->left.kind = PRIM_EXPR;
    binop->left.prim_expr = *assigned_expr;
    binop->left.loc = assigned_expr->loc;
    binop->left.length = assigned_expr->length;
    binop->right = *right;
    binop->op = (token_t*)danpa_alloc(sizeof(token_t));
    *binop->op = *tok;
    binop->op->data.op = (tok->data.op == OP_INC ? OP_ADD : OP_SUB);

    assignment->expr = (expression_t*)danpa_alloc(sizeof(expression_t));
    assignment->expr->kind = BINOP;
    assignment->expr->flags = 0;
    assignment->expr->binop = binop;
    assignment->discard_result = 0;

    value->type = ENCLOSED;
    value->expr = (expression_t*)danpa_alloc(sizeof(expression_t));
    value->expr->flags = 0;
    value->expr->kind = ASSIGNMENT;
    value->expr->assignment = *assignment;
}

void parse_array_lit(array_lit_expr_t* value)
{
    value->loc = next_token()->location;
    DYNARRAY_INIT(value->elements, 16);

    expect(TOK_OPEN_BRACE);
    if (!accept(TOK_CLOSE_BRACE))
        while (1)
        {
            DYNARRAY_ADD(value->elements, {});
            parse_expr(&value->elements.ptr[value->elements.size-1], 0);
            if (accept(TOK_COMMA))
                continue;
            else
            {
                expect(TOK_CLOSE_BRACE);
                break;
            }
        }

    value->length = prev_token()->location.ptr - value->loc.ptr;
}

void parse_prim_expr(primary_expression_t* value)
{
    token_t* tok;
    token_t* first_tok = next_token();
    value->loc = first_tok->location;

    if ((tok = accept(TOK_OPEN_PARENTHESIS)))
    {
        if (token_is_type(next_token()))
        {
            parse_type(&value->cast_expr.target_type);
            value->type = CAST_EXPRESSION;
            value->cast_expr.cast_type_token = tok;
            expect(TOK_CLOSE_PARENTHESIS);

            value->cast_expr.expr = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
            parse_prim_expr(value->cast_expr.expr);
        }
        else
        {
            expression_t expr;
            parse_expr(&expr, 0);
            expect(TOK_CLOSE_PARENTHESIS);

            value->type = ENCLOSED;
            expression_t* factor_expr = (expression_t*)danpa_alloc(sizeof(expression_t));
            *factor_expr = expr;
            value->expr = factor_expr;
        }
    }
    else if (next_token()->type == TOK_OPEN_BRACE)
    {
        array_lit_expr_t array_lit;
        parse_array_lit(&array_lit);

        value->type = ARRAY_LIT;
        value->array_lit = array_lit;
    }
    else if ((tok = accept(TOK_OPEN_BRACKET)))
    {
        array_range_expr_t array_range;
        array_range.left_bound  = danpa_alloc(sizeof(primary_expression_t));
        array_range.right_bound = danpa_alloc(sizeof(primary_expression_t));

        parse_prim_expr(array_range.left_bound);
        expect(TOK_SLICE_DOTS);
        parse_prim_expr(array_range.right_bound);
        expect(TOK_CLOSE_BRACKET);

        value->type = ARRAY_RANGE_GEN;
        value->array_range = array_range;
    }
    // prefix increment/decrement
    else if ((tok = accept_op(OP_INC)) || (tok = accept_op(OP_DEC)))
    {
        parse_incdec(tok, value);
    }
    else if ((tok = accept_op(OP_MUL)))
    {
        primary_expression_t* expr = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
        parse_prim_expr(expr);

        value->type = POINTER_DEREF;
        value->deref.asterisk_token = tok;
        value->deref.pointer_expr = expr;
    }
    else if ((tok = accept_op(OP_BITAND)))
    {
        primary_expression_t* expr = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
        parse_prim_expr(expr);

        value->type = ADDR_GET;
        value->addr.addr_token = tok;
        value->addr.addr_expr = expr;
    }
    else if ((tok = accept_op(OP_ADD)) || (tok = accept_op(OP_SUB)) || (tok = accept_op(OP_LOGICNOT)) || (tok = accept_op(OP_BITNOT))
             || (tok = accept(TOK_QUESTION)))
    {
        primary_expression_t* unary_op_factor = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
        parse_prim_expr(unary_op_factor);

        value->type = UNARY_OP_FACTOR;
        value->unary_expr.unary_op = tok;
        value->unary_expr.unary_value = unary_op_factor;
    }
    else if (accept_op(OP_MOD)) // '%' token
    {
        primary_expression_t* random_expr = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
        parse_prim_expr(random_expr);

        if (accept(TOK_SLICE_DOTS))
        {
            primary_expression_t* right_expr = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
            parse_prim_expr(right_expr);

            value->type = RAND_EXPR;
            value->rand_expr.is_range = 1;
            value->rand_expr.left_bound = random_expr;
            value->rand_expr.right_bound = right_expr;
        }
        else
        {
            value->type = RAND_EXPR;
            value->rand_expr.is_range = 0;
            value->rand_expr.random_expr = random_expr;
        }
    }
    else if (accept(KEYWORD_NULL))
    {
        value->type = NULL_LITERAL;
    }
    else if (accept(KEYWORD_ASM))
    {
        DYNARRAY_INIT(value->asm_expr.arguments, 4);

        expect(TOK_OPEN_PARENTHESIS);
        token_t* code = expect(TOK_STRING_LITERAL);
        while (accept(TOK_COMMA))
        {
            expression_t* expr = (expression_t*)danpa_alloc(sizeof(expression_t));
            parse_expr(expr, 0);
            DYNARRAY_ADD(value->asm_expr.arguments, expr);
        }
        if (accept(TOK_COLON))
            parse_type(&value->asm_expr.ret_type);
        else
            value->asm_expr.ret_type = mk_type(VOID);
        expect(TOK_CLOSE_PARENTHESIS);

        value->type = ASM_EXPR;
        value->asm_expr.asm_code = code->data.str;
    }
    else if (accept(KEYWORD_SIZEOF))
    {
        value->sizeof_expr.loc = next_token()->location;
        expect(TOK_OPEN_PARENTHESIS);
        if (maybe_parse_type())
        {
            parse_type(&value->sizeof_expr.type);
            value->sizeof_expr.is_expr = 0;
        }
        else
        {
            expression_t* expr = (expression_t*)danpa_alloc(sizeof(expression_t));
            parse_expr(expr, 0);
            value->sizeof_expr.expr = expr;
            value->sizeof_expr.is_expr = 1;
        }
        expect(TOK_CLOSE_PARENTHESIS);
        value->sizeof_expr.length = prev_token()->location.ptr - value->sizeof_expr.loc.ptr;

        value->type = SIZEOF_EXPR;
    }
    else if (accept(KEYWORD_NEW))
    {
        value->new_expr.loc = next_token()->location;

        type_t type;
        parse_type(&type);

        value->type = NEW_EXPR;
        value->new_expr.new_type = type;
        value->new_expr.length = prev_token()->location.ptr - value->new_expr.loc.ptr;
    }
    else if (accept(KEYWORD_MATCH))
    {
        expect(TOK_OPEN_PARENTHESIS);
        value->match_expr.tested_expr = danpa_alloc(sizeof(expression_t));
        parse_expr(value->match_expr.tested_expr, 0);
        expect(TOK_CLOSE_PARENTHESIS);
        expect(TOK_OPEN_BRACE);

        DYNARRAY_INIT(value->match_expr.cases, 8);
        do
        {
            match_case_t match_case;
            parse_match_case(&match_case);
            DYNARRAY_ADD(value->match_expr.cases, match_case);

            if (forward(1)->type != TOK_CLOSE_BRACE) // last comma
                expect(TOK_COMMA);
            else
                accept(TOK_COMMA);
        } while (!accept(TOK_CLOSE_BRACE));

        value->type = MATCH_EXPR;
    }
    else if ((tok = accept(TOK_IDENTIFIER)))
    {
        type_t type = get_type(tok->data.str);
        if (is_struct(&type))
        {
            // struct initializer
            value->type = STRUCT_INIT;
            value->struct_init.type = type;
            value->struct_init.loc = tok->location;

            expect(TOK_OPEN_PARENTHESIS);
            DYNARRAY_INIT(value->struct_init.elements, 8);
            do
            {
                DYNARRAY_ADD(value->struct_init.elements, {});
                parse_expr(&value->struct_init.elements.ptr[value->struct_init.elements.size-1], 0);

                if (accept(TOK_CLOSE_PARENTHESIS))
                    break;

                if (forward(1)->type != TOK_CLOSE_PARENTHESIS) // last comma
                    expect(TOK_COMMA);
                else
                    accept(TOK_COMMA);
            } while (1);

            value->struct_init.length = (prev_token()->location.ptr+prev_token()->length) - value->struct_init.loc.ptr;
        }
        else
        {
            value->type = IDENT;
            value->ident.name = tok;
            value->ident.flags = 0;
        }
    }
    else if ((tok = accept(TOK_INTEGER_LITERAL)))
    {
        value->type = INT_CONSTANT;
        value->int_constant = tok;
    }
    else if ((tok = accept(TOK_FLOAT_LITERAL)))
    {
        value->type = FLOAT_CONSTANT;
        value->flt_constant = tok;
    }
    else if ((tok = accept(TOK_STRING_LITERAL)))
    {
        value->type = STRING_LITERAL;
        value->string_lit = tok;
    }
    else
    {
        tok = next_token();
        error(tok->location, 1, "expected expression got '%s'\n", tokens_str[tok->type]);
    }

    // array subscript
    for (;;)
    {
        token_t* last_token = next_token();
        if ((tok = accept(TOK_OPEN_BRACKET)))
        {
            primary_expression_t* expr_within = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
            expression_t* sub_expr    = (expression_t*)danpa_alloc(sizeof(expression_t));
            parse_expr(sub_expr, 0);
            if (accept(TOK_SLICE_DOTS))
            {
                expression_t* right_expr    = (expression_t*)danpa_alloc(sizeof(expression_t));
                parse_expr(right_expr, 0);

                expect(TOK_CLOSE_BRACKET);

                *expr_within = *value;
                expr_within->length = last_token->location.ptr - first_tok->location.ptr;
                value->type = ARRAY_SLICE;
                value->array_slice.bracket_token = tok;
                value->array_slice.array_expr = expr_within;
                value->array_slice.left_expr = sub_expr;
                value->array_slice.right_expr = right_expr;
            }
            else
            {
                expect(TOK_CLOSE_BRACKET);

                *expr_within = *value;
                expr_within->length = last_token->location.ptr - first_tok->location.ptr;
                value->type = ARRAY_SUBSCRIPT;
                value->array_sub.bracket_token = tok;
                value->array_sub.array_expr = expr_within;
                value->array_sub.subscript_expr = sub_expr;
            }
        }
        else if ((tok = accept(TOK_DOT)) || (tok = accept(TOK_ARROW)))
        {
            primary_expression_t* expr_within = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
            *expr_within = *value;
            expr_within->length = last_token->location.ptr - first_tok->location.ptr;

            // UFCS
            if (next_token()->type == TOK_IDENTIFIER && forward(1)->type == TOK_OPEN_PARENTHESIS &&
                has_function(next_token()->data.str))
            {
                primary_expression_t* func_name_expr = danpa_alloc(sizeof(primary_expression_t));
                func_name_expr->type = IDENT;
                func_name_expr->loc = next_token()->location; func_name_expr->length = next_token()->length;
                func_name_expr->ident.name = expect(TOK_IDENTIFIER);
                func_name_expr->ident.flags = 0;

                value->type = FUNCTION_CALL;
                value->func_call.call_expr = func_name_expr;

                parse_func_parameters(&value->func_call);

                expression_t* arg_expr = danpa_alloc(sizeof(expression_t));
                arg_expr->loc = expr_within->loc;
                arg_expr->length = expr_within->length;
                arg_expr->kind = PRIM_EXPR;
                arg_expr->flags = 0;
                arg_expr->prim_expr = *expr_within;

                // add the argument
                DYNARRAY_RESIZE(value->func_call.arguments, value->func_call.arguments.size+1);
                // move the previous arguments to the right
                memmove(&value->func_call.arguments.ptr[1], &value->func_call.arguments.ptr[0], sizeof(struct expression_t*)*(value->func_call.arguments.size-1));
                // add 'this' as the first argument
                value->func_call.arguments.ptr[0] = arg_expr;
            }
            else
            {
                token_t* field = expect(TOK_IDENTIFIER);

                value->type = STRUCT_ACCESS;
                value->struct_access.struct_expr = expr_within;
                value->struct_access.indirect_access = (tok->type == TOK_ARROW);
                value->struct_access.field_name = field;
            }
        }
        else if (next_token()->type == TOK_OPEN_PARENTHESIS)
        {
            primary_expression_t* expr_within = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
            *expr_within = *value;
            expr_within->length = last_token->location.ptr - first_tok->location.ptr;

            value->type = FUNCTION_CALL;
            value->func_call.call_expr = expr_within;

            parse_func_parameters(&value->func_call);
        }
        else
            break;
    }

    value->length = (next_token()->location.ptr) - first_tok->location.ptr;
}

void parse_return_statement(return_statement_t* ret_statement)
{
    expression_t expr;

    ret_statement->return_token = expect(KEYWORD_RETURN);
    if (accept(TOK_SEMICOLON))
    {
        ret_statement->empty_return = 1;
    }
    else
    {
        parse_expr(&expr, 0);

        ret_statement->expr = expr;
        ret_statement->empty_return = 0;

        expect(TOK_SEMICOLON);
    }
}

void parse_statement(statement_t* statement);

void parse_if_statement(if_statement_t* if_statement)
{
    if_statement->statement = (statement_t*)danpa_alloc(sizeof(statement_t));

    expect(KEYWORD_IF);
    expect(TOK_OPEN_PARENTHESIS);
    parse_expr(&if_statement->test, 0);
    expect(TOK_CLOSE_PARENTHESIS);
    parse_statement(if_statement->statement);

    if (accept(KEYWORD_ELSE))
    {
        statement_t* stat = (statement_t*)(danpa_alloc(sizeof(statement_t)));

        parse_statement(stat);

        if_statement->else_statement = stat;
    }
    else
    {
        if_statement->else_statement = NULL;
    }
}

void parse_while_statement(while_statement_t* while_statement)
{
    while_statement->statement = (statement_t*)danpa_alloc(sizeof(statement_t));

    expect(KEYWORD_WHILE);
    expect(TOK_OPEN_PARENTHESIS);
    parse_expr(&while_statement->test, 0);
    expect(TOK_CLOSE_PARENTHESIS);
    parse_statement(while_statement->statement);
}

void parse_do_while_statement(do_while_statement_t* while_statement)
{
    while_statement->statement = (statement_t*)danpa_alloc(sizeof(statement_t));

    expect(KEYWORD_DO);
    parse_statement(while_statement->statement);

    expect(KEYWORD_WHILE);
    expect(TOK_OPEN_PARENTHESIS);
    parse_expr(&while_statement->test, 0);
    expect(TOK_CLOSE_PARENTHESIS);
    expect(TOK_SEMICOLON);
}

void parse_loop_ctrl_statement(loop_ctrl_statement_t* loop_ctrl_statement)
{
    token_t* tok;
    if ((tok = accept(KEYWORD_BREAK)))
    {
        loop_ctrl_statement->tok = tok;
        loop_ctrl_statement->type = LOOP_BREAK;
    }
    else
    {
        tok = expect(KEYWORD_CONTINUE);

        loop_ctrl_statement->tok = tok;
        loop_ctrl_statement->type = LOOP_CONTINUE;
    }

    expect(TOK_SEMICOLON);
}

void parse_for_statement(for_statement_t* for_statement)
{
    expect(KEYWORD_FOR);
    expect(TOK_OPEN_PARENTHESIS);

    statement_t* init_statement = (statement_t*)danpa_alloc(sizeof(statement_t));
    parse_statement(init_statement);

    parse_expr(&for_statement->test, 0);
    expect(TOK_SEMICOLON);

    parse_expr(&for_statement->loop_expr, 0);
    if (for_statement->loop_expr.kind == ASSIGNMENT)
        for_statement->loop_expr.assignment.discard_result = 1;

    expect(TOK_CLOSE_PARENTHESIS);

    statement_t* statement = (statement_t*)danpa_alloc(sizeof(statement_t));
    parse_statement(statement);

    for_statement->init_statement = init_statement;
    for_statement->statement = statement;
}

void parse_foreach_statement(foreach_statement_t* foreach_statement)
{
    expect(KEYWORD_FOREACH);
    if (next_token()->type == TOK_IDENTIFIER && strcmp(next_token()->data.str, "ref") == 0)
    {
        consume_token();
        foreach_statement->foreach_ref = 1;
    }
    else
        foreach_statement->foreach_ref = 0;

    expect(TOK_OPEN_PARENTHESIS);

    // variable type is specified
    if (!(next_token()->type == TOK_IDENTIFIER && (forward(1)->type == TOK_OPERATOR && forward(1)->data.op == OP_IN)))
    {
        foreach_statement->loop_var_type = danpa_alloc(sizeof(type_t));
        parse_type(foreach_statement->loop_var_type);
    }
    else
        foreach_statement->loop_var_type = NULL;

    foreach_statement->loop_ident.name = expect(TOK_IDENTIFIER);
    foreach_statement->loop_ident.flags = 0;
    expect_op(OP_IN);

    parse_expr(&foreach_statement->array_expr, 0);
    expect(TOK_CLOSE_PARENTHESIS);

    statement_t* statement = (statement_t*)danpa_alloc(sizeof(statement_t));
    parse_statement(statement);

    foreach_statement->statement = statement;
}

void parse_typedef_declaration(typedef_declaration_t* decl)
{
    expect(KEYWORD_TYPEDEF);
    parse_type(&decl->type);
    decl->name = expect(TOK_IDENTIFIER);

    if (get_type(decl->name->data.str).base_type != INVALID_TYPE)
        error(decl->name->location, 1, "typename '%s' is already taken\n", decl->name->data.str);

    add_typedef(decl->name->data.str, decl->type);

    expect(TOK_SEMICOLON);
}

void parse_assignment_rhs(assignment_t* assignment)
{
    token_t* tok = consume_token();
    assignment->eq_token = tok;
    assignment->expr = (expression_t*)danpa_alloc(sizeof(expression_t));
    parse_expr(assignment->expr, 0);
    if (tok->type == TOK_ASSIGNMENT_OP)
    {

    }
    else
    {

        binop_t* binop = (binop_t*)danpa_alloc(sizeof(binop_t));
        binop->left.kind = PRIM_EXPR;
        binop->left.prim_expr = assignment->var;
        binop->left.loc = assignment->var.loc;
        binop->left.length = assignment->var.length;
        binop->right = *assignment->expr;
        binop->op = (token_t*)danpa_alloc(sizeof(token_t));
        *binop->op = *assignment->eq_token;

        assignment->expr->kind = BINOP;
        assignment->expr->binop = binop;

        switch (tok->type)
        {
            case TOK_ADD_ASSIGNMENT_OP:
                binop->op->data.op = OP_ADD;
                break;
            case TOK_SUB_ASSIGNMENT_OP:
                binop->op->data.op = OP_SUB;
                break;
            case TOK_MUL_ASSIGNMENT_OP:
                binop->op->data.op = OP_MUL;
                break;
            case TOK_DIV_ASSIGNMENT_OP:
                binop->op->data.op = OP_DIV;
                break;
            case TOK_MOD_ASSIGNMENT_OP:
                binop->op->data.op = OP_MOD;
                break;
            case TOK_CAT_ASSIGNMENT_OP:
                binop->op->data.op = OP_CAT;
                break;
            default:
                error(tok->location, tok->length, "expected assignment operator\n");

        }
    }
}

void parse_assignment(assignment_t* assignment)
{
    parse_prim_expr(&assignment->var);
    parse_assignment_rhs(assignment);
    assignment->discard_result = 1;
}

void parse_variable_declaration(variable_declaration_t* decl)
{
    parse_variable_type(&decl->type);
    decl->name = expect(TOK_IDENTIFIER);
    token_t* tok = next_token();
    if (tok->type == TOK_ASSIGNMENT_OP)
    {
        assignment_t* assigment = (assignment_t*)danpa_alloc(sizeof(assignment_t));
        assigment->var.type = IDENT;
        assigment->var.ident.name = decl->name;
        assigment->var.ident.flags = 0;
        assigment->var.loc = tok->location;
        assigment->var.length = tok->length;
        parse_assignment_rhs(assigment);

        assigment->eq_token = tok;
        assigment->discard_result = 1;

        decl->init_assignment = assigment;
    }
    else
    {
        decl->init_assignment = NULL;
    }
    expect(TOK_SEMICOLON);
}

void parse_struct_declaration(struct_declaration_t* decl)
{
    DYNARRAY_INIT(decl->field_decls, 16);

    expect(KEYWORD_STRUCT);
    decl->structure.name = expect(TOK_IDENTIFIER);
    expect(TOK_OPEN_BRACE);

    type_t invalid = mk_type(INVALID_TYPE);
    type_t search = get_type(decl->structure.name->data.str);
    if (!cmp_types(&search, &invalid))
    {
        // ignore if forward declared
        if (is_struct(&search) && get_struct(&search)->incomplete)
            ;
        else
            error(decl->structure.name->location, decl->structure.name->length, "type '%s' already exists\n", decl->structure.name->data.str);
    }
    type_t struct_type = forward_declare_structure(decl->structure.name->data.str);

    while (!accept(TOK_CLOSE_BRACE))
    {
        variable_declaration_t field_decl;
        parse_variable_declaration(&field_decl);
        DYNARRAY_ADD(decl->field_decls, field_decl);
    }

    DYNARRAY_INIT(decl->structure.fields, decl->field_decls.size);
    DYNARRAY_RESIZE(decl->structure.fields, decl->field_decls.size);
    unsigned byte_offset = 0;
    for (int i = 0; i < decl->field_decls.size; ++i)
    {
        decl->structure.fields.ptr[i].name = decl->field_decls.ptr[i].name;
        decl->structure.fields.ptr[i].type = decl->field_decls.ptr[i].type;
        decl->structure.fields.ptr[i].byte_size = sizeof_type(&decl->field_decls.ptr[i].type);
        decl->structure.fields.ptr[i].offset = byte_offset;
        byte_offset += decl->structure.fields.ptr[i].byte_size;
    }
    decl->structure.byte_size = byte_offset; // cumulative size
    define_structure(&struct_type, &decl->structure);

    if (!accept(TOK_SEMICOLON))
    {
        error(decl->structure.name->location, decl->structure.name->length, "expected a ';' at the end of the struct declaration");
    }
}

void parse_declaration(declaration_t* decl)
{
    if (token_is_type(next_token()))
    {
        decl->type = VARIABLE_DECLARATION;
        parse_variable_declaration(&decl->var);
    }
    else if (next_token()->type == KEYWORD_TYPEDEF)
    {
        decl->type = TYPEDEF_DECLARATION;
        parse_typedef_declaration(&decl->typedef_decl);
    }
    else
    {
        decl->type = STRUCT_DECLARATION;
        parse_struct_declaration(&decl->struct_decl);
    }
}

void parse_statement(statement_t* statement)
{
    if (accept(TOK_SEMICOLON))
    {
        statement->type = EMPTY_STATEMENT;
    }
    else if (token_is_type(next_token()) || next_token()->type == KEYWORD_TYPEDEF)
    {
        declaration_t decl;
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
    else if (next_token()->type == KEYWORD_WHILE)
    {
        while_statement_t while_statement;
        parse_while_statement(&while_statement);
        statement->type = WHILE_STATEMENT;
        statement->while_statement = while_statement;
    }
    else if (next_token()->type == KEYWORD_DO)
    {
        do_while_statement_t while_statement;
        parse_do_while_statement(&while_statement);
        statement->type = DO_WHILE_STATEMENT;
        statement->do_while_statement = while_statement;
    }
    else if (next_token()->type == KEYWORD_BREAK || next_token()->type == KEYWORD_CONTINUE)
    {
        loop_ctrl_statement_t ctrl_statement;
        parse_loop_ctrl_statement(&ctrl_statement);
        statement->type = LOOP_CTRL_STATEMENT;
        statement->loop_ctrl_statement = ctrl_statement;
    }
    else if (next_token()->type == KEYWORD_FOR)
    {
        for_statement_t for_statement;
        parse_for_statement(&for_statement);

        statement->type = FOR_STATEMENT;
        statement->for_statement = for_statement;
    }
    else if (next_token()->type == KEYWORD_FOREACH)
    {
        foreach_statement_t foreach_statement;
        parse_foreach_statement(&foreach_statement);

        statement->type = FOREACH_STATEMENT;
        statement->foreach_statement = foreach_statement;
    }
    else if (next_token()->type == TOK_OPEN_BRACE)
    {
        consume_token();

        DYNARRAY_INIT(statement->compound.statement_list, 8);

        while (!accept(TOK_CLOSE_BRACE))
        {
            statement_t st;
            parse_statement(&st);
            DYNARRAY_ADD(statement->compound.statement_list, st);
        }

        statement->type = COMPOUND_STATEMENT;
    }
    else
    {
        expression_t expr;
        parse_expr(&expr, 0);

        if (expr.kind == ASSIGNMENT)
            expr.assignment.discard_result = 1; // don't need to push the value on the stack

        expect(TOK_SEMICOLON);
        statement->expression = expr;
        statement->type = DISCARDED_EXPRESSION;
    }
}

void parse_function(function_t* func)
{
    DYNARRAY_INIT(func->args, 8);
    DYNARRAY_INIT(func->statement_list, 128);
    DYNARRAY_INIT(func->signature.parameter_types, 8);

    parse_type(&func->signature.ret_type);
    token_t* name = expect(TOK_IDENTIFIER);
    if (strcmp(name->data.str, "operator") == 0)
    {
        token_t* op = expect(TOK_OPERATOR);
        func->is_operator_overload = 1;
        func->overloaded_op = op->data.op;
    }
    else
        func->is_operator_overload = 0;
    expect(TOK_OPEN_PARENTHESIS);
    if (next_token()->type == TOK_IDENTIFIER)
    {
        parameter_t param;
        parse_variable_type(&param.type);
        param.name = expect(TOK_IDENTIFIER);
        DYNARRAY_ADD(func->args, param);
        DYNARRAY_ADD(func->signature.parameter_types, param.type);

        while (accept(TOK_COMMA))
        {
            parse_variable_type(&param.type);
            param.name = expect(TOK_IDENTIFIER);
            DYNARRAY_ADD(func->args, param);
            DYNARRAY_ADD(func->signature.parameter_types, param.type);
        }
    }
    expect(TOK_CLOSE_PARENTHESIS);
    expect(TOK_OPEN_BRACE);

    func->name = name;

    while (next_token()->type != TOK_CLOSE_BRACE)
    {
        statement_t statement;
        parse_statement(&statement);
        DYNARRAY_ADD(func->statement_list, statement);
    }

    expect(TOK_CLOSE_BRACE);
}

int maybe_func_decl()
{
    REWIND_BEGIN(maybe_func);

    type_t type;
    parse_type(&type);

    expect(TOK_IDENTIFIER);
    accept(TOK_OPERATOR); // operator overloads
    expect(TOK_OPEN_PARENTHESIS);

    REWIND_END(maybe_func);

    return REWIND_FAILED(maybe_func) == 0;
}

void parse_program(program_t* program)
{
    current_program = program;

    DYNARRAY_INIT(program->function_list, 16);
    DYNARRAY_INIT(program->global_declarations, 32);
    program->strings = mk_hash_table(256);

    while (next_token()->type != TOKEN_EOF)
    {
        // function declaration
        //if (token_is_type(next_token()) && forward(1)->type == TOK_IDENTIFIER && forward(2)->type == TOK_OPEN_PARENTHESIS)
        if (maybe_func_decl())
        {
            function_t func;
            parse_function(&func);
            if (func.is_operator_overload)
                register_overload(&func);
            DYNARRAY_ADD(program->function_list, func);
        }
        else
        {
            declaration_t decl;
            parse_declaration(&decl);
            DYNARRAY_ADD(program->global_declarations, decl);
        }
    }
}

void parse_non_ternary_expr(expression_t* expr, int expr_precedence)
{
    expression_t* lhs = (expression_t*)danpa_alloc(sizeof(expression_t));
    lhs->flags = 0;

    token_t* first_tok = next_token();
    lhs->loc = first_tok->location;

    primary_expression_t val;
    parse_prim_expr(&val);
    lhs->kind = PRIM_EXPR;
    lhs->prim_expr = val;

    token_t* op = next_token();

    if (op->type >= TOK_ASSIGNMENT_OP && op->type < TOK_ASSIGNMENT_END)
    {
        parse_assignment_rhs(&lhs->assignment);
        lhs->kind = ASSIGNMENT;
        lhs->assignment.discard_result = 0;
    }
    else
    {
        while (op->type == TOK_OPERATOR && is_binop(op->data.op)
               && operators[op->data.op].precedence >= expr_precedence)
        {
            consume_token();

            lhs->length = (prev_token()->location.ptr + prev_token()->length) - first_tok->location.ptr;

            expression_t rhs;
            parse_non_ternary_expr(&rhs, operators[op->data.op].precedence + 1);

            expression_t* new_node = (expression_t*)danpa_alloc(sizeof(expression_t));

            new_node->loc = lhs->loc;
            new_node->kind = BINOP;
            new_node->flags = 0;
            new_node->binop = (binop_t*)danpa_alloc(sizeof(binop_t));
            new_node->binop->left = *lhs;
            new_node->binop->right = rhs;
            new_node->binop->op = op;

            lhs = new_node;

            op = next_token();
        }
    }

    *expr = *lhs;

    expr->length = (prev_token()->location.ptr + prev_token()->length) - first_tok->location.ptr;
}

void parse_expr(expression_t* expr, int expr_precedence)
{
    parse_non_ternary_expr(expr, expr_precedence);

    // ternary expression
    if (accept(TOK_QUESTION))
    {
        expression_t* cond_branch = (expression_t*)danpa_alloc(sizeof(expression_t));
        expression_t* true_branch  = (expression_t*)danpa_alloc(sizeof(expression_t));
        expression_t* false_branch = (expression_t*)danpa_alloc(sizeof(expression_t));

        *cond_branch = *expr;
        parse_expr(true_branch,  0);
        expect(TOK_COLON);
        parse_expr(false_branch, 0);

        expr->kind = TERNARY_EXPR;
        expr->ternary.cond_expr = cond_branch;
        expr->ternary.true_branch = true_branch;
        expr->ternary.false_branch = false_branch;
    }
}
