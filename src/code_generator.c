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
#include "error.h"
#include "builtin.h"

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

#define AST_PASS_NAME generate
#include "ast_functions.h"

#define LABEL_MAX_LEN 16

    static label_list_t loop_entry_labels; // no need for dynarray_init if it is declared as global, auto zero'ed
static label_list_t loop_exit_labels;

    FILE* out_stream;

    program_t* current_program = NULL;
instruction_t* current_instruction = NULL;
label_list_t next_instruction_labels;
char* next_instruction_comments;

    const char binop_opcodes[POD_TYPES_END][OP_BIN_END][8] =
        {
            // INT
            {"add",     "sub",  "mul",  "idiv",  "mod", "invl", "inlv",  "eq", "neq",  "gt",  "ge",  "lt",  "le",  "land",   "lor",  "and", "or", "xor", "shl", "shr"},
            // FLOAT
            {"fadd",    "fsub", "fmul", "fdiv", "fmod", "invl", "invl", "feq","fneq", "fgt", "fge", "flt", "fle", "invl", "invl", "invl", "invl", "invl", "invl", "invl"},
            // STR
            {"strcat",  "invl", "invl", "invl", "invl", "invl", "strcat", "streq", "strneq", "strgt", "strge", "strlt", "strle", "invl", "invl", "invl", "invl", "invl", "invl", "invl"},
            };

    const char unary_opcodes[POD_TYPES_END][OP_UNARY_END - OP_BIN_END][8] =
        {
            // INT
            {"inc",  "dec",  "lnot",  "not"},
            // FLOAT
            {"inc",  "dec",  "invl",  "invl"},
            // STR
            {"invl", "invl", "invl",  "invl"},
            };

    const char cast_opcodes[POD_TYPES_END][POD_TYPES_END][8] =
        {
            // INT
            {"invl",  "cvti2f",  "cvti2s"},
            // FLOAT
            {"cvtf2i",  "invl",  "cvtf2s"},
            // STR
            {"invl", "invl", "invl"},
            };

void generate(const char* op, const char* operand, ...)
{
    assert(strcmp(op, "invl") != 0); // invalid instruction generated

    char* buffer = danpa_alloc(128);

    va_list args;
    va_start(args, operand);

    vsnprintf(buffer, 128, operand, args);

    va_end(args);

    instruction_t* next = danpa_alloc(sizeof(instruction_t));
    DYNARRAY_INIT(next->labels, next_instruction_labels.size);
    DYNARRAY_RESIZE(next->labels, next_instruction_labels.size);
    memcpy(next->labels.ptr, next_instruction_labels.ptr, next_instruction_labels.size*sizeof(const char*));

    next->opcode = op;
    next->operand = buffer;
    next->comment = next_instruction_comments;
    next->next = NULL;
    next->prev = current_instruction;

    if (current_instruction)
        current_instruction->next = next;
    else // first pushed instruction, mark as beginning of the instruction list
        instruction_list = next;
    current_instruction = next;

    DYNARRAY_RESIZE(next_instruction_labels, 0);
    next_instruction_comments = NULL;
}

static inline void generate_jump_target(const char* label)
{
    DYNARRAY_ADD(next_instruction_labels, label);
}

static inline void add_comment(const char* fmt, ...)
{
    next_instruction_comments = danpa_alloc(256);

    va_list args;
    va_start(args, fmt);

    vsnprintf(next_instruction_comments, 256, fmt, args);

    va_end(args);
}

static inline void generate_array_size(const type_t* array_type)
{
    if (array_type->kind == ARRAY)
    {
        generate_expression(array_type->array.initial_size);
        array_type = array_type->array.array_type;
    }

    while (array_type->kind == ARRAY)
    {
        generate_expression(array_type->array.initial_size);
        generate("mul","");

        array_type = array_type->array.array_type;
    }
    if (sizeof_type(array_type) > 1)
    {
        generate("pushi","#%d", sizeof_type(array_type));
        generate("mul","");
    }
}


static void handle_discarded_expression(expression_t* expr)
{
    type_t void_type = mk_type(VOID);
    type_t prim_expr_type = expr->prim_expr.value_type;

    if (cmp_types(&prim_expr_type, &void_type))
    {
        // exprs of type void don't return anything
    }
    else if (expr->kind == ASSIGNMENT)
    {
        // assignment automatically discards the new lvalue if used in an expression statement
    }
    else
    {
        generate("pop",""); // discard pushed expression
    }
}

static int label_counter;
static int addr_calc_state;

void enter_addr_calc()
{
    ++addr_calc_state;
}

int pop_addr_calc_state()
{
    if (addr_calc_state == 0)
        return 0;
    else
    {
        --addr_calc_state;
        return 1;
    }
}

static inline void generate_label(char* buf)
{
    snprintf(buf, LABEL_MAX_LEN, ".L%x", label_counter++);
}
// atoms
static void generate_ident(ident_t* ident)
{
    if (ident->name)
        add_comment("// %s", ident->name->data.str);
    if ((ident->flags & IDENT_GLOBAL))
        generate("pushg", "%d", ident->global_id);
    else
        generate("pushl", "%d", ident->local_id);
}
static void generate_int_constant(token_t* val)
{
    generate("pushi", "#%d", val->data.integer);
}
static void generate_float_constant(token_t* val)
{
    generate("pushf", "#%f", val->data.fp);
}
static void generate_string_literal(token_t* val)
{
    hash_value_t* node;

    if (strlen(val->data.str) < 64) // show short strings as a comment in the asm output
    {
        add_comment("// \"%s\"", val->data.str);
    }

    // if the string already exists in the string table, reuse it
    if ((node = hash_table_get(&current_program->strings, val->data.str)))
    {
        generate("pushs", "%d", node->idx);
    }
    else
    {
        int idx = current_program->strings.count;
        hash_table_insert(&current_program->strings, val->data.str, (hash_value_t){.idx = idx});
        generate("pushs", "%d", idx);
    }
}

// replaces all " with \"
const char* stringify(const char* str)
{
    const char* start = str;
    char* result = danpa_alloc(strlen(str)*2); // str can only be at most 2 times larger
    char* ptr = result;
    while (*str)
    {
        if (*str == '"' && (str == start || str[-1] != '\\'))
        {
            *ptr++ = '\\';
            *ptr++ = '"';
            ++str;
        }
        else
            *ptr++ = *str++;
    }
    *ptr++ = '\0';

    return result;
}

void generate_strings_callback(hash_node_t* node)
{
    generate(".string", "%d, \"%s\"", node->value.idx, stringify(node->key));
}

AST_PROGRAM()
{
    current_program = arg_program;

    generate_jump_target("_global_init");
    AST_PROGRAM_PROCESS_1(); // global init stuff
    generate("jmp", "main");
    AST_PROGRAM_PROCESS_2();

    hash_table_iterate(&current_program->strings, generate_strings_callback);
}

AST_FUNCTION()
{
    generate_jump_target(arg_function->name->data.str);

    // fetch the parameters on the stack into the matching local variables
    // in reverse order : stack is right-to-left, local variables are left-to-right
    for (int i = arg_function->args.size - 1; i >= 0; --i)
    {
        add_comment("// get '%s'", arg_function->args.ptr[i].name->data.str);
        generate("movl", "%d", i);
        if (is_struct(&arg_function->args.ptr[i].type))
        {
            // copy the structure
            // dest
            generate("pushi", "#%d", sizeof_type(&arg_function->args.ptr[i].type));
            generate("alloc", "");
            generate("dup", "");

            // src
            generate("pushl", "%d", i);

            // size
            generate("pushi", "#%d", sizeof_type(&arg_function->args.ptr[i].type));

            generate("copy","");
            generate("movl", "%d", i);
        }
    }

    AST_FUNCTION_PROCESS();

    generate("ret", "");
}

AST_TYPE()
{

}

AST_RETURN_STATEMENT()
{
    AST_RETURN_STATEMENT_PROCESS();
    generate("ret", "");
}

AST_ASSIGNMENT()
{
    type_t str_type = mk_type(STR);

    // special case for strings
    if (arg_assignment->var.type == ARRAY_SUBSCRIPT)
    {
        type_t array_type = arg_assignment->var.array_sub.array_expr->value_type;
        if (cmp_types(&array_type, &str_type))
        {
            generate_primary_expression(arg_assignment->var.array_sub.array_expr); // string
            generate_expression(arg_assignment->var.array_sub.subscript_expr); // index
            generate("add","");
            AST_ASSIGNMENT_PROCESS_2(); // value
            generate("store","");

            goto out;
        }
    }
    if (arg_assignment->var.type != IDENT)
    {
        int old_addr_val = addr_calc_state;
        enter_addr_calc();
        AST_ASSIGNMENT_PROCESS_1();
        assert(addr_calc_state == old_addr_val);
    }

    type_t assign_type = arg_assignment->var.value_type;
    if (arg_assignment->var.type == IDENT)
    {
        add_comment("// %s = *sp", arg_assignment->var.ident.name->data.str);
        if (is_struct(&assign_type)) // copy the struct data
        {
            if (arg_assignment->var.ident.flags & IDENT_GLOBAL)
                generate("pushg", "%d", arg_assignment->var.ident.global_id);
            else
                generate("pushl", "%d", arg_assignment->var.ident.local_id);

            int old_addr_val = addr_calc_state;
            enter_addr_calc();
            AST_ASSIGNMENT_PROCESS_2();
            addr_calc_state = old_addr_val;
            //assert(addr_calc_state == old_addr_val);

            generate("pushi", "#%d", sizeof_type(&assign_type));
            generate("copy","");
        }
        else // the easy part
        {
            AST_ASSIGNMENT_PROCESS_2();

            if (arg_assignment->var.ident.flags & IDENT_GLOBAL)
                generate("movg", "%d", arg_assignment->var.ident.global_id);
            else
                generate("movl", "%d", arg_assignment->var.ident.local_id);
        }

    }
    else
    {
        AST_ASSIGNMENT_PROCESS_2();

        if (!is_struct(&assign_type)) // the easy part
        {
            generate("store","");
        }
        else
        {
            generate("pushi", "#%d", sizeof_type(&assign_type));
            generate("copy","");
        }
    }

out:
    if (!arg_assignment->discard_result)
    {
        AST_ASSIGNMENT_PROCESS_1(); // return the assigned lvalue
    }
}

AST_IF_STATEMENT()
{
    char* else_label = danpa_alloc(LABEL_MAX_LEN);
    generate_label(else_label);

    AST_IF_STATEMENT_PROCESS_1();
    add_comment("// if");
    generate("jf", "%s", else_label);

    AST_IF_STATEMENT_PROCESS_2();
    if (arg_if_statement->else_statement)
    {
        char* out_label  = danpa_alloc(LABEL_MAX_LEN);
        generate_label(out_label );

        generate("jmp", "%s", out_label);

        generate_jump_target(else_label);
        AST_IF_STATEMENT_PROCESS_3();

        generate_jump_target(out_label);
    }
    else // don't generate code for an else part
    {
        generate_jump_target(else_label);
    }
}

AST_WHILE_STATEMENT()
{
    char* out_label = danpa_alloc(LABEL_MAX_LEN);
    char* loop_label = danpa_alloc(LABEL_MAX_LEN);
    generate_label(out_label);
    generate_label(loop_label);
    DYNARRAY_ADD(loop_entry_labels, loop_label);
    DYNARRAY_ADD(loop_exit_labels, out_label);

    generate_jump_target(loop_label);

    AST_WHILE_STATEMENT_PROCESS_1();
    generate("jf", "%s", out_label);

    AST_WHILE_STATEMENT_PROCESS_2();
    generate("jmp", "%s", loop_label);

    generate_jump_target(out_label);
    DYNARRAY_POP(loop_entry_labels);
    DYNARRAY_POP(loop_exit_labels);
}

AST_FOR_STATEMENT()
{
    char* out_label = danpa_alloc(LABEL_MAX_LEN);
    char* loop_label = danpa_alloc(LABEL_MAX_LEN);
    generate_label(out_label);
    generate_label(loop_label);
    DYNARRAY_ADD(loop_entry_labels, loop_label);
    DYNARRAY_ADD(loop_exit_labels, out_label);

    AST_FOR_STATEMENT_PROCESS_INIT();

    generate_jump_target(loop_label);
    AST_FOR_STATEMENT_PROCESS_TEST();
    generate("jf", "%s", out_label);

    AST_FOR_STATEMENT_PROCESS_BODY();
    AST_FOR_STATEMENT_PROCESS_LOOP();
    handle_discarded_expression(&arg_for_statement->loop_expr);

    generate("jmp", "%s", loop_label);

    generate_jump_target(out_label);
    DYNARRAY_POP(loop_entry_labels);
    DYNARRAY_POP(loop_exit_labels);
}

AST_FOREACH_STATEMENT()
{
    char* out_label = danpa_alloc(LABEL_MAX_LEN);
    char* loop_label = danpa_alloc(LABEL_MAX_LEN);
    generate_label(out_label);
    generate_label(loop_label);
    DYNARRAY_ADD(loop_entry_labels, loop_label);
    DYNARRAY_ADD(loop_exit_labels, out_label);

    // loop ident init
    generate_variable_declaration(&arg_foreach_statement->loop_var_decl);

    // counter init
    generate("pushi", "#0");
    generate("movl","%d", arg_foreach_statement->counter_var_id);
    // counter test
    generate_jump_target(loop_label);
    generate("pushl","%d", arg_foreach_statement->counter_var_id);
    AST_FOREACH_STATEMENT_PROCESS_ARRAY();
    if (arg_foreach_statement->array_expr.value_type.kind == ARRAY) // not a string
    {
        generate("memsize","");
        const size_t sizeof_array_type = sizeof_type(arg_foreach_statement->array_expr.value_type.array.array_type);
        if (sizeof_array_type > 1)
        {
            generate("pushi","#%d", sizeof_array_type);
            generate("idiv","");
        }
    }
    else // STR
        generate("strlen","");

    generate("lt",""); // counter < array.size
    generate("jf", "%s", out_label);

    // foreach body
    generate_assignment(&arg_foreach_statement->loop_var_assignment);
    AST_FOREACH_STATEMENT_PROCESS_BODY();
    // counter increment
    generate("incl","%d", arg_foreach_statement->counter_var_id);
    generate("jmp", "%s", loop_label);

    generate_jump_target(out_label);
    DYNARRAY_POP(loop_entry_labels);
    DYNARRAY_POP(loop_exit_labels);
}

AST_DO_WHILE_STATEMENT()
{
    char* loop_label = danpa_alloc(LABEL_MAX_LEN);
    char* out_label  = danpa_alloc(LABEL_MAX_LEN);
    generate_label(loop_label);
    generate_label(out_label);
    DYNARRAY_ADD(loop_entry_labels, loop_label);
    DYNARRAY_ADD(loop_exit_labels, out_label);

    generate_jump_target(loop_label);

    AST_DO_WHILE_STATEMENT_PROCESS_1();
    AST_DO_WHILE_STATEMENT_PROCESS_2();

    generate("jt", "%s", loop_label);
    generate_jump_target(out_label);
    DYNARRAY_POP(loop_entry_labels);
    DYNARRAY_POP(loop_exit_labels);
}

AST_LOOP_CTRL_STATEMENT()
{
    if (arg_loop_ctrl_statement->type == LOOP_BREAK)
    {
        generate("jmp", "%s", DYNARRAY_BACK(loop_exit_labels));
    }
    else if (arg_loop_ctrl_statement->type == LOOP_CONTINUE)
    {
        generate("jmp", "%s", DYNARRAY_BACK(loop_entry_labels));
    }
    AST_LOOP_CTRL_STATEMENT();
}

AST_COMPOUND_STATEMENT()
{
    AST_COMPOUND_STATEMENT_PROCESS();
}

AST_ASM_EXPR()
{
    AST_ASM_EXPR_PROCESS();

    generate("", "%s", arg_asm_expr->asm_code);
}

AST_NEW_EXPR()
{
    AST_NEW_EXPR_PROCESS();

    generate("pushi","#%d", sizeof_type(&arg_new_expr->new_type));
    generate("alloc","");
}


AST_MATCH_PATTERN()
{
    type_t str_type = mk_type(STR);
    type_t int_type = mk_type(INT);

    switch (arg_match_pattern->type)
    {
        case PAT_INT_LIT:
            generate_int_constant(arg_match_pattern->int_constant);
            generate("eq","");
            break;
        case PAT_STR_LIT:
            generate_string_literal(arg_match_pattern->string_lit);
            generate("streq","");
            break;
        case PAT_IDENT:
            generate_ident(&arg_match_pattern->ident);
            if (cmp_types(&arg_match_pattern->ident.type, &str_type))
                generate("streq","");
            else if (cmp_types(&arg_match_pattern->ident.type, &int_type))
                generate("eq","");
            else
                error(arg_match_pattern->loc, arg_match_pattern->length, "invalid match ident type : %s\n", type_to_str(&arg_match_pattern->ident.type));
            break;
        case PAT_RANGE:
            generate("dup","");
            generate_int_constant(arg_match_pattern->left_bound);
            generate("ge","");
            generate_int_constant(arg_match_pattern->right_bound);
            generate("le","");
            generate("logicand","");
            break;
    }
}

AST_MATCH_CASE()
{
    for (int i = 0; i < arg_match_case->patterns.size; ++i)
    {
        generate("pushl","%d", arg_match_case->test_expr_loc_id);
        generate_match_pattern(&arg_match_case->patterns.ptr[i]);
        if (i != 0)
        {
            generate("logicand","");
        }
    }
}

AST_MATCH_EXPR()
{
    char* out_label = danpa_alloc(LABEL_MAX_LEN);
    generate_label(out_label);

    generate_expression(arg_match_expr->tested_expr);
    generate("movl","%d", arg_match_expr->test_expr_loc_id);

    for (int i = 0; i < arg_match_expr->cases.size; ++i)
    {
        char* next_label = danpa_alloc(LABEL_MAX_LEN);
        generate_label(next_label);

        if (!arg_match_expr->cases.ptr[i].is_wildcard)
        {
            generate_match_case(&arg_match_expr->cases.ptr[i]);
            generate("jf", "%s", next_label);
        }

        generate_expression(arg_match_expr->cases.ptr[i].expr);
        generate("jmp", "%s", out_label);

        generate_jump_target(next_label);
    }

    generate_jump_target(out_label);
}

AST_RAND_EXPR()
{
    if (arg_random_expr->is_range)
    {
        generate_primary_expression(arg_random_expr->right_bound);
        generate_primary_expression(arg_random_expr->left_bound);
        generate("sub",""); // get range size
        generate("randi","");
        generate_primary_expression(arg_random_expr->left_bound);
        generate("add","");
    }
    else
    {
        AST_RAND_EXPR_PROCESS();
        switch (arg_random_expr->type)
        {
            default:
            case RAND_INT:
                generate("randi",""); break;
            case RAND_FLT:
                generate("randf",""); break;
            case RAND_ARR:
                generate("randa",""); break;
        }
    }
}

// FIXME : right now it assigns the structs by reference and not by value!
AST_ARRAY_LIT_EXPR()
{
    generate("pushi","#%d", arg_array_lit_expr->elements.size);
    generate("alloc","");
    generate("dup","");
    AST_ARRAY_LIT_EXPR_PROCESS();
    generate("stackcpy","#%d", arg_array_lit_expr->elements.size);
}

AST_ARRAY_RANGE_EXPR()
{
    AST_ARRAY_RANGE_EXPR_PROCESS_1();
    AST_ARRAY_RANGE_EXPR_PROCESS_2();
    generate("mkrange","");
}

AST_STRUCT_INIT_EXPR()
{
    generate("pushi", "#%d", sizeof_type(&arg_struct_initializer->type));
    generate("alloc","");

    unsigned cum_offset = 0;
    for (int i = 0; i < arg_struct_initializer->elements.size; ++i)
    {
        expression_t* field = &arg_struct_initializer->elements.ptr[i];
        generate("dup","");
        generate("pushi", "#%d", cum_offset);
        generate("add","");

        generate_expression(field);

        if (!is_struct(&field->value_type)) // the easy part
        {
            generate("store","");
        }
        else
        {
            generate("pushi", "#%d", sizeof_type(&field->value_type));
            generate("copy","");
        }

        cum_offset += sizeof_type(&field->value_type);
    }
}

AST_NULL_EXPRESSION()
{
    generate("pushnull","");
}

AST_STATEMENT()
{
    AST_STATEMENT_PROCESS();
    if (arg_statement->type == DISCARDED_EXPRESSION)
    {
        handle_discarded_expression(&arg_statement->expression);
    }
}

AST_TYPEDEF_DECLARATION()
{
    AST_TYPEDEF_DECLARATION_PROCESS();
}

AST_VARIABLE_DECLARATION()
{
    int initialized = 0;
    // allocate struct space, and mark as managed
    if (is_struct(&arg_variable_declaration->type))
    {
        generate("pushi", "#%d", sizeof_type(&arg_variable_declaration->type));
        generate("alloc","");
        initialized = 1;
    }
    else if (arg_variable_declaration->type.kind == ARRAY)
    {
        //if (!arg_variable_declaration->type.array.is_empty)
        {
            // calculate the total multidimensionnal array size
            generate_array_size(&arg_variable_declaration->type);

            add_comment("// %s", type_to_str(&arg_variable_declaration->type));
            generate("alloc","");
            initialized = 1;
        }
    }
    // null initialization
    else if (arg_variable_declaration->init_assignment == NULL &&
             (arg_variable_declaration->type.kind == OPTIONAL
              || arg_variable_declaration->type.kind == POINTER
              || arg_variable_declaration->type.kind == FUNCTION))
    {
        generate("pushnull","");
        initialized = 1;
    }

    if (initialized)
    {
        if (arg_variable_declaration->global)
            generate("movg", "%d", arg_variable_declaration->var_id); // movgmngd
        else
            generate("movl", "%d", arg_variable_declaration->var_id); // movlmngd
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

    op_overload_t* overload;

    // array element cat
    if (arg_binop->left.value_type.kind == ARRAY && arg_binop->right.value_type.kind == BASIC)
    {
        generate("arraycat","");
    }
    // string + char
    else if (arg_binop->left.value_type.kind == BASIC && arg_binop->left.value_type.base_type == STR
             && arg_binop->right.value_type.kind == BASIC && arg_binop->right.value_type.base_type == INT)
    {
        assert(arg_binop->op->data.op == OP_CAT);
        generate("stradd","");
    }
    else if (arg_binop->op->data.op == OP_IN)
    {
        if (is_indirect_type(&arg_binop->left.value_type))
        {
            generate("pushi", "#%d", sizeof_type(&arg_binop->left.value_type)); // comparison size
            generate("findi","");
        }
        else
            generate("find","");
        // finds return '-1' if the element wasn't found
        generate("inc",""); // '0' if not found, 'nonzero' if found
    }
    else if ((overload = find_binop_overload(arg_binop->op->data.op, &arg_binop->left.value_type, &arg_binop->right.value_type)))
    {
        // call the overload
        generate("call", overload->mangled_name);
    }
    else if (arg_binop->left.value_type.kind == POINTER || arg_binop->right.value_type.kind == POINTER)
        generate(binop_opcodes[INT][arg_binop->op->data.op], "");
    else
        generate(binop_opcodes[arg_binop->left.value_type.base_type][arg_binop->op->data.op], "");
}

AST_FUNC_CALL_EXPRESSION()
{
    if (arg_function_call->builtin)
    {
        arg_function_call->builtin->generate(arg_function_call->arguments);
    }
    else if (arg_function_call->indirect == 0)
    {
        AST_FUNC_CALL_EXPRESSION_PROCESS_2();

        generate("call", "%s", arg_function_call->call_expr->ident.name->data.str);
    }
    else
    {
        // arguments first
        AST_FUNC_CALL_EXPRESSION_PROCESS_2();
        // then call pointer
        AST_FUNC_CALL_EXPRESSION_PROCESS_1();

        generate("calli","");
    }
}

AST_ARRAY_SUBSCRIPT()
{
    int addr_calc_mode = pop_addr_calc_state();

    //enter_addr_calc();
    AST_ARRAY_SUBSCRIPT_PROCESS_1(); // calculate array address

    // calculate array subscript
    if (arg_array_subscript->array_expr->value_type.kind == ARRAY)
    {
        type_t* array_type = arg_array_subscript->array_expr->value_type.array.array_type;
        generate_expression(arg_array_subscript->subscript_expr);

        if (array_type->kind == ARRAY)
        {
            generate_array_size(array_type);
            add_comment("// %s", type_to_str(array_type));
            generate("mul", "");
        }
        else if (sizeof_type(array_type) > 1)
        {
            generate("pushi","#%d", sizeof_type(array_type));
            add_comment("// sizeof(%s)", type_to_str(array_type));
            generate("mul", "");
        }
        generate("add", "");

        if (!addr_calc_mode)
            generate("load", "");
    }
    else // string
    {
        generate_expression(arg_array_subscript->subscript_expr);

        generate("add", "");

        if (!addr_calc_mode)
            generate("load", "");
    }
}


AST_ARRAY_SLICE()
{
    AST_ARRAY_SLICE_PROCESS_1();
    AST_ARRAY_SLICE_PROCESS_2();
    AST_ARRAY_SLICE_PROCESS_3();

    generate("getslice","");
}

AST_STRUCT_ACCESS()
{
    int addr_calc_mode = pop_addr_calc_state();
    int saved_addr_mode_state = addr_calc_state;
    if (!arg_struct_access->indirect_access)
    {
        enter_addr_calc();
    }
    AST_STRUCT_ACCESS_PROCESS();
    addr_calc_state = saved_addr_mode_state;

    type_t struct_type = arg_struct_access->struct_expr->value_type;
    if (arg_struct_access->indirect_access && struct_type.kind == OPTIONAL)
    {
        generate("chknotnul","");
    }

    add_comment("// %s", arg_struct_access->field_name->data.str);
    generate("pushi", "#%d", arg_struct_access->field->offset);
    generate("add","");

    if (!addr_calc_mode)
    {
        add_comment("// %s", arg_struct_access->field_name->data.str);
        generate("load","");
    }
}

AST_DEREF_EXPR()
{
    int addr_calc_mode = pop_addr_calc_state();

    AST_DEREF_EXPR_PROCESS();
    if (arg_deref_expr->is_optional_access)
    {
        // generate code to check that the value isn't null
        generate("chknotnul","");
    }
    else
        if (!addr_calc_mode)
        generate("load","");
}

AST_ADDR_EXPR()
{
    // taking the address of a function ?
    if (arg_addr_expr->addressed_function)
    {
        generate("pushi","%s", arg_addr_expr->addressed_function->name->data.str);
    }
    // treat direct variables as a special case
    else if (arg_addr_expr->addr_expr->type == IDENT)
    {
        add_comment("// %s",  arg_addr_expr->addr_expr->ident.name->data.str);
        if (is_struct(&arg_addr_expr->addr_expr->ident.type)) // struct variables already hold a pointer
        {
            if (arg_addr_expr->addr_expr->ident.flags & IDENT_GLOBAL)
                generate("pushg", "%d", arg_addr_expr->addr_expr->ident.global_id);
            else
                generate("pushl", "%d", arg_addr_expr->addr_expr->ident.local_id);
        }
        else // POD variable
        {
            if (arg_addr_expr->addr_expr->ident.flags & IDENT_GLOBAL)
                generate("getaddrg", "%d", arg_addr_expr->addr_expr->ident.global_id);
            else
                generate("getaddrl", "%d", arg_addr_expr->addr_expr->ident.local_id);
        }
    }
    else
    {
        int old_addr_val = addr_calc_state;
        enter_addr_calc();
        AST_ADDR_EXPR_PROCESS();
        assert(addr_calc_state == old_addr_val);
    }
}

AST_SIZEOF_EXPR()
{
    add_comment("// sizeof(%s)", type_to_str(&arg_sizeof_expr->type));
    generate("pushi", "#%d", sizeof_type(&arg_sizeof_expr->type));
}

AST_UNARY_EXPRESSION()
{
    AST_UNARY_EXPRESSION_PROCESS();

    type_t type = arg_unary_expr->unary_value->value_type;
    op_overload_t* overload;

    if (type.kind == POINTER)
    {
        assert(operators[arg_unary_expr->unary_op->data.op].category == OPC_UNARY);
        generate(unary_opcodes[INT][arg_unary_expr->unary_op->data.op - OP_BIN_END],"");
    }
    else if (type.kind == OPTIONAL)
    {
        assert(arg_unary_expr->unary_op->type == TOK_QUESTION);
        generate("isnull","");
        generate("lnot","");
    }
    else if ((overload = find_unop_overload(arg_unary_expr->unary_op->data.op, &type)))
    {
        // call the overload
        generate("call", overload->mangled_name);
    }
    else
    {
        assert(type.kind == BASIC);
        type_t i = mk_type(INT);
        type_t f = mk_type(REAL);
        switch (arg_unary_expr->unary_op->data.op)
        {
            case OP_ADD:
                break; // litteraly nothing to do
            case OP_SUB:
                if (cmp_types(&type, &i))
                    generate("neg","");
                else if (cmp_types(&type, &f))
                    generate("fneg","");
                else
                    assert(0 && "invalid type");
                break;
            default:
                assert(operators[arg_unary_expr->unary_op->data.op].category == OPC_UNARY);
                generate(unary_opcodes[type.base_type][arg_unary_expr->unary_op->data.op - OP_BIN_END],"");
                break;
        }
    }
}

AST_CAST_EXPRESSION()
{
    AST_CAST_EXPRESSION_PROCESS();

    type_t from = arg_cast_expression->expr->value_type;
    type_t to = arg_cast_expression->target_type;
    if (from.kind == POINTER || from.kind == OPTIONAL || from.kind == FUNCTION)
    {
        generate("isnull","");
        generate("lnot","");
    }
    else
    {
        assert(from.kind == BASIC);
        assert(to.kind == BASIC);

        generate(cast_opcodes[from.base_type][to.base_type],"");
    }
}

AST_TERNARY_EXPRESSION()
{
    AST_TERNARY_EXPRESSION_PROCESS_1();
    AST_TERNARY_EXPRESSION_PROCESS_2();
    AST_TERNARY_EXPRESSION_PROCESS_3();
    generate("cmov","");
}

AST_PRIM_EXPRESSION()
{
    AST_PRIM_EXPRESSION_PROCESS();


}

AST_EXPRESSION()
{
    AST_EXPRESSION_PROCESS();
}
