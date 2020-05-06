#include "builtin.h"

#include "hash_table.h"
#include "code_generator.h"
#include "error.h"

#define MK_SIGNATURE(ret, ...) \
    ({ \
function_signature_t sig; \
sig.ret_type = ret; \
DYNARRAY_INIT(sig.parameter_types, 8); \
type_t arg_array[] = { __VA_ARGS__ }; \
for (unsigned i = 0; i < sizeof(arg_array)/sizeof(type_t); ++i) \
    DYNARRAY_ADD(sig.parameter_types, arg_array[i]); \
sig; \
})

static hash_table_t builtin_table;

void callback_size(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    if (args.ptr[0]->value_type.kind == ARRAY)
    {
        generate("memsize","");
        if (sizeof_type(&args.ptr[0]->value_type) > 1)
        {
            generate("pushi","#%d", sizeof_type(&args.ptr[0]->value_type));
            generate("idiv","");
        }
    }
    else
    {
        generate("strlen","");
    }
}

void callback_resize(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate_expression(args.ptr[1]);
    if (args.ptr[0]->value_type.kind == ARRAY)
    {
        if (sizeof_type(&args.ptr[0]->value_type) > 1)
        {
            generate("pushi","#%d", sizeof_type(&args.ptr[0]->value_type));
            generate("mul","");
        }
        generate("memresize","");
    }
    else
    {
        generate("strresize","");
    }
}

void callback_alloc(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("alloc","");
}

void callback_cos(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("cos","");
}
void callback_sin(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("sin","");
}
void callback_tan(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("tan","");
}
void callback_acos(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("acos","");
}
void callback_asin(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("asin","");
}
void callback_atan(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("atan","");
}
void callback_ln(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("ln","");
}
void callback_log10(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("log10","");
}
void callback_exp(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("exp","");
}
void callback_atan2(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate_expression(args.ptr[1]);
    generate("atan2","");
}
void callback_pow(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate_expression(args.ptr[1]);
    generate("pow","");
}
void callback_sqrt(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("sqrt","");
}
void callback_abs(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("abs","");
}
void callback_fabs(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("fabs","");
}
void callback_ceil(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("sqrt","");
}
void callback_floor(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("abs","");
}
void callback_rad2deg(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("rad2deg","");
}
void callback_deg2rad(func_arg_list_t args)
{
    generate_expression(args.ptr[0]);
    generate("deg2rad","");
}

void callback_find(func_arg_list_t args)
{
    if (!cmp_types(args.ptr[0]->value_type.array.array_type, &args.ptr[1]->value_type))
    {
        error(args.ptr[1]->loc, args.ptr[1]->length, "cannot call 'find' with types <%s> and <%s>\n",
              type_to_str(&args.ptr[1]->value_type), type_to_str(args.ptr[0]->value_type.array.array_type));
    }

    generate_expression(args.ptr[1]); // element
    generate_expression(args.ptr[0]); // array
    if (is_indirect_type(&args.ptr[1]->value_type))
    {
        generate("pushi", "#%d", sizeof_type(&args.ptr[1]->value_type)); // comparison size
        generate("findi","");
    }
    else
        generate("find","");
}

#define DEFINE_BUILTIN(name, ...) \
    static builtin_t builtin_##name; \
    builtin_##name = \
            (builtin_t){ \
            .signature = MK_SIGNATURE(__VA_ARGS__), \
    .generate = callback_##name \
            }; \
    hash_table_insert(&builtin_table, #name  , (hash_value_t){.ptr = &builtin_##name});

void init_builtins()
{
    builtin_table = mk_hash_table(64);

    DEFINE_BUILTIN(size  , mk_type(INT), mk_type(SPEC_ARRAY));
    DEFINE_BUILTIN(resize, mk_type(VOID), mk_type(SPEC_ARRAY), mk_type(INT));
    DEFINE_BUILTIN(alloc,  mk_type(SPEC_POINTER), mk_type(INT));
    DEFINE_BUILTIN(find,  mk_type(INT), mk_type(SPEC_ARRAY), mk_type(SPEC_ANY));

    // math builtins
    DEFINE_BUILTIN(cos,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(sin,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(tan,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(acos,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(asin,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(atan,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(atan2,  mk_type(REAL), mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(pow,  mk_type(REAL), mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(ln,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(log10,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(exp,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(sqrt,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(abs,  mk_type(INT), mk_type(INT));
    DEFINE_BUILTIN(fabs,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(ceil,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(floor,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(rad2deg,  mk_type(REAL), mk_type(REAL));
    DEFINE_BUILTIN(deg2rad,  mk_type(REAL), mk_type(REAL));

    /*
    builtin_size =
        (builtin_t){
            .signature = MK_SIGNATURE(mk_type(INT), mk_type(SPEC_ARRAY)),
            .generate = callback_size
        };
    builtin_resize =
        (builtin_t){
            .signature = MK_SIGNATURE(mk_type(VOID), mk_type(SPEC_ARRAY), mk_type(INT)),
            .generate = callback_resize
        };

    hash_table_insert(&builtin_table, "size"  , (hash_value_t){.ptr = &builtin_size});
    hash_table_insert(&builtin_table, "resize", (hash_value_t){.ptr = &builtin_resize});
    */
}

builtin_t* find_builtin(const char* name)
{
    hash_value_t* val;
    if ((val = hash_table_get(&builtin_table, name)))
    {
        return (builtin_t*)val->ptr;
    }
    else
        return NULL;
}
