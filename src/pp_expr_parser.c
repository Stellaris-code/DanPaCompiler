#include "pp_expr_parser.h"

#include <assert.h>

#include "hash_table.h"
#include "dynarray.h"
#include "preprocessor.h"
#include "error.h"

typedef struct rpl_token_t
{
    enum
    {
        RPL_LITERAL,
        RPL_OP
    } type;
    union
    {
        int value;
        int op;
    };
} rpl_token_t;

typedef DYNARRAY(rpl_token_t) rpl_stack_t;

int evaluate_rpl_input(const source_location_t* start, int length, const rpl_stack_t* rpl_stack)
{
    DYNARRAY(int) data_stack;
    DYNARRAY_INIT(data_stack, 4);

    for (int i = 0; i < rpl_stack->size; ++i)
    {
        if (rpl_stack->ptr[i].type == RPL_LITERAL)
            DYNARRAY_ADD(data_stack, rpl_stack->ptr[i].value);
        else
        {
            int op = rpl_stack->ptr[i].op;
            if (op == OP_CAT)
                error(*start, length, "cat operator cannot be used in an #if expression\n");
            if (op >= OP_BIN_END && op < OP_UNARY_END) // unary op
            {
                if (data_stack.size <= 0)
                    error(*start, length, "invalid #if expression\n");

                data_stack.ptr[data_stack.size-1] = eval_int_unop(op, data_stack.ptr[data_stack.size-1]);
            }
            else
            {
                if (data_stack.size <= 1)
                    error(*start, length, "invalid #if expression\n");

                int x = data_stack.ptr[data_stack.size-2];
                int y = data_stack.ptr[data_stack.size-1];
                int result = eval_int_binop(op, x, y);
                DYNARRAY_POP(data_stack);
                data_stack.ptr[data_stack.size-1] = result;
            }
        }
    }

    if (data_stack.size != 1)
        error(*start, length, "invalid #if expression\n");

    return data_stack.ptr[0];
}

int pp_evaluate_expr(const token_list_t *tokens)
{
    rpl_stack_t rpl_stack;
    DYNARRAY_INIT(rpl_stack, 16);
    DYNARRAY(int) op_stack;
    DYNARRAY_INIT(op_stack, 4);

    rpl_token_t rpl_token;
    rpl_token.type = 0; rpl_token.value = 0; // silence warnings

    for (int i = 0; i < tokens->size; ++i)
    {
        token_t* token = &tokens->ptr[i];

        switch (token->type)
        {
            case TOK_INTEGER_LITERAL:
                rpl_token.type = RPL_LITERAL;
                rpl_token.value = token->data.integer;
                DYNARRAY_ADD(rpl_stack, rpl_token);
                break;
            case TOK_IDENTIFIER: // defined(...) : '1' or '0' if the macro is either defined or not
            {
                if (i+3 < tokens->size && strncmp(token->data.str, "defined", 7) == 0
                    && tokens->ptr[i+1].type == TOK_OPEN_PARENTHESIS
                    && tokens->ptr[i+2].type == TOK_IDENTIFIER
                    && tokens->ptr[i+3].type == TOK_CLOSE_PARENTHESIS)
                {
                    rpl_token.type = RPL_LITERAL;
                    if (hash_table_get(&macro_definitions, tokens->ptr[i+2].data.str))
                        rpl_token.value = 1;
                    else
                        rpl_token.value = 0;

                    i += 4;
                }
                else
                    error(token->location, token->length, "expected 'defined(<MACRO>)'\n");

                DYNARRAY_ADD(rpl_stack, rpl_token);
                break;
            }
            case TOK_OPERATOR:
                // unary operator
                if (token->data.op >= OP_BIN_END && token->data.op < OP_UNARY_END)
                {
                    rpl_token.type = RPL_OP;
                    rpl_token.op = token->data.op;
                    DYNARRAY_ADD(op_stack, token->data.op);
                }
                // binary operator
                else
                {
                    while  (op_stack.size > 0 &&
                           (op_stack.ptr[op_stack.size-1] != OP_LEFT_PARENTHESIS) &&
                           (operators[op_stack.ptr[op_stack.size-1]].precedence > operators[token->data.op].precedence
                            || (op_stack.ptr[op_stack.size-1] >= OP_BIN_END && op_stack.ptr[op_stack.size-1] < OP_UNARY_END)))
                    {
                        rpl_token.type = RPL_OP;
                        rpl_token.op = op_stack.ptr[op_stack.size-1];
                        DYNARRAY_ADD(rpl_stack, rpl_token);
                        DYNARRAY_POP(op_stack);
                    }

                    DYNARRAY_ADD(op_stack, token->data.op);
                }
                break;
            case TOK_OPEN_PARENTHESIS:
                DYNARRAY_ADD(op_stack, OP_LEFT_PARENTHESIS);
                break;
            case TOK_CLOSE_PARENTHESIS:
                while (op_stack.size > 0 &&
                       op_stack.ptr[op_stack.size-1] != OP_LEFT_PARENTHESIS)
                {
                    rpl_token.type = RPL_OP;
                    rpl_token.op = op_stack.ptr[op_stack.size-1];
                    DYNARRAY_ADD(rpl_stack, rpl_token);
                    DYNARRAY_POP(op_stack);
                }

                if (op_stack.size == 0)
                    error(token->location, token->length, "unmatched parenthesis found\n");
                DYNARRAY_POP(op_stack); // pop the '('
                break;
            default:
                error(token->location, token->length, "unexpected token %s\n", tokens_str[token->type]);
        }
    }
    // while there are remaining operators on the op stack
    while (op_stack.size > 0)
    {
        rpl_token.type = RPL_OP;
        rpl_token.op   = op_stack.ptr[op_stack.size-1];
        DYNARRAY_POP(op_stack);
        DYNARRAY_ADD(rpl_stack, rpl_token);
    }

    source_location_t loc_begin = tokens->ptr[0].location;
    int length = tokens->ptr[tokens->size-1].location.ptr + tokens->ptr[tokens->size-1].length - tokens->ptr[0].location.ptr;

    return evaluate_rpl_input(&loc_begin, length, &rpl_stack);
}
