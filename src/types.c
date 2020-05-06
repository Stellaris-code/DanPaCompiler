/*
types.c

Copyright (c) 25 Yann BOUCHER (yann)

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
#include "error.h"
#include "ast_nodes.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

        typedef DYNARRAY(const char*) types_str_t;

const char* default_types_str[DEFAULT_TYPES_END] =
    {
        "int",
        "real",
        "str",
        "void",

        "<array>",
        "<pointer>",
        "<null>",
        "<any>"
};

static types_str_t types_str;

typedef struct typedef_alias_t
{
    const char* alias;
    type_t type;
} typedef_alias_t;

static DYNARRAY(structure_t) defined_structures;
static DYNARRAY(typedef_alias_t) typedef_list;

// 0 - cannot, 1 - explicit, 2 - always
const uint8_t cast_matrix[POD_TYPES_END][POD_TYPES_END] =
    {
        // int to
        {2, 2, 0, 0},
        // real to
        {1, 2, 0, 0},
        // str to
        {0, 0, 2, 0},
        // void to
        {0, 0, 0, 0}
};

type_t mk_type(base_type_t base)
{
    type_t type;
    type.kind = BASIC;
    type.base_type = base;
    return type;
}

void types_init()
{
    DYNARRAY_INIT(defined_structures, 32);
    DYNARRAY_INIT(types_str, DEFAULT_TYPES_END + 32);
    DYNARRAY_RESIZE(types_str, DEFAULT_TYPES_END);
    memcpy(types_str.ptr, default_types_str, DEFAULT_TYPES_END*sizeof(const char*));
}

const char* type_to_str(const type_t* type)
{
    char* type_str_buffer = (char*)danpa_alloc(256);

    type_str_buffer[0] = '\0'; // reset
    int length = 0;

    DYNARRAY(int) type_stack;
    DYNARRAY_INIT(type_stack, 4);
    DYNARRAY_RESIZE(type_stack, 0);
    while (type->kind != BASIC && type->kind != FUNCTION)
    {
        DYNARRAY_ADD(type_stack, type->kind);
        if (type->kind == ARRAY)
        {
            type = type->array.array_type;
        }
        else if (type->kind == POINTER)
        {
            type = type->pointer.pointed_type;
        }
        else if (type->kind == OPTIONAL)
        {
            type = type->opt.opt_type;
        }
        else
            assert(0 && "invalid type");
    }


    if (type->kind == FUNCTION)
    {
        const char* ret_type_str = type_to_str(&type->function.signature->ret_type);
        length += snprintf(type_str_buffer+length, 256-length, "%s(", ret_type_str);
        for (int i = 0; i < type->function.signature->parameter_types.size; ++i)
        {
            const char* param_type = type_to_str(&type->function.signature->parameter_types.ptr[i]);
            length += snprintf(type_str_buffer+length, 256-length, "%s", param_type);

            if (i != type->function.signature->parameter_types.size-1)
                length += snprintf(type_str_buffer+length, 256-length, "%s", ", ");
        }

        length += snprintf(type_str_buffer+length, 256-length, "%s", ")");
    }
    else
    {
        length += snprintf(type_str_buffer+length, 256-length, "%s", types_str.ptr[type->base_type]);
    }

    for (int i = type_stack.size-1; i >= 0; --i)
    {
        if (type_stack.ptr[i] == ARRAY)
        {
            length += snprintf(type_str_buffer+length, 256-length, "%s", "[]");
        }
        else if (type_stack.ptr[i] == POINTER)
        {
            length += snprintf(type_str_buffer+length, 256-length, "%s", "*");
        }
        else if (type_stack.ptr[i] == OPTIONAL)
        {
            length += snprintf(type_str_buffer+length, 256-length, "%s", "?");
        }
    }

    return type_str_buffer;
}

const structure_t* get_struct(const type_t* type)
{
    assert(is_struct(type) && defined_structures.size > type->base_type - DEFAULT_TYPES_END);
    return &defined_structures.ptr[type->base_type - DEFAULT_TYPES_END];
}

size_t sizeof_type(const type_t* type)
{
    if (type->kind == ARRAY)
        return sizeof_type(type->array.array_type);
    if (type->kind != BASIC || type->base_type < DEFAULT_TYPES_END)
        return POD_SIZE;
    else
    {
        if (defined_structures.ptr[type->base_type - DEFAULT_TYPES_END].incomplete)
        {
            return (size_t)-1;
        }
        return defined_structures.ptr[type->base_type - DEFAULT_TYPES_END].byte_size;
    }
}

int cmp_types(const type_t* lhs, const type_t* rhs)
{
    // special cases : 'array', 'pointer' and 'any'
    if (lhs->kind == BASIC && lhs->base_type == SPEC_ARRAY && (rhs->kind == ARRAY || (rhs->kind == BASIC && rhs->base_type == STR)))
        return 1;
    if (rhs->kind == BASIC && rhs->base_type == SPEC_ARRAY && (lhs->kind == ARRAY || (lhs->kind == BASIC && lhs->base_type == STR)))
        return 1;
    if (lhs->kind == BASIC && lhs->base_type == SPEC_POINTER && rhs->kind == POINTER)
        return 1;
    if (rhs->kind == BASIC && rhs->base_type == SPEC_POINTER && lhs->kind == POINTER)
        return 1;
    if (lhs->kind == BASIC && lhs->base_type == SPEC_NULL && rhs->kind == POINTER)
        return 1;
    if (rhs->kind == BASIC && rhs->base_type == SPEC_NULL && lhs->kind == POINTER)
        return 1;
    if ((lhs->kind == BASIC && lhs->base_type == SPEC_ANY) ||
        (rhs->kind == BASIC && rhs->base_type == SPEC_ANY))
        return 1;

    if (lhs->kind != rhs->kind)
        return 0;
    if (lhs->kind == POINTER)
        return cmp_types(lhs->pointer.pointed_type, rhs->pointer.pointed_type);
    if (lhs->kind == OPTIONAL)
        return cmp_types(lhs->opt.opt_type, rhs->opt.opt_type);
    if (lhs->kind == ARRAY)
        return cmp_types(lhs->array.array_type, rhs->array.array_type);
    if (lhs->kind == FUNCTION)
    {
        if (!cmp_types(&lhs->function.signature->ret_type, &rhs->function.signature->ret_type))
            return 0;
        if (lhs->function.signature->parameter_types.size != rhs->function.signature->parameter_types.size)
            return 0;
        for (int i = 0; i < lhs->function.signature->parameter_types.size; ++i)
        {
            if (!cmp_types(&lhs->function.signature->parameter_types.ptr[i], &rhs->function.signature->parameter_types.ptr[i]))
                return 0;
        }
        return 1;
    }
    return lhs->base_type == rhs->base_type;
}
type_t get_expression_type(const expression_t *expr);

type_t get_binop_type(const binop_t* binop)
{
    type_t l_type = get_expression_type(&binop->left);
    type_t r_type = get_expression_type(&binop->right);

    op_overload_t* overload;

    // narray element cat
    if (l_type.kind == ARRAY && cmp_types(l_type.array.array_type, &r_type))
        return l_type;
    else if (l_type.kind == BASIC && l_type.base_type == STR && r_type.kind == BASIC && r_type.base_type == INT)
        return l_type;
    else if (binop->op->data.op == OP_IN)
    {
        assert(r_type.kind == ARRAY);
        assert(cmp_types(&l_type, r_type.array.array_type));
        return l_type;
    }
    else if ((overload = find_binop_overload(binop->op->data.op, &l_type, &r_type)))
        return overload->signature.ret_type;
    else
    {
        assert(cmp_types(&l_type, &r_type) == 1);
        if (operators[binop->op->data.op].is_bool)
            return mk_type(INT);
        else
            return l_type;
    }
}

int is_lvalue(const primary_expression_t* prim_expr)
{
    switch (prim_expr->type)
    {
        case IDENT:
        case ARRAY_SUBSCRIPT:
        case STRUCT_ACCESS:
        case POINTER_DEREF:
            return 1;
        default:
            return 0;
    }
}

type_t get_prim_expr_type(const primary_expression_t* prim_expr)
{
    type_t ptr_type;
    type_t array_type;
    op_overload_t* overload;
    switch (prim_expr->type)
    {
        case ENCLOSED:
            return get_expression_type(prim_expr->expr);
        case UNARY_OP_FACTOR:
        {
            type_t unary_type = prim_expr->unary_expr.unary_value->value_type;
            if ((overload = find_unop_overload(prim_expr->unary_expr.unary_op->data.op, &unary_type)))
                return overload->signature.ret_type;
            else
            {
                // 'optional type' test or '!'
                if (prim_expr->unary_expr.unary_op->type == TOK_QUESTION ||
                    (prim_expr->unary_expr.unary_op->type == TOK_OPERATOR &&
                     operators[prim_expr->unary_expr.unary_op->data.op].is_bool))
                {
                    return mk_type(INT); // boolean
                }
                return prim_expr->unary_expr.unary_value->value_type;
            }
        }
        case CAST_EXPRESSION:
            return prim_expr->cast_expr.target_type;
        case IDENT:
            return prim_expr->ident.type;
        case ARRAY_SUBSCRIPT:
            array_type = prim_expr->array_sub.array_expr->value_type;
            if (array_type.kind == ARRAY)
                return *prim_expr->array_sub.array_expr->value_type.array.array_type;
            else // string
                return mk_type(INT);
        case ARRAY_SLICE:
            return prim_expr->array_sub.array_expr->value_type;
        case ARRAY_RANGE_GEN:
            array_type.kind = ARRAY;
            array_type.array.array_type = danpa_alloc(sizeof(type_t));
            *array_type.array.array_type = mk_type(INT);
            array_type.array.initial_size = NULL;
            array_type.array.is_empty = 0;
            return array_type;
        case STRUCT_ACCESS:
            return prim_expr->struct_access.value_type;
        case POINTER_DEREF:
            return *prim_expr->deref.pointer_expr->value_type.pointer.pointed_type;
        case ADDR_GET:
            if (prim_expr->addr.addressed_function)
            {
                ptr_type.kind = FUNCTION;
                ptr_type.function.signature = &prim_expr->addr.addressed_function->signature;
                return ptr_type;
            }
            else
            {
                ptr_type.kind = POINTER;
                ptr_type.pointer.pointed_type = (type_t*)danpa_alloc(sizeof(type_t));
                *ptr_type.pointer.pointed_type = prim_expr->addr.addr_expr->value_type;
                return ptr_type;
            }
        case MATCH_EXPR:
            assert(prim_expr->match_expr.cases.size > 0);
            return prim_expr->match_expr.cases.ptr[0].expr->value_type;
        case ASM_EXPR:
            return prim_expr->asm_expr.ret_type;
        case SIZEOF_EXPR:
            return mk_type(INT);
        case NEW_EXPR:
            ptr_type.kind = POINTER;
            ptr_type.pointer.pointed_type = (type_t*)danpa_alloc(sizeof(type_t));
            *ptr_type.pointer.pointed_type = prim_expr->new_expr.new_type;
            return ptr_type;
        case RAND_EXPR:
            if (prim_expr->rand_expr.type == RAND_INT || prim_expr->rand_expr.type == RAND_RNG)
                return mk_type(INT);
            if (prim_expr->rand_expr.type == RAND_FLT)
                return mk_type(REAL);
            if (prim_expr->rand_expr.type == RAND_ARR)
            {
                assert(prim_expr->rand_expr.random_expr->value_type.kind == ARRAY);
                return *prim_expr->rand_expr.random_expr->value_type.array.array_type;
            }
        case FUNCTION_CALL:
            return prim_expr->func_call.signature->ret_type;
        case ARRAY_LIT:
            // let the semantic anaylsis pass figure out the type
            return prim_expr->array_lit.type;
        case STRUCT_INIT:
            return prim_expr->struct_init.type;
        case INT_CONSTANT:
            return mk_type(INT);
        case FLOAT_CONSTANT:
            return mk_type(REAL);
        case STRING_LITERAL:
            return mk_type(STR);
        case NULL_LITERAL:
            return mk_type(SPEC_NULL);
        default:
            assert(0 && "invalid prim expr type");
    }
}

type_t get_expression_type(const expression_t *expr)
{
    if (expr->kind == PRIM_EXPR)
        return expr->prim_expr.value_type;
    else if (expr->kind == BINOP)
        return get_binop_type(expr->binop);
    else if (expr->kind == ASSIGNMENT)
        return get_expression_type(expr->assignment.expr);
    else if (expr->kind == TERNARY_EXPR)
        return expr->ternary.true_branch->value_type;
    else
        assert(0 && "invalid expr type");
}

type_t get_type(const char* type)
{
    for (int i = 0; i < types_str.size; ++i)
    {
        if (strcmp(type, types_str.ptr[i]) == 0)
        {
            return mk_type((base_type_t)i);
        }
    }

    for (int i = 0; i < typedef_list.size; ++i)
    {
        if (strcmp(type, typedef_list.ptr[i].alias) == 0)
        {
            return typedef_list.ptr[i].type;
        }
    }

    return mk_type(INVALID_TYPE);
}

void add_typedef(const char* alias, type_t real_type)
{
    DYNARRAY_ADD(typedef_list, {alias, real_type});
}

int can_implicit_cast(const type_t* from, const type_t* to)
{
    if (cmp_types(from, to))
        return 1; // trivial

    if (from->kind != BASIC || to->kind != BASIC)
        return 0;

    if (from->base_type >= DEFAULT_TYPES_END || to->base_type >= DEFAULT_TYPES_END) // user types
        return 0;

    return cast_matrix[from->base_type][to->base_type] >= 2;
}

int can_explicit_cast(const type_t* from, const type_t* to)
{
    if (cmp_types(from, to))
        return 1; // trivial

    if (from->kind != BASIC || to->kind != BASIC)
        return 0;

    if (from->base_type >= DEFAULT_TYPES_END || to->base_type >= DEFAULT_TYPES_END) // user types
        return 0;

    return cast_matrix[from->base_type][to->base_type] >= 1;
}

type_t forward_declare_structure(const char* name)
{
    structure_t dummy;
    dummy.incomplete = 1;

    DYNARRAY_ADD(types_str, name);
    DYNARRAY_ADD(defined_structures, dummy);
    defined_structures.ptr[defined_structures.size-1].incomplete = 0;

    type_t new_type;
    new_type.base_type = DEFAULT_TYPES_END + defined_structures.size-1;
    return new_type; // structure id + default type offset
}

void define_structure(type_t *type, const structure_t *structure)
{
    structure_t* actual_data = get_struct(type);
    *actual_data = *structure;
    actual_data->incomplete = 0;
}
