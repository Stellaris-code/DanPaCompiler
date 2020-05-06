/*
semantic_pass.c

Copyright (c) 26 Yann BOUCHER (yann)

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

#include "semantic_pass.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "error.h"
#include "builtin.h"

#define AST_PASS_NAME semanal
#include "ast_functions.h"

        static int in_function;
static int nest_depth;
static int loop_depth;
static function_t* current_function = NULL;
static program_t*  current_program  = NULL;

// FIXME : at the end of a compound statement, mark outlived variables as invalid
static local_variable_t* find_local(ident_t* ident, int* id)
{
    if (current_function == NULL)
        return NULL;

    for (int i = 0; i < current_function->locals.size; ++i)
    {
        local_variable_t* item = &current_function->locals.ptr[i];
        if (!item->temp && item->nest_depth <= nest_depth && strcmp(item->ident.name->data.str, ident->name->data.str) == 0) // FIXME : wtf it won't work idiot
        {
            *id = i;
            return item;
        }
    }
    return NULL;
}

static global_variable_t* find_global(ident_t* ident, int* id)
{
    for (int i = 0; i < current_program->globals.size; ++i)
    {
        global_variable_t* item = &current_program->globals.ptr[i];
        if (strcmp(item->ident.name->data.str, ident->name->data.str) == 0)
        {
            *id = i;
            return item;
        }
    }
    return NULL;
}

// creates a temporary local variable
static local_variable_t* create_temporary(type_t type)
{
    local_variable_t param_local;
    param_local.ident.type = type;
    param_local.ident.flags = 0;
    param_local.ident.local_id = current_function->locals.size;
    param_local.nest_depth = nest_depth;
    param_local.temp = 1;
    DYNARRAY_ADD(current_function->locals, param_local);

    return &current_function->locals.ptr[current_function->locals.size-1];
}

static function_t* find_function(ident_t* ident)
{
    for (int i = 0; i < current_program->function_list.size; ++i)
    {
        function_t* item = &current_program->function_list.ptr[i];
        // operator overloads shouldn't be explicitely called
        if (!item->is_operator_overload && strcmp(item->name->data.str, ident->name->data.str) == 0)
        {
            return item;
        }
    }
    return NULL;
}

static void cast_to_boolean(source_location_t loc, int length, expression_t* in)
{
    if (in->value_type.kind == BASIC && in->value_type.kind == INT)
        return; // no cast needed

    if ((in->value_type.kind == POINTER || in->value_type.kind == OPTIONAL || in->value_type.kind == FUNCTION)
        || (in->value_type.kind == BASIC && in->value_type.kind == REAL))
    {
        expression_t* new_expression = (expression_t*)danpa_alloc(sizeof(expression_t));
        *new_expression = *in; // copy current expression

        // reuse 'in' to store the new cast expression
        in->kind = PRIM_EXPR;
        in->value_type = mk_type(INT); // booleans are secretly ints, don't tell anyone else shhhhhh
        primary_expression_t* cast_prim_expr = &in->prim_expr;
        primary_expression_t* target_prim_exp = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
        cast_prim_expr->type = CAST_EXPRESSION;
        cast_prim_expr->cast_expr.target_type = mk_type(INT);
        cast_prim_expr->cast_expr.expr = target_prim_exp;
        cast_prim_expr->value_type = mk_type(INT);

        target_prim_exp->type = ENCLOSED;
        target_prim_exp->expr = new_expression;
        target_prim_exp->value_type = get_expression_type(new_expression);
    }
    else
    {
        error(loc, length,
              "cannot cast '%s' to a boolean expression\n", type_to_str(&in->value_type));
    }
}

static void generate_type_conversion(source_location_t loc, int length, expression_t* in, const type_t* target)
{
    // set the value to null
    if (target->kind == OPTIONAL && in->value_type.kind == BASIC && in->value_type.base_type == SPEC_NULL)
        return;

    if (!cmp_types(&in->value_type, target))
    {
        // implicitely cast to optional
        if (target->kind == OPTIONAL)
        {
            target = target->opt.opt_type;
            if (cmp_types(&in->value_type, target))
                return;
        }

        if (!can_implicit_cast(&in->value_type, target))
        {
            error(loc, length,
                  "cannot implicitly cast '%s' to '%s'\n", type_to_str(&in->value_type), type_to_str(target));
        }

        expression_t* new_expression = (expression_t*)danpa_alloc(sizeof(expression_t));
        *new_expression = *in; // copy current expression

        // reuse 'in' to store the new cast expression
        in->kind = PRIM_EXPR;
        in->value_type = *target;
        primary_expression_t* cast_prim_expr = &in->prim_expr;
        primary_expression_t* target_prim_exp = (primary_expression_t*)danpa_alloc(sizeof(primary_expression_t));
        cast_prim_expr->type = CAST_EXPRESSION;
        cast_prim_expr->cast_expr.target_type = *target;
        cast_prim_expr->cast_expr.expr = target_prim_exp;
        cast_prim_expr->value_type = *target;

        target_prim_exp->type = ENCLOSED;
        target_prim_exp->expr = new_expression;
        target_prim_exp->value_type = get_expression_type(new_expression);
    }
}

// atoms
static void semanal_ident(ident_t* ident)
{
    if (ident->flags & IDENT_PROCESSED) // semantic analysis has already been done on this identifier
        return;

    int id;
    local_variable_t* local;
    global_variable_t* global;
    if ((local = find_local(ident, &id)))
    {
        ident->type = local->ident.type;
        ident->flags = 0;
        ident->local_id = id;
    }
    else if ((global = find_global(ident, &id)))
    {
        ident->type = global->ident.type;
        ident->flags = IDENT_GLOBAL;
        ident->global_id = id;
    }
    else
    {
        error(ident->name->location, ident->name->length, "unknown identifier '%s'\n", ident->name->data.str);
    }
}
static void semanal_int_constant(token_t* val)
{
}
static void semanal_float_constant(token_t* val)
{
}
static void semanal_string_literal(token_t* name)
{
}

AST_PROGRAM()
{
    current_program = arg_program;
    DYNARRAY_INIT(current_program->globals, 32);
    AST_PROGRAM_PROCESS_1();
    AST_PROGRAM_PROCESS_2();
}

AST_FUNCTION()
{
    in_function = 1;
    current_function = arg_function;
    DYNARRAY_INIT(current_function->locals, 16);

    // Declare the parameters as local variables
    for (int i = 0; i < arg_function->args.size; ++i)
    {
        parameter_t param = arg_function->args.ptr[i];
        local_variable_t param_local;
        param_local.ident.name = param.name;
        param_local.ident.type = param.type;
        param_local.nest_depth = 0;
        param_local.temp = 0;

        DYNARRAY_ADD(current_function->locals, param_local);
    }

    AST_FUNCTION_PROCESS();

    in_function = 0;
}

AST_TYPE()
{

    AST_TYPE_PROCESS();
    if (arg_type->kind == ARRAY)
    {
        type_t int_type = mk_type(INT);
        if (!can_implicit_cast(&arg_type->array.initial_size->value_type, &int_type))
        {
            error(arg_type->array.initial_size->loc, arg_type->array.initial_size->length, "invalid array size type : %s\n", type_to_str(&arg_type->array.initial_size->value_type)); // FIX
        }
    }

}

AST_RETURN_STATEMENT()
{
    AST_RETURN_STATEMENT_PROCESS();
    if (!arg_return_statement->empty_return)
        generate_type_conversion(arg_return_statement->expr.loc, arg_return_statement->expr.length, &arg_return_statement->expr, &current_function->signature.ret_type);
    else
    {
        type_t void_type = mk_type(VOID);
        if (!cmp_types(&current_function->signature.ret_type, &void_type))
            error(arg_return_statement->return_token->location, arg_return_statement->return_token->length, "function return type is not void");
    }
}

AST_ASSIGNMENT()
{
    if (!is_lvalue(&arg_assignment->var))
        error(arg_assignment->expr->loc, arg_assignment->expr->length, "Assigned expression is not an lvalue\n");

    AST_ASSIGNMENT_PROCESS_1();
    AST_ASSIGNMENT_PROCESS_2();

    type_t assign_type = arg_assignment->var.value_type;
    generate_type_conversion(arg_assignment->expr->loc,
                             arg_assignment->expr->length,
                             arg_assignment->expr, &assign_type);
}

AST_IF_STATEMENT()
{
    AST_IF_STATEMENT_PROCESS_1();
    AST_IF_STATEMENT_PROCESS_2();
    AST_IF_STATEMENT_PROCESS_3();

    cast_to_boolean(arg_if_statement->test.loc, arg_if_statement->test.length, &arg_if_statement->test);
}

AST_WHILE_STATEMENT()
{
    ++loop_depth;
    AST_WHILE_STATEMENT_PROCESS_1();
    AST_WHILE_STATEMENT_PROCESS_2();
    --loop_depth;

    cast_to_boolean(arg_while_statement->test.loc, arg_while_statement->test.length, &arg_while_statement->test);
}

AST_DO_WHILE_STATEMENT()
{
    ++loop_depth;
    AST_DO_WHILE_STATEMENT_PROCESS_1();
    AST_DO_WHILE_STATEMENT_PROCESS_2();
    --loop_depth;

    cast_to_boolean(arg_do_while_statement->test.loc, arg_do_while_statement->test.length, &arg_do_while_statement->test);
}

AST_FOR_STATEMENT()
{
    ++loop_depth;
    ++nest_depth;
    AST_FOR_STATEMENT_PROCESS_INIT();
    AST_FOR_STATEMENT_PROCESS_TEST();
    AST_FOR_STATEMENT_PROCESS_LOOP();

    AST_FOR_STATEMENT_PROCESS_BODY();
    --nest_depth;
    --loop_depth;

    cast_to_boolean(arg_for_statement->test.loc, arg_for_statement->test.length, &arg_for_statement->test);
}

AST_FOREACH_STATEMENT()
{
    ++loop_depth;
    ++nest_depth;

    AST_FOREACH_STATEMENT_PROCESS_ARRAY();

    if (arg_foreach_statement->array_expr.value_type.kind != ARRAY &&
        !(arg_foreach_statement->array_expr.value_type.kind == BASIC && arg_foreach_statement->array_expr.value_type.base_type == STR))
        error(arg_foreach_statement->array_expr.loc, arg_foreach_statement->array_expr.length,
              "cannot use foreach on a non-array type\n");
    if (arg_foreach_statement->loop_var_type != NULL)
        arg_foreach_statement->loop_ident.type = *arg_foreach_statement->loop_var_type;
    else
    {
        if (arg_foreach_statement->array_expr.value_type.kind == ARRAY)
            arg_foreach_statement->loop_ident.type = *arg_foreach_statement->array_expr.value_type.array.array_type;
        else // STR
            arg_foreach_statement->loop_ident.type = mk_type(INT);

        if (arg_foreach_statement->foreach_ref)
        {
            type_t* pointed_type = danpa_alloc(sizeof(type_t));
            *pointed_type = arg_foreach_statement->loop_ident.type;

            arg_foreach_statement->loop_ident.type.kind = POINTER;
            arg_foreach_statement->loop_ident.type.pointer.pointed_type = pointed_type;
        }
    }


    arg_foreach_statement->loop_ident.local_id = current_function->locals.size-1;
    arg_foreach_statement->loop_ident.flags = 0;

    arg_foreach_statement->loop_var_decl.name = arg_foreach_statement->loop_ident.name;
    arg_foreach_statement->loop_var_decl.type = arg_foreach_statement->loop_ident.type;
    arg_foreach_statement->loop_var_decl.init_assignment = NULL;
    semanal_variable_declaration(&arg_foreach_statement->loop_var_decl);

    // add the counter variable declaration
    arg_foreach_statement->counter_var_id = create_temporary(mk_type(INT))->ident.local_id;

    // define the loop variable assignment
    assignment_t* assign = &arg_foreach_statement->loop_var_assignment;
    assign->var.type = IDENT;
    assign->var.ident = arg_foreach_statement->loop_ident;

    assign->expr = danpa_alloc(sizeof(expression_t));
    assign->expr->flags = 0;
    assign->expr->kind = PRIM_EXPR;
    assign->expr->loc = arg_foreach_statement->loop_ident.name->location;
    assign->expr->length = arg_foreach_statement->loop_ident.name->length;

    assign->expr->prim_expr.type = ARRAY_SUBSCRIPT;
    assign->expr->prim_expr.array_sub.array_expr = danpa_alloc(sizeof(primary_expression_t));
    assign->expr->prim_expr.array_sub.array_expr->type = ENCLOSED;
    assign->expr->prim_expr.array_sub.array_expr->expr = &arg_foreach_statement->array_expr;
    assign->expr->prim_expr.array_sub.subscript_expr = danpa_alloc(sizeof(expression_t));
    assign->expr->prim_expr.array_sub.subscript_expr->kind = PRIM_EXPR;
    assign->expr->prim_expr.array_sub.subscript_expr->flags = 0;
    assign->expr->prim_expr.array_sub.subscript_expr->value_type = mk_type(INT);

    assign->expr->prim_expr.array_sub.subscript_expr->prim_expr.type = IDENT;
    assign->expr->prim_expr.array_sub.subscript_expr->prim_expr.ident.flags = IDENT_PROCESSED; // don't run semantic analysis on it !
    assign->expr->prim_expr.array_sub.subscript_expr->prim_expr.ident.local_id = arg_foreach_statement->counter_var_id;
    assign->expr->prim_expr.array_sub.subscript_expr->prim_expr.ident.name = NULL;
    assign->expr->prim_expr.value_type = assign->expr->prim_expr.value_type;

    // if foreach ref, take the address
    if (arg_foreach_statement->foreach_ref)
    {
        primary_expression_t* refered_expr = danpa_alloc(sizeof(primary_expression_t));
        *refered_expr = assign->expr->prim_expr;

        assign->expr->prim_expr.type = ADDR_GET;
        assign->expr->prim_expr.addr.addressed_function = NULL;
        assign->expr->prim_expr.addr.addr_expr = refered_expr;
        assign->expr->prim_expr.addr.addr_token = arg_foreach_statement->loop_ident.name;
        assign->expr->prim_expr.value_type = assign->expr->prim_expr.value_type;
    }

    assign->expr->value_type = get_expression_type(assign->expr);

    assign->discard_result = 1;
    semanal_assignment(assign);

    AST_FOREACH_STATEMENT_PROCESS_IDENT();

    AST_FOREACH_STATEMENT_PROCESS_BODY();
    --nest_depth;
    --loop_depth;
}

AST_COMPOUND_STATEMENT()
{
    ++nest_depth;
    AST_COMPOUND_STATEMENT_PROCESS();
    --nest_depth;
}

AST_ASM_EXPR()
{
    AST_ASM_EXPR_PROCESS();
}

AST_NEW_EXPR()
{
    AST_NEW_EXPR_PROCESS();
    if (sizeof_type(&arg_new_expr->new_type) == (size_t)-1)
    {
        error(arg_new_expr->loc, arg_new_expr->length, "allocation of incomplete type %s\n", type_to_str(&arg_new_expr->new_type));
    }
}


AST_SIZEOF_EXPR()
{
    AST_SIZEOF_EXPR_PROCESS();
    if (arg_sizeof_expr->is_expr)
        arg_sizeof_expr->type = arg_sizeof_expr->expr->value_type;
    if (sizeof_type(&arg_sizeof_expr->type) == (size_t)-1)
    {
        error(arg_sizeof_expr->loc, arg_sizeof_expr->length, "sizeof of incomplete type %s\n", type_to_str(&arg_sizeof_expr->expr->value_type));
    }
}

AST_STATEMENT()
{
    AST_STATEMENT_PROCESS();
}

AST_LOOP_CTRL_STATEMENT()
{
    if (loop_depth == 0)
        error(arg_loop_ctrl_statement->tok->location, arg_loop_ctrl_statement->tok->length, "loop control statement cannot be use outside of a loop\n");
    AST_LOOP_CTRL_STATEMENT();
}

AST_TYPEDEF_DECLARATION()
{
    AST_TYPEDEF_DECLARATION_PROCESS();
}

AST_VARIABLE_DECLARATION()
{
    if (in_function)
    {
        local_variable_t local;
        local.ident.name = arg_variable_declaration->name;
        local.ident.type = arg_variable_declaration->type;
        local.nest_depth = nest_depth;
        local.temp = 0;
        DYNARRAY_ADD(current_function->locals, local);
        arg_variable_declaration->var_id = current_function->locals.size-1;
        arg_variable_declaration->global = 0;
    }
    else
    {
        global_variable_t global;
        global.ident.name = arg_variable_declaration->name;
        global.ident.type = arg_variable_declaration->type;
        DYNARRAY_ADD(current_program->globals, global);
        arg_variable_declaration->var_id = current_program->globals.size-1;
        arg_variable_declaration->global = 1;
    }

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

    // if an overload was found, ignore the type checking process
    if (find_binop_overload(arg_binop->op->data.op, &arg_binop->left.value_type, &arg_binop->right.value_type))
        return;

    type_t* left_target_type = &arg_binop->left.value_type;
    type_t* right_target_type = &arg_binop->left.value_type;
    type_t flt_type = mk_type(REAL);
    type_t int_type = mk_type(INT);
    // array cat
    if (arg_binop->op->data.op == OP_CAT && left_target_type->kind == ARRAY)
    {
        right_target_type = left_target_type->array.array_type;
    }
    else if (arg_binop->op->data.op == OP_CAT && left_target_type->kind == BASIC && left_target_type->base_type == STR)
    {
        right_target_type = &int_type;
    }
    // 'in' operator
    else if (arg_binop->op->data.op == OP_IN && arg_binop->right.value_type.kind == ARRAY && cmp_types(arg_binop->right.value_type.array.array_type, left_target_type))
    {
        // no type conversion here, all is well
        return;
    }
    // logic operators : cast argument to pseudp 'boolean' type
    else if (operators[arg_binop->op->data.op].is_logic)
    {
        left_target_type = right_target_type = &int_type;
        cast_to_boolean(arg_binop->left.loc, arg_binop->left.length, &arg_binop->left);
        cast_to_boolean(arg_binop->right.loc, arg_binop->right.length, &arg_binop->right);
    }
    // if one of them is real, convert to real
    else if (cmp_types(&arg_binop->left.value_type, &flt_type) || cmp_types(&arg_binop->left.value_type, &flt_type))
    {
        // convert all to real
        left_target_type = right_target_type = &flt_type;
    }
    // error !
    else if (!can_implicit_cast(&arg_binop->left.value_type, &arg_binop->right.value_type))
    {
        // I forgot why this exists
        /*
        // TODO : have a better error reporting system capable of treating this kind of case automatically
        while (arg_binop->right.loc.macro_invok_type == MACRO_ARG_TOKEN)
        {
            arg_binop->right.length = arg_binop->right.loc.macro_invok_token->length;
            arg_binop->right.loc = arg_binop->right.loc.macro_invok_token->location;
        }
        while (arg_binop->left.loc.macro_invok_type == MACRO_ARG_TOKEN)
        {
            arg_binop->left.length = arg_binop->left.loc.macro_invok_token->length;
            arg_binop->left.loc = arg_binop->left.loc.macro_invok_token->location;
        }
*/
        error(arg_binop->left.loc, (arg_binop->right.loc.ptr + arg_binop->right.length) - arg_binop->left.loc.ptr,
              "cannot do '%s' with '%s' and '%s'\n", operators[arg_binop->op->data.op].str,
              type_to_str(&arg_binop->left.value_type), type_to_str(&arg_binop->right.value_type));
    }

    if (arg_binop->left.value_type.kind == POINTER ||
        arg_binop->right.value_type.kind == POINTER)
    {
        if (arg_binop->op->data.op != OP_EQUAL && arg_binop->op->data.op != OP_DIFF)
            error(arg_binop->left.loc, (arg_binop->right.length + arg_binop->right.loc.ptr) - arg_binop->left.loc.ptr, "pointers can only be compared for equality\n");
    }

    // cannot do non-overloaded binops on struct types
    if (is_struct(&arg_binop->left.value_type) || is_struct(&arg_binop->right.value_type))
    {
        error(arg_binop->left.loc, (arg_binop->right.loc.ptr + arg_binop->right.length) - arg_binop->left.loc.ptr,
              "cannot do '%s' with '%s' and '%s'\n", operators[arg_binop->op->data.op].str,
              type_to_str(&arg_binop->left.value_type), type_to_str(&arg_binop->right.value_type));
    }

    generate_type_conversion(arg_binop->left.loc, (arg_binop->right.length + arg_binop->right.loc.ptr) - arg_binop->left.loc.ptr, &arg_binop->left, left_target_type);
    generate_type_conversion(arg_binop->left.loc, (arg_binop->right.length + arg_binop->right.loc.ptr) - arg_binop->left.loc.ptr, &arg_binop->right, right_target_type);
}

AST_FUNC_CALL_EXPRESSION()
{
    function_t* func;
    builtin_t*  builtin;
    arg_function_call->builtin = NULL;

    if (arg_function_call->call_expr->type == IDENT &&
        (func = find_function(&arg_function_call->call_expr->ident)))
    {
        arg_function_call->indirect = 0;
        arg_function_call->signature = &func->signature;
    }
    else if (arg_function_call->call_expr->type == IDENT &&
             (builtin = find_builtin(arg_function_call->call_expr->ident.name->data.str)))
    {
        arg_function_call->indirect = 0;
        arg_function_call->builtin = builtin;
        arg_function_call->signature = &arg_function_call->builtin->signature;
    }
    else
    {
        AST_FUNC_CALL_EXPRESSION_PROCESS_1(); // do semanal on the call expr

        arg_function_call->indirect = 1;
        type_t call_expr_type = arg_function_call->call_expr->value_type;
        if (call_expr_type.kind != FUNCTION)
            error(arg_function_call->call_expr->loc, arg_function_call->call_expr->length, "expr is not a function\n");
        arg_function_call->signature = call_expr_type.function.signature;
    }

    type_t signature_type;
    signature_type.kind = FUNCTION;
    signature_type.function.signature = arg_function_call->signature;

    if (arg_function_call->arguments.size != arg_function_call->signature->parameter_types.size)
        error(arg_function_call->call_expr->loc, arg_function_call->call_expr->length, "invalid parameter count for : expected %d, got %d (function signature is %s)\n",
              arg_function_call->signature->parameter_types.size, arg_function_call->arguments.size, type_to_str(&signature_type));

    AST_FUNC_CALL_EXPRESSION_PROCESS_2();

    for (int i = 0; i < arg_function_call->signature->parameter_types.size; ++i)
    {
        generate_type_conversion(arg_function_call->arguments.ptr[i]->loc, arg_function_call->arguments.ptr[i]->length,
                                 arg_function_call->arguments.ptr[i], &arg_function_call->signature->parameter_types.ptr[i]);
    }
}

AST_ARRAY_LIT_EXPR()
{
    AST_ARRAY_LIT_EXPR_PROCESS();

    if (arg_array_lit_expr->elements.size)
    {
        type_t first_type = arg_array_lit_expr->elements.ptr[0].value_type;
        for (int i = 1; i < arg_array_lit_expr->elements.size; ++i)
        {
            if (!cmp_types(&first_type, &arg_array_lit_expr->elements.ptr[i].value_type))
            {
                error(arg_array_lit_expr->elements.ptr[i].loc, arg_array_lit_expr->elements.ptr[i].length,
                      "invalid array expression type : got <%s>, expected <%n>\n", type_to_str(&arg_array_lit_expr->elements.ptr[i].value_type), type_to_str(&first_type));
            }
        }
    }

    type_t array_type;
    array_type.kind = ARRAY;
    array_type.array.array_type = (type_t*)danpa_alloc(sizeof(type_t));
    if (arg_array_lit_expr->elements.size)
        *array_type.array.array_type = arg_array_lit_expr->elements.ptr[0].value_type;
    else
        *array_type.array.array_type = mk_type(INVALID_TYPE);

    array_type.array.initial_size = (expression_t*)danpa_alloc(sizeof(expression_t));
    array_type.array.initial_size->flags = 0;
    array_type.array.initial_size->kind = PRIM_EXPR;
    array_type.array.initial_size->prim_expr.type = INT_CONSTANT;
    array_type.array.initial_size->prim_expr.int_constant = (token_t*)danpa_alloc(sizeof(token_t));
    array_type.array.initial_size->prim_expr.int_constant->data.integer = arg_array_lit_expr->elements.size;

    arg_array_lit_expr->type = array_type;
}

AST_ARRAY_SUBSCRIPT()
{
    type_t str_type = mk_type(STR);

    AST_ARRAY_SUBSCRIPT_PROCESS_1();
    type_t array_type = arg_array_subscript->array_expr->value_type;
    if (array_type.kind != ARRAY && cmp_types(&str_type, &array_type) == 0)
        error(arg_array_subscript->array_expr->loc, arg_array_subscript->array_expr->length,
              "type '%s' is not an array or a string\n", type_to_str(&array_type));
    AST_ARRAY_SUBSCRIPT_PROCESS_2();
}


AST_ARRAY_SLICE()
{
    type_t str_type = mk_type(STR);
    type_t int_type = mk_type(INT);

    AST_ARRAY_SLICE_PROCESS_1();
    type_t array_type = arg_array_slice->array_expr->value_type;
    if (array_type.kind != ARRAY && cmp_types(&str_type, &array_type) == 0)
        error(arg_array_slice->array_expr->loc, arg_array_slice->array_expr->length,
              "type '%s' is not an array\n", type_to_str(&array_type));

    AST_ARRAY_SLICE_PROCESS_2();
    AST_ARRAY_SLICE_PROCESS_3();

    if (!cmp_types(&arg_array_slice->left_expr->value_type, &int_type))
    {
        error(arg_array_slice->left_expr->loc, arg_array_slice->left_expr->length,
              "left slice bound isn't an int\n");
    }
    if (!cmp_types(&arg_array_slice->right_expr->value_type, &int_type))
    {
        error(arg_array_slice->right_expr->loc, arg_array_slice->right_expr->length,
              "right slice bound isn't an int\n");
    }
}

AST_ARRAY_RANGE_EXPR()
{
    AST_ARRAY_RANGE_EXPR_PROCESS_1();
    AST_ARRAY_RANGE_EXPR_PROCESS_2();

    type_t int_type = mk_type(INT);
    if (!cmp_types(&int_type, &arg_array_range_expr->left_bound->value_type))
    {
        error(arg_array_range_expr->left_bound->loc, arg_array_range_expr->left_bound->length, "array range bound must be int");
    }
    if (!cmp_types(&int_type, &arg_array_range_expr->right_bound->value_type))
    {
        error(arg_array_range_expr->right_bound->loc, arg_array_range_expr->right_bound->length, "array range bound must be int");
    }
}

AST_DEREF_EXPR()
{
    AST_DEREF_EXPR_PROCESS();
    type_t type = arg_deref_expr->pointer_expr->value_type;
    arg_deref_expr->is_optional_access = type.kind == OPTIONAL;
    if (type.kind != POINTER && type.kind != OPTIONAL)
    {
        error(arg_deref_expr->pointer_expr->loc, arg_deref_expr->pointer_expr->length, "type %s is not a pointer nor an optional\n", type_to_str(&type));
    }
}

AST_ADDR_EXPR()
{
    function_t* func;
    if (arg_addr_expr->addr_expr->type == IDENT && (func = find_function(&arg_addr_expr->addr_expr->ident)))
    {
        // special case, addressed in the code generator : take the address of a function
        arg_addr_expr->addressed_function = func;
    }
    else
    {
        arg_addr_expr->addressed_function = NULL;

        AST_ADDR_EXPR_PROCESS();

        if (!is_lvalue(arg_addr_expr->addr_expr))
        {
            error(arg_addr_expr->addr_expr->loc, arg_addr_expr->addr_expr->length, "Cannot get the address of a rvalue\n");
        }
    }
}

AST_RAND_EXPR()
{
    AST_RAND_EXPR_PROCESS();
    if (!arg_random_expr->is_range)
    {
        type_t expr_type = arg_random_expr->random_expr->value_type;
        if (expr_type.kind == ARRAY)
        {
            arg_random_expr->type = RAND_ARR;
        }
        else if (expr_type.kind == BASIC && expr_type.base_type == INT)
        {
            arg_random_expr->type = RAND_INT;
        }
        else if (expr_type.kind == BASIC && expr_type.base_type == REAL)
        {
            arg_random_expr->type = RAND_FLT;
        }
        else
        {
            error(arg_random_expr->random_expr->loc, arg_random_expr->random_expr->length, "invalid random expression type <%s>\n", type_to_str(&expr_type));
        }
    }
    else
    {
        type_t left_type = arg_random_expr->left_bound->value_type;
        type_t right_type = arg_random_expr->right_bound->value_type;
        type_t int_type = mk_type(INT);
        if (!cmp_types(&left_type, &int_type))
            error(arg_random_expr->left_bound->loc, arg_random_expr->left_bound->length, "expected integer type, got <%s>\n", type_to_str(&left_type));
        if (!cmp_types(&right_type, &int_type))
            error(arg_random_expr->right_bound->loc, arg_random_expr->right_bound->length, "expected integer type, got <%s>\n", type_to_str(&right_type));

        arg_random_expr->type = RAND_RNG;
    }
}

AST_MATCH_PATTERN()
{
    switch (arg_match_pattern->type)
    {
        case PAT_INT_LIT:
            semanal_int_constant(arg_match_pattern->int_constant);
            arg_match_pattern->value_type = mk_type(INT);
            break;
        case PAT_STR_LIT:
            semanal_string_literal(arg_match_pattern->string_lit);
            arg_match_pattern->value_type = mk_type(STR);
            break;
        case PAT_IDENT:
            semanal_ident(&arg_match_pattern->ident);
            arg_match_pattern->value_type = arg_match_pattern->ident.type;
            break;
        case PAT_RANGE:
            semanal_int_constant(arg_match_pattern->left_bound);
            semanal_int_constant(arg_match_pattern->right_bound);
            arg_match_pattern->value_type = mk_type(INT);
            break;
    }
}

AST_MATCH_CASE()
{
    for (int i = 0; i < arg_match_case->patterns.size; ++i)
        semanal_match_pattern(&arg_match_case->patterns.ptr[i]);

    // check types
    for (int i = 1; i < arg_match_case->patterns.size; ++i)
    {
        if (!cmp_types(&arg_match_case->patterns.ptr[0].value_type, &arg_match_case->patterns.ptr[i].value_type))
        {
            error(arg_match_case->patterns.ptr[i].loc, arg_match_case->patterns.ptr[i].length, "pattern types don't match\n");
        }
    }

    semanal_expression(arg_match_case->expr);

    arg_match_case->value_type = arg_match_case->patterns.ptr[0].value_type;
}

AST_MATCH_EXPR()
{
    semanal_expression(arg_match_expr->tested_expr);

    int wildcard_count = 0;

    for (int i = 0; i < arg_match_expr->cases.size; ++i)
    {
        semanal_match_case(&arg_match_expr->cases.ptr[i]);
        if (arg_match_expr->cases.ptr[i].is_wildcard)
            ++wildcard_count;
        if (wildcard_count > 1)
        {
            error(arg_match_expr->cases.ptr[i].loc, arg_match_expr->cases.ptr[i].length, "cannot have multiple wildcard cases in match expression\n");
        }

        if (!cmp_types(&arg_match_expr->cases.ptr[i].value_type, &arg_match_expr->tested_expr->value_type))
        {
            error(arg_match_expr->cases.ptr[i].loc, arg_match_expr->cases.ptr[i].length, "match case type incompatible with tested expression\n");
        }
    }

    for (int i = 1; i < arg_match_expr->cases.size; ++i)
    {
        if (!cmp_types(&arg_match_expr->cases.ptr[0].expr->value_type, &arg_match_expr->cases.ptr[i].expr->value_type))
        {
            error(arg_match_expr->cases.ptr[i].expr->loc, arg_match_expr->cases.ptr[i].expr->length, "match case expressions don't have the same type\n");
        }
    }

    if (arg_match_expr->tested_expr->kind == PRIM_EXPR && arg_match_expr->tested_expr->prim_expr.type == IDENT &&
        !(arg_match_expr->tested_expr->prim_expr.ident.flags & IDENT_GLOBAL))
    {
        // reuse the input local variable as the test variable
        arg_match_expr->test_expr_loc_id = arg_match_expr->tested_expr->prim_expr.ident.local_id;
    }
    else
    {
        // create local variable to store the tested expression
        arg_match_expr->test_expr_loc_id = create_temporary(arg_match_expr->tested_expr->value_type)->ident.local_id;;
    }
    for (int i = 0; i < arg_match_expr->cases.size; ++i)
    {
        arg_match_expr->cases.ptr[i].test_expr_loc_id = arg_match_expr->test_expr_loc_id;
    }
}

AST_NULL_EXPRESSION()
{

}

AST_STRUCT_ACCESS()
{
    AST_STRUCT_ACCESS_PROCESS();
    type_t expr_type = arg_struct_access->struct_expr->value_type;
    if (arg_struct_access->indirect_access)
    {
        if (expr_type.kind != POINTER && expr_type.kind != OPTIONAL)
        {
            error(arg_struct_access->struct_expr->loc, arg_struct_access->struct_expr->length, "type %s is not a pointer nor an optional\n", type_to_str(&expr_type));
        }
        if (expr_type.kind == POINTER)
            expr_type = *expr_type.pointer.pointed_type;
        if (expr_type.kind == OPTIONAL)
            expr_type = *expr_type.opt.opt_type;
    }

    if (!is_struct(&expr_type))
    {
        error(arg_struct_access->struct_expr->loc, arg_struct_access->struct_expr->length, "type %s is not a struct\n", type_to_str(&expr_type));
    }
    const structure_t* struct_type = get_struct(&expr_type);
    if (struct_type->incomplete)
    {
        error(arg_struct_access->struct_expr->loc, arg_struct_access->struct_expr->length, "type %s is incomplete\n", type_to_str(&expr_type));
    }
    structure_field_t* field = NULL;

    for (int i = 0; i < struct_type->fields.size; ++i)
    {
        if (strcmp(arg_struct_access->field_name->data.str, struct_type->fields.ptr[i].name->data.str) == 0)
            field = &struct_type->fields.ptr[i];
    }
    if (field == NULL)
    {
        error(arg_struct_access->field_name->location, arg_struct_access->field_name->length, "type %s has no field named %s\n", type_to_str(&expr_type), arg_struct_access->field_name->data.str);
    }

    arg_struct_access->field = field;
    arg_struct_access->value_type = field->type;
}

AST_STRUCT_INIT_EXPR()
{
    assert(is_struct(&arg_struct_initializer->type));
    const structure_t* strct = get_struct(&arg_struct_initializer->type);

    AST_STRUCT_INIT_EXPR_PROCESS();

    if (strct->fields.size != arg_struct_initializer->elements.size)
        error(arg_struct_initializer->loc, arg_struct_initializer->length, "Invalid initialize count, expected %d, got %d\n",
              strct->fields.size, arg_struct_initializer->elements.size);
}

AST_UNARY_EXPRESSION()
{
    type_t int_type = mk_type(INT);
    type_t flt_type = mk_type(REAL);

    AST_UNARY_EXPRESSION_PROCESS();
    type_t expr_type = arg_unary_expr->unary_value->value_type;
    if (arg_unary_expr->unary_op->type == TOK_QUESTION)
    {
        if (expr_type.kind != OPTIONAL)
            error(arg_unary_expr->unary_value->loc, arg_unary_expr->unary_value->length, "optional test type must be an optional type\n");
        return;
    }
    if (find_unop_overload(arg_unary_expr->unary_op->data.op, &expr_type))
        return; // bail; let the overload take care of it


    if (operators[arg_unary_expr->unary_op->data.op].is_bool)
    {
        expression_t* expr = danpa_alloc(sizeof(expression_t));
        expr->flags = 0; expr->kind = PRIM_EXPR;
        expr->value_type = arg_unary_expr->unary_value->value_type;
        expr->prim_expr = *arg_unary_expr->unary_value;

        cast_to_boolean(arg_unary_expr->unary_value->loc, arg_unary_expr->unary_value->length, expr);

        arg_unary_expr->unary_value->type = ENCLOSED;
        arg_unary_expr->unary_value->expr = expr;
    }
    else if (!cmp_types(&expr_type, &int_type) && !cmp_types(&expr_type, &flt_type))
        error(arg_unary_expr->unary_value->loc, arg_unary_expr->unary_value->length, "unary expr type must be int or float\n");
}

AST_CAST_EXPRESSION()
{
    AST_CAST_EXPRESSION_PROCESS();

    type_t from   = arg_cast_expression->expr->value_type;
    type_t to     = arg_cast_expression->target_type;
    if (!can_explicit_cast(&from, &to))
    {
        error(arg_cast_expression->cast_type_token->location,
              (arg_cast_expression->expr->loc.ptr + arg_cast_expression->expr->length) - arg_cast_expression->cast_type_token->location.ptr,
              "cannot cast '%s' to '%s'\n", type_to_str(&from), type_to_str(&to));
    }
}

AST_TERNARY_EXPRESSION()
{
    AST_TERNARY_EXPRESSION_PROCESS_1();
    AST_TERNARY_EXPRESSION_PROCESS_2();
    AST_TERNARY_EXPRESSION_PROCESS_3();

    cast_to_boolean(arg_ternary_expr->cond_expr->loc, arg_ternary_expr->cond_expr->length, arg_ternary_expr->cond_expr);

    type_t true_expr_type = arg_ternary_expr->true_branch->value_type;
    generate_type_conversion(arg_ternary_expr->false_branch->loc, arg_ternary_expr->false_branch->length, arg_ternary_expr->false_branch, &true_expr_type);
}

AST_PRIM_EXPRESSION()
{
    AST_PRIM_EXPRESSION_PROCESS();
    arg_primary_expression->value_type = get_prim_expr_type(arg_primary_expression);
}

AST_EXPRESSION()
{
    AST_EXPRESSION_PROCESS();

    arg_expression->value_type = get_expression_type(arg_expression);
    arg_expression->flags = 0;
    if (arg_expression->kind == BINOP && operators[arg_expression->binop->op->data.op].is_bool)
    {
        arg_expression->flags |= IS_BOOL_EXPR;
    }
    else if (arg_expression->kind == PRIM_EXPR && arg_expression->prim_expr.type == UNARY_OP_FACTOR &&
             arg_expression->prim_expr.unary_expr.unary_op->type == TOK_OPERATOR &&
            operators[arg_expression->prim_expr.unary_expr.unary_op->data.op].is_bool)
    {
        arg_expression->flags |= IS_BOOL_EXPR;
    }
}
