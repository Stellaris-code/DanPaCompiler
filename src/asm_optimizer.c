#include "asm_optimizer.h"

#include <string.h>
#include <assert.h>

typedef enum opt_pass_behavior
{
    SKIP = 0,
    NEXT_INSTRUCTION = 1,
    CONTINUE = 2
} opt_pass_behavior;

static inline void remove_ins(instruction_t* ins)
{
    if (ins->next)
        assert(ins->next->prev == ins);

    ins->next->prev = ins->prev;
    ins->prev->next = ins->next;

    // shift the labels to the next instruction
    for (int i = 0; i < ins->labels.size; ++i)
    {
        DYNARRAY_ADD(ins->next->labels, ins->labels.ptr[i]);
    }
}

enum
{
    NOLABEL = (1<<0)
} next_ins_flag;

static inline instruction_t* next_ins(instruction_t* ins, int idx, int flags)
{
    while (idx--)
    {
        if (ins->next == NULL)
            return NULL;
        ins = ins->next;
    }

    if (ins->labels.size && (flags & NOLABEL))
        return NULL;
    return ins;
}


static hash_table_t label_targets;
static void find_label_targets(instruction_t* ins_list)
{
    label_targets = mk_hash_table(64);
    instruction_t* ins = ins_list;
    while (ins)
    {
        for (int i = 0; i < ins->labels.size; ++i)
            hash_table_insert(&label_targets, ins->labels.ptr[i], (hash_value_t){.ptr = ins});

        ins = ins->next;
    }
}

/*
jf .L0
pushi #4
movl 2
jmp .L1
.L0:
pushi #8
movl 2
.L1:
*/
static opt_pass_behavior peephole_cmov(instruction_t* ins)
{
    if (strcmp(ins->opcode, "jf") != 0 && strcmp(ins->opcode, "jt") != 0)
        return SKIP;

    instruction_t* ins_1, *ins_2, *ins_3, *ins_4, *ins_5, *ins_6;
    ins_1 = next_ins(ins, 1, NOLABEL);
    ins_2 = next_ins(ins, 2, NOLABEL);
    ins_3 = next_ins(ins, 3, NOLABEL);
    ins_4 = next_ins(ins, 4, 0);
    ins_5 = next_ins(ins, 5, NOLABEL);
    ins_6 = next_ins(ins, 6, 0);

    if (ins_1 == NULL || strcmp(ins_1->opcode, "pushi") != 0)
        return SKIP;
    if (ins_2 == NULL || strcmp(ins_2->opcode, "movl") != 0)
        return SKIP;
    if (ins_3 == NULL || strcmp(ins_3->opcode, "jmp") != 0)
        return SKIP;
    if (ins_4 == NULL || strcmp(ins_4->opcode, "pushi") != 0)
        return SKIP;
    if (ins_5 == NULL || strcmp(ins_5->opcode, "movl") != 0)
        return SKIP;
    if (ins_6 == NULL)
        return SKIP;
    if (strcmp(ins_2->operand, ins_5->operand) != 0)
        return SKIP;
    if (ins_4->labels.size != 1 || strcmp(ins_4->labels.ptr[0], ins->operand) != 0)
        return SKIP;
    int target_found = 0;
    for (int i = 0; i < ins_6->labels.size; ++i)
    {
        if (strcmp(ins_6->labels.ptr[i], ins_3->operand) == 0)
            target_found = 1;
    }
    if (target_found == 0)
        return SKIP;

    if (strcmp(ins->opcode, "jf") == 0)
    {
        ins->opcode = "pushi"; ins->operand = ins_1->operand; ins->comment = "// true";
        ins->next->opcode = "pushi"; ins->next->operand = ins_4->operand; ins->next->comment = "// false";
    }
    else
    {
        ins->opcode = "pushi"; ins->operand = ins_4->operand; ins->comment = "// true";
        ins->next->opcode = "pushi"; ins->next->operand = ins_1->operand; ins->next->comment = "// false";
    }
    ins->next->next->opcode = "cmov";
    remove_ins(ins_3);
    remove_ins(ins_4);
    remove_ins(ins_5);

    return CONTINUE;
}

// replace movl->pushl with copyl to avoid unecessary stack operations
static opt_pass_behavior peephole_copyl(instruction_t* ins)
{
    if (strcmp(ins->opcode, "movl") != 0)
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "pushl") != 0)
        return SKIP;
    if (strcmp(ins->operand, ins->next->operand) != 0)
        return SKIP;
    if (ins->next->labels.size) // if the second instruction is a jump target
        return SKIP;
    // if it is immediately followed by a discarding 'pop', don't bother to convert to copyl and leave it to another peephole pass
    if (ins->next->next && strcmp(ins->next->next->opcode, "pop") == 0
        && ins->next->next->labels.size == 0)
        return SKIP;

    ins->opcode = "copyl";
    remove_ins(ins->next);

    return CONTINUE;
}

static opt_pass_behavior peephole_addsub_zero(instruction_t* ins)
{
    if (strcmp(ins->opcode, "pushi") != 0 || strcmp(ins->operand, "#0") != 0)
        return SKIP;
    if (ins->next == NULL || (strcmp(ins->next->opcode, "add") != 0 && strcmp(ins->next->opcode, "sub") != 0))
        return SKIP;
    if (ins->labels.size)
        return SKIP;
    if (ins->next->labels.size) // if the second instruction is a jump target
        return SKIP;

    remove_ins(ins->next);
    remove_ins(ins);

    return NEXT_INSTRUCTION;
}

// replace movl->pushl with copyl to avoid unecessary stack operations
static opt_pass_behavior peephole_useless_copyl(instruction_t* ins)
{
    if (strcmp(ins->opcode, "copyl") != 0)
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "copyl") != 0)
        return SKIP;
    if (strcmp(ins->operand, ins->next->operand) != 0)
        return SKIP;
    if (ins->next->labels.size) // if the second instruction is a jump target
        return SKIP;

    remove_ins(ins->next);

    return CONTINUE;
}

static opt_pass_behavior peephole_useless_copyl_sandwich(instruction_t* ins)
{
    if (strcmp(ins->opcode, "copyl") != 0)
        return SKIP;
    if (ins->next == NULL || ins->next->next == NULL || strcmp(ins->next->next->opcode, "copyl") != 0)
        return SKIP;
    if (strcmp(ins->operand, ins->next->next->operand) != 0)
        return SKIP;
    if (ins->next->labels.size) // if the middle instruction is a jump target
        return SKIP;
    if (ins->next->next->labels.size) // if the third instruction is a jump target
        return SKIP;

    remove_ins(ins); // first copyl is then redundant

    return NEXT_INSTRUCTION;
}

static opt_pass_behavior peephole_pushi_copyl(instruction_t* ins)
{
    if (strcmp(ins->opcode, "pushi") != 0)
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "movl") != 0)
        return SKIP;
    if (ins->next->next == NULL || strcmp(ins->next->next->opcode, "pushi") != 0)
        return SKIP;
    if (strcmp(ins->operand, ins->next->next->operand) != 0)
        return SKIP;
    if (ins->next->next->labels.size) // if the third instruction is a jump target
        return SKIP;

    ins->next->opcode = "copyl";
    remove_ins(ins->next->next);

    return CONTINUE;
}

static opt_pass_behavior peephole_pushpop(instruction_t* ins)
{
    if (strncmp(ins->opcode, "push", 4) != 0)
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "pop") != 0
        || ins->next->labels.size)
        return SKIP;

    remove_ins(ins->next);
    remove_ins(ins);

    return NEXT_INSTRUCTION;
}

// replace '== 0' with '!'
static opt_pass_behavior peephole_logicnot(instruction_t* ins)
{
    if (strcmp(ins->opcode, "pushi") != 0 || strcmp(ins->operand, "#0") != 0)
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "eq") != 0)
        return SKIP;
    if (ins->next->labels.size) // if the second instruction is a jump target
        return SKIP;

    ins->opcode = "lnot";
    ins->operand = "";
    remove_ins(ins->next);

    return CONTINUE;
}

static opt_pass_behavior peephole_logicnot_chain(instruction_t* ins)
{
    instruction_t* first_ins = ins;

    if (strcmp(first_ins->opcode, "lnot") != 0)
        return SKIP;
    int acc = 1;
    instruction_t* next_ins = ins->next;
    while (1)
    {
        ins = next_ins;

        if (ins == NULL || strcmp(ins->opcode, "lnot") != 0)
            break;

        next_ins = ins->next;
        remove_ins(ins);
        ++acc;
    }

    if ((acc % 2) == 0) // even amount of lnot's <=> no negation
    {
        remove_ins(first_ins);
        return NEXT_INSTRUCTION;
    }
    else
    {
        return CONTINUE;
    }
}

static opt_pass_behavior peephole_normalize_logic(instruction_t* ins)
{
    const char* test_op;
    const char* opposite_op;

    test_op = ins->opcode;
    if (strcmp(ins->opcode, "neq") == 0)
    {
        opposite_op = "eq";
    }
    else if (strcmp(ins->opcode, "fneq") == 0)
    {
        opposite_op = "feq";
    }
    else if (strcmp(ins->opcode, "strneq") == 0)
    {
        opposite_op = "streq";
    }
    else
        return SKIP;

    if (ins->next == NULL)
        return SKIP;
    if (ins->next->labels.size) // if the second instruction is a jump target
        return SKIP;

    if (strcmp(ins->next->opcode, "jf") == 0)
    {
        ins->opcode = opposite_op;
        ins->next->opcode = "jt";
    }
    else if (strcmp(ins->next->opcode, "jt") == 0)
    {
        ins->opcode = opposite_op;
        ins->next->opcode = "jf";
    }

    return CONTINUE;
}

static opt_pass_behavior peephole_incdec(instruction_t* ins)
{
    int negative = 0;

    if (strcmp(ins->opcode, "pushi") != 0 || (strcmp(ins->operand, "#1") != 0 && strcmp(ins->operand, "#-1") != 0))
        return SKIP;
    if (ins->next == NULL)
        return SKIP;
    if (ins->next->labels.size) // if the second instruction is a jump target
        return SKIP;

    if (strcmp(ins->operand, "#-1") == 0)
        negative = 1;

    if (strcmp(ins->next->opcode, "add") == 0)
    {
        ins->opcode = negative ? "dec" : "inc";
        ins->operand = "";
        remove_ins(ins->next);
    }
    else if (strcmp(ins->next->opcode, "sub") == 0)
    {
        ins->opcode = negative ? "inc" : "dec";
        ins->operand = "";
        remove_ins(ins->next);
    }

    return CONTINUE;
}

// optimize 'jcc .L1; jmp .out; .L1: ...' sequences
static opt_pass_behavior peephole_jcc_jmp(instruction_t* ins)
{
    if (ins->next == NULL || strcmp(ins->next->opcode, "jmp") != 0)
        return SKIP;
    if (ins->next->labels.size) // if the first instruction is a jump target
        return SKIP;
    if (ins->next->next == NULL)
    {
        int found_label = 0;
        for (int i = 0; i < ins->next->next->labels.size; ++i)
        {
            if (strcmp(ins->operand, ins->next->next->labels.ptr[i]) == 0)
                found_label = 1;
        }
        if (!found_label)
            return SKIP;
    }

    if (strcmp(ins->opcode, "jf") == 0)
    {
        ins->opcode = "jt";
        ins->operand = ins->next->operand;
        remove_ins(ins->next);
    }
    else if (strcmp(ins->opcode, "jt") == 0)
    {
        ins->opcode = "jf";
        ins->operand = ins->next->operand;
        remove_ins(ins->next);
    }

    return CONTINUE;
}

static opt_pass_behavior peephole_incldecl(instruction_t* ins)
{
    if (strcmp(ins->opcode, "pushl") != 0)
        return SKIP;
    if (ins->next == NULL || ins->next->next == NULL)
        return SKIP;
    if (ins->next->labels.size || ins->next->next->labels.size) // if the second or third instruction is a jump target
        return SKIP;
    if (strcmp(ins->next->next->opcode, "movl") != 0)
        return SKIP;
    if (strcmp(ins->operand, ins->next->next->operand) != 0)
        return SKIP;

    if (strcmp(ins->next->opcode, "inc") == 0)
    {
        ins->opcode = "incl";
        ins->operand = ins->operand;
        remove_ins(ins->next->next);
        remove_ins(ins->next);
    }
    else if (strcmp(ins->next->opcode, "dec") == 0)
    {
        ins->opcode = "decl";
        ins->operand = ins->operand;
        remove_ins(ins->next->next);
        remove_ins(ins->next);
    }
    else
        return SKIP;

    return CONTINUE;
}

static opt_pass_behavior peephole_jumpnot(instruction_t* ins)
{
    if (strcmp(ins->opcode, "lnot") != 0)
        return SKIP;
    if (ins->next == NULL)
        return SKIP;
    if (ins->labels.size) // if the first instruction is a jump target
        return SKIP;

    if (strcmp(ins->next->opcode, "jf") == 0)
    {
        // remove the 'notl'
        ins->next->opcode = "jt";
        remove_ins(ins);
    }
    else if (strcmp(ins->next->opcode, "jt") == 0)
    {
        // remove the 'notl'
        ins->next->opcode = "jf";
        remove_ins(ins);
    }
    else
        return SKIP;

    return NEXT_INSTRUCTION;
}

extern const char binop_opcodes[POD_TYPES_END][OP_BIN_END][8];
static opt_pass_behavior peephole_consteval_int(instruction_t* ins)
{
    if (strcmp(ins->opcode, "pushi") != 0 || ins->operand[0] != '#')
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "pushi") != 0 ||
        ins->next->labels.size || ins->next->operand[0] != '#')
        return SKIP;
    if (ins->next->next == NULL || ins->next->next->labels.size)
        return SKIP;

    if (strcmp(ins->next->next->opcode, "strcat") == 0)
        return SKIP;

    errno = 0;

    char* endptr = NULL;
    int lhs = strtol(ins->operand+1, &endptr, 0); // skip the first '#'
    if (errno != 0)
        return SKIP;
    endptr = NULL;
    int rhs = strtol(ins->next->operand+1, &endptr, 0);
    if (errno != 0)
        return SKIP;


    for (int i = 0; i < OP_BIN_END; ++i)
    {
        if (strcmp(ins->next->next->opcode, binop_opcodes[INT][i]) == 0)
        {

            int result = eval_int_binop(i, lhs, rhs);
            ins->operand = danpa_alloc(64);
            snprintf((char*)ins->operand, 64, "#%d", result);

            remove_ins(ins->next->next);
            remove_ins(ins->next);
            return CONTINUE;
        }
    }

    return SKIP;
}

static opt_pass_behavior peephole_useless_rep_nop(instruction_t* ins)
{
    int replaced = 0;

    if (strcmp(ins->opcode, "ret") == 0)
    {
        ins = ins->next;
        while (ins && ins->labels.size == 0 &&
               strcmp(ins->opcode, "ret") == 0)
        {
            instruction_t* old = ins;
            ins = ins->next;
            remove_ins(old);

            replaced = 1;
        }
    }
    if (ins && strcmp(ins->opcode, "nop") == 0)
    {
        ins = ins->next;
        while (ins && ins->labels.size == 0 &&
               strcmp(ins->opcode, "nop") == 0)
        {
            instruction_t* old = ins;
            ins = ins->next;
            remove_ins(old);

            replaced = 1;
        }
    }

    if (replaced)
        return CONTINUE;
    else
        return SKIP;
}

static opt_pass_behavior peephole_dup(instruction_t* ins)
{
    int replaced = 0;

    instruction_t* base_ins = ins;

    if (strcmp(base_ins->opcode, "copyl") == 0 ||
        strcmp(base_ins->opcode, "pushl") == 0)
    {
        ins = ins->next;
        while (ins && ins->labels.size == 0 &&
               strcmp(ins->opcode, "pushl") == 0 &&
               strcmp(ins->operand, base_ins->operand) == 0)
        {
            ins->opcode = "dup";
            ins->operand = "";

            replaced = 1;
        }
    }

    if (replaced)
        return CONTINUE;
    else
        return SKIP;
}

static opt_pass_behavior peephole_lnot_cmov(instruction_t* ins)
{
    if (strcmp(ins->opcode, "lnot") != 0)
        return SKIP;
    if (ins->next == NULL || strncmp(ins->next->opcode, "push", 4) != 0)
        return SKIP;
    if (ins->next->next == NULL || strncmp(ins->next->next->opcode, "push", 4) != 0)
        return SKIP;
    if (ins->next->next->next == NULL || strcmp(ins->next->next->next->opcode, "cmov") != 0)
        return SKIP;

    // swap the cmov operands
    const char* tmp_operand = ins->next->operand;
    ins->next->operand = ins->next->next->operand;
    ins->next->next->operand = tmp_operand;

    // remove the lnot
    remove_ins(ins);

    return NEXT_INSTRUCTION;
}

static opt_pass_behavior peephole_push_dup(instruction_t* ins)
{
    instruction_t* push_ins = ins;

    if (strncmp(push_ins->opcode, "push", 4) != 0)
        return SKIP;
    while (1)
    {
        ins = ins->next;

        if (ins == NULL || strcmp(push_ins->opcode, ins->opcode) != 0)
            return SKIP;
        if (strcmp(push_ins->operand, ins->operand) != 0)
            return SKIP;

        // replace the second push by a dup
        ins->opcode = "dup";
        ins->operand = "";
    }

    return CONTINUE;
}

// replace call; ret with jmp
static opt_pass_behavior peephole_tailcall(instruction_t* ins)
{
    if (strcmp(ins->opcode, "call") != 0)
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "ret") != 0 || ins->next->labels.size)
        return SKIP;

    ins->opcode = "jmp";
    remove_ins(ins->next);

    return CONTINUE;
}

// remove ret after jmp
static opt_pass_behavior peephole_dead_ret(instruction_t* ins)
{
    if (strcmp(ins->opcode, "jmp") != 0)
        return SKIP;
    if (ins->next == NULL || strcmp(ins->next->opcode, "ret") != 0 || ins->next->labels.size)
        return SKIP;

    remove_ins(ins->next);

    return CONTINUE;
}

// replace
// pushl
// pushi
// <op>
// with :
// pushi
// <op>l
static opt_pass_behavior peephole_inplace_bool_binops(instruction_t* ins)
{
    if (strcmp(ins->opcode, "pushl") != 0)
        return SKIP;
    if (ins->next == NULL || (strcmp(ins->next->opcode, "pushi") != 0 && strcmp(ins->next->opcode, "pushf")) || ins->next->labels.size)
        return SKIP;
    if (ins->next->next == NULL || ins->next->next->labels.size)
        return SKIP;
    const char* op = ins->next->next->opcode;

    for (int i = 0; i < OP_BIN_END; ++i)
    {
        if ((strcmp(op, binop_opcodes[INT][i]) == 0 || strcmp(op, binop_opcodes[REAL][i]) == 0)
            && operators[i].is_bool)
        {
            int op_len = strlen(op);
            char* inplace_op = danpa_alloc(op_len+1 + 1);
            memcpy(inplace_op, op, op_len);
            inplace_op[op_len] = 'l';
            inplace_op[op_len+1] = '\0';
            ins->next->next->opcode = inplace_op;
            ins->next->next->operand = ins->operand;
            remove_ins(ins);

            return NEXT_INSTRUCTION;
        }
    }

    return SKIP;
}

static opt_pass_behavior peephole_small_pushi(instruction_t* ins)
{
    if (strcmp(ins->opcode, "pushi") != 0)
        return SKIP;
    int32_t val = strtol(ins->operand+1, NULL, 0);
    if (ins->operand[0] == '#' && val >= -128 && val < 127) // can fit in a single byte
    {
        int8_t byte_val = val;
        ins->opcode  = "pushib";
        char* buffer = danpa_alloc(64);
        snprintf(buffer, 64, "#%d", byte_val);
        ins->operand = buffer;

        return CONTINUE;
    }
    else
        return SKIP;
}

typedef opt_pass_behavior(*opt_pass_t)(instruction_t*);

opt_pass_t optimization_passes[] =
    {
        peephole_copyl,
        peephole_useless_copyl,
        peephole_useless_copyl_sandwich,
        peephole_addsub_zero,
        peephole_pushi_copyl,
        peephole_pushpop,
        peephole_logicnot,
        peephole_logicnot_chain,
        peephole_jumpnot,
        peephole_jcc_jmp,
        peephole_incdec,
        peephole_incldecl,
        peephole_normalize_logic,
        peephole_cmov,
        peephole_consteval_int,
        peephole_useless_rep_nop,
        peephole_dup,
        peephole_lnot_cmov,
        peephole_push_dup,
        peephole_tailcall,
        peephole_dead_ret,
        peephole_inplace_bool_binops,
        peephole_small_pushi
};

static void do_peephole(instruction_t* ins)
{
    for (unsigned i = 0; i < sizeof(optimization_passes)/sizeof(optimization_passes[0]); ++i)
    {
        opt_pass_behavior result = optimization_passes[i](ins);
        if (result == NEXT_INSTRUCTION)
            return;
    }
}

static void shorten_jumps(instruction_t* ins)
{
    if (strcmp(ins->opcode, "jmp") != 0)
        return;

    hash_value_t* node = hash_table_get(&label_targets, ins->operand);
    assert(node);
    instruction_t* target = node->ptr;
    assert(target);

    if (strcmp(target->opcode, "jmp") == 0 ||
        strcmp(target->opcode, "jf" ) == 0 ||
        strcmp(target->opcode, "jt" ) == 0 ||
        strcmp(target->opcode, "ret") == 0)
    {
        ins->opcode = danpa_alloc(16);
        strncpy((char*)ins->opcode, target->opcode, 16);
        ins->operand = danpa_alloc(64);
        strncpy((char*)ins->operand, target->operand, 64);
    }
}

/*
 * TODO :
 * collapse op chains similar to this one :
 *  pushi #3
 *  add
 *  pushi #2 // func
 *  add
 * To
 *  pushi #5
 *  add
 */

void optimize_asm(instruction_t* instructions)
{
    instruction_t* ins = instructions;

    while (ins)
    {
        if (ins->next)
            assert(ins->next->prev == ins);

        do_peephole(ins);

        ins = ins->next;
    }

    find_label_targets(instructions);

    ins = instructions;

    // optimizations that require label target information
    while (ins)
    {
        shorten_jumps(ins);

        ins = ins->next;
    }
}
