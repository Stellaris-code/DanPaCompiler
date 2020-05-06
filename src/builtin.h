#ifndef BUILTIN_H
#define BUILTIN_H

#include "types.h"
#include "ast_nodes.h"

typedef struct builtin_t
{
    function_signature_t signature;
    void(*generate)(func_arg_list_t list);
} builtin_t;

void init_builtins();

builtin_t* find_builtin(const char* name);

#endif // BUILTIN_H
