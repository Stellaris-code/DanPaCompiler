#ifndef OPERATORS_H
#define OPERATORS_H

#include "types.h"

typedef enum operator_type_t
{
    // binary ops

    OP_ADD = 0,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_IN,
    OP_CAT, OP_BITNOT = OP_CAT, // OP_BITNOT is actually just OP_CAT as an unary op
    OP_EQUAL,
    OP_DIFF,
    OP_GT,
    OP_GE,
    OP_LT,
    OP_LE,
    OP_LOGICAND,
    OP_LOGICOR,
    OP_BITAND,
    OP_BITOR,
    OP_BITXOR,
    OP_SHL,
    OP_SHR,

    // unary ops
    OP_INC, OP_BIN_END = OP_INC,
    OP_DEC,
    OP_LOGICNOT,

    OP_ENUM_END,
    OP_UNARY_END = OP_ENUM_END,
    OP_LEFT_PARENTHESIS = OP_ENUM_END // for the shunting-yard algorithm
} operator_type_t;

typedef enum operator_category_t
{
    OPC_BINARY,
    OPC_UNARY
} operator_category_t;

typedef struct operator_t
{
    const char str[4];
    const char str_alpha[16];
    int precedence;
    int is_bool;
    int is_logic;
    operator_category_t category;
} operator_t;

extern operator_t operators[OP_ENUM_END];

typedef struct op_overload_t
{
    operator_type_t op;
    char mangled_name[256];
    function_signature_t signature;
} op_overload_t;

typedef struct function_t function_t;
typedef struct function_signature_t function_signature_t;
typedef struct type_t type_t;

void register_overload(function_t *func);
op_overload_t *find_binop_overload(operator_type_t op, const type_t* lhs_type, const type_t* rhs_type);
op_overload_t *find_unop_overload(operator_type_t op, const type_t* type);

int eval_int_binop(operator_type_t op, int x, int y);
int eval_int_unop(operator_type_t op, int x);

float eval_float_binop(operator_type_t op, float x, float y);
int eval_float_unop(operator_type_t op, float x);

#endif // OPERATORS_H
