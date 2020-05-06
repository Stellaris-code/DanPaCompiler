#include "operators.h"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include "types.h"
#include "ast_nodes.h"
#include "error.h"

    operator_t operators[OP_ENUM_END] =
        {
          // str  stra  prec  bool  logic category
            {"+", "add", 30, false, false, OPC_BINARY},
            {"-", "sub", 30, false, false, OPC_BINARY},
            {"*", "mul", 40, false, false, OPC_BINARY},
            {"/", "div", 40, false, false, OPC_BINARY},
            {"%", "mod", 40, false, false, OPC_BINARY},
            {"in","in" , 22, true,  false, OPC_BINARY},
            {"~", "not", 30, false, false, OPC_BINARY},
            {"==", "eq", 10, true, false , OPC_BINARY},
            {"!=", "neq", 10, true, false, OPC_BINARY},
            {">", "gt", 20, true, false,   OPC_BINARY},
            {">=", "ge", 20, true, false,  OPC_BINARY},
            {"<", "lt", 20, true, false,   OPC_BINARY},
            {"<=", "le", 20, true, false,  OPC_BINARY},
            {"&&", "andl", 3, true, true,  OPC_BINARY},
            {"||", "orl", 2, true, true,   OPC_BINARY},
            {"&", "and", 6, false, false,  OPC_BINARY},
            {"|", "or", 4, false, false,   OPC_BINARY},
            {"^", "xor", 5, false, false,  OPC_BINARY},
            {"<<", "shl", 25, false, false,OPC_BINARY},
            {">>", "shr", 25, false, false,OPC_BINARY},
            {"++", "inc", 0, false, false, OPC_UNARY},
            {"--", "dec", 0, false, false, OPC_UNARY},
            {"!", "notl", 0, true, true,   OPC_UNARY},
            } ;

static DYNARRAY(op_overload_t) overloads;

int eval_int_binop(operator_type_t op, int x, int y)
{
    switch (op)
    {
        case OP_ADD:
            return x+y;
        case OP_SUB:
            return x-y;
        case OP_MUL:
            return x*y;
        case OP_DIV:
            return x/y;
        case OP_MOD:
            return x%y;
        case OP_EQUAL:
            return x==y;
        case OP_DIFF:
            return x!=y;
        case OP_GT:
            return x>y;
        case OP_GE:
            return x>=y;
        case OP_LT:
            return x<y;
        case OP_LE:
            return x<=y;
        case OP_LOGICAND:
            return x&&y;
        case OP_LOGICOR:
            return x||y;
        case OP_BITAND:
            return x&y;
        case OP_BITOR:
            return x|y;
        case OP_BITXOR:
            return x^y;
        case OP_SHL:
            return x<<y;
        case OP_SHR:
            return x>>y;
        default:
            assert(0 && "unknown int binop operator encountered\n");
    }
    return 0xdeadbeef;
}

int eval_int_unop(operator_type_t op, int x)
{
    switch (op)
    {
        case OP_LOGICNOT:
            return !x;
        case OP_BITNOT:
            return ~x;
        case OP_ADD:
            return +x;
        case OP_SUB:
            return -x;
        default:
            assert(0 && "invalid int unop operator encountered\n");
    }
    return 0xdeadbeef;
}

int eval_float_unop(operator_type_t op, float x)
{
    switch (op)
    {
        case OP_ADD:
            return +x;
        case OP_SUB:
            return -x;
        default:
            assert(0 && "invalid float unop operator encountered\n");
    }
    return 0xdeadbeef;
}

float eval_float_binop(operator_type_t op, float x, float y)
{
    switch (op)
    {
        case OP_ADD:
            return x+y;
        case OP_SUB:
            return x-y;
        case OP_MUL:
            return x*y;
        case OP_DIV:
            return x/y;
        case OP_EQUAL:
            return x==y;
        case OP_DIFF:
            return x!=y;
        case OP_GT:
            return x>y;
        case OP_GE:
            return x>=y;
        case OP_LT:
            return x<y;
        case OP_LE:
            return x<=y;
        default:
            assert(0 && "unknown float binop operator encountered\n");
    }
    return 0.0f;
}

void register_overload(function_t *func)
{
    assert(func->is_operator_overload);

    DYNARRAY_ADD(overloads, {});
    op_overload_t* overload = &overloads.ptr[overloads.size-1];
    overload->op = func->overloaded_op;
    overload->signature.ret_type = func->signature.ret_type;
    DYNARRAY_INIT(overload->signature.parameter_types, 2);

    // binop
    // OP_CAT is ambiguous
    if ((func->overloaded_op != OP_CAT && operators[func->overloaded_op].category == OPC_BINARY)
        || (func->overloaded_op == OP_CAT && func->signature.parameter_types.size == 2))
    {
        if (func->signature.parameter_types.size != 2)
            error(func->name->location, func->name->length, "invalid operator overload argument count\n");

        // only POD types : this is a non-overloadable operator
        if (!is_struct(&func->signature.ret_type) && !is_struct(&func->signature.parameter_types.ptr[0])
            && !is_struct(&func->signature.parameter_types.ptr[1]))
        {
            error(func->name->location, func->name->length, "can't overload operator%s with types %s, %s, %s\n", operators[func->overloaded_op].str,
                  type_to_str(&func->signature.ret_type),
                  type_to_str(&func->signature.parameter_types.ptr[0]), type_to_str(&func->signature.parameter_types.ptr[1]));
        }

        DYNARRAY_ADD(overload->signature.parameter_types, func->signature.parameter_types.ptr[0]);
        DYNARRAY_ADD(overload->signature.parameter_types, func->signature.parameter_types.ptr[1]);
        snprintf(overload->mangled_name, 256, "operatorb%s_%s_%s", operators[func->overloaded_op].str_alpha,
                 type_to_str(&func->signature.parameter_types.ptr[0]), type_to_str(&func->signature.parameter_types.ptr[1]));

        assert(find_binop_overload(func->overloaded_op, &func->signature.parameter_types.ptr[0], &func->signature.parameter_types.ptr[1]));
    }
    // unary op
    else if ((func->overloaded_op != OP_CAT && operators[func->overloaded_op].category == OPC_UNARY)
             || (func->overloaded_op == OP_CAT && func->signature.parameter_types.size == 1))
    {
        if (func->signature.parameter_types.size != 1)
            error(func->name->location, func->name->length, "invalid operator overload argument count\n");

        // only POD types : this is a non-overloadable operator
        if (!is_struct(&func->signature.ret_type) && !is_struct(&func->signature.parameter_types.ptr[0]))
        {
            error(func->name->location, func->name->length, "can't overload operator%s with types %s, %s\n", operators[func->overloaded_op].str,
                  type_to_str(&func->signature.ret_type),
                  type_to_str(&func->signature.parameter_types.ptr[0]));
        }

        DYNARRAY_ADD(overload->signature.parameter_types, func->signature.parameter_types.ptr[0]);
        snprintf(overload->mangled_name, 256, "operatoru%s_%s", operators[func->overloaded_op].str_alpha, type_to_str(&func->signature.parameter_types.ptr[0]));

        assert(find_unop_overload(func->overloaded_op, &func->signature.parameter_types.ptr[0]));
    }
    else
        assert("invalid op" && 0);

    func->name->data.str = danpa_alloc(strlen((const char*)overload->mangled_name));
    strcpy((char*)func->name->data.str, overload->mangled_name);
}

op_overload_t *find_binop_overload(operator_type_t op, const type_t *lhs_type, const type_t *rhs_type)
{
    for (int i = 0; i < overloads.size; ++i)
    {
        if (overloads.ptr[i].op == op && overloads.ptr[i].signature.parameter_types.size == 2
            && cmp_types(&overloads.ptr[i].signature.parameter_types.ptr[0], lhs_type)
            && cmp_types(&overloads.ptr[i].signature.parameter_types.ptr[1], rhs_type))
        {
            return &overloads.ptr[i];
        }
    }
    return NULL;
}

op_overload_t *find_unop_overload(operator_type_t op, const type_t *type)
{
    for (int i = 0; i < overloads.size; ++i)
    {
        if (overloads.ptr[i].op == op && overloads.ptr[i].signature.parameter_types.size == 1
            && cmp_types(&overloads.ptr[i].signature.parameter_types.ptr[0], type))
        {
            return &overloads.ptr[i];
        }
    }
    return NULL;
}
