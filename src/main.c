#include <stdio.h>
#include <time.h>

#include "lexer.h"
#include "parser.h"
#include "ast_printer.h"
#include "code_generator.h"
#include "semantic_pass.h"
#include "ast_optimize.h"
#include "asm_optimizer.h"
#include "code_printer.h"
#include "preprocessor.h"
#include "alloc.h"
#include "builtin.h"
#include "file_read.h"

// TODO : mixin ! should be simple to implement
// TODO : implement mutable inplace operators
// TODO : have a 'primexpr_to_expr' type returning an initialized 'expression_t*'

const char* example_program = ""
                              "int collatz(int n)\n"
                              "{\n"
                              "if (n%2 == 0)\n"
                              " return n/2;\n"
                              "else\n"
                              " return 3*n + 1;\n"
                              "}\n"
                              "void main()\n"
                              "{\n"
                              "int val = asm(\"syscall #1\":int);\n"
                              "do{\n"
                              "    val = collatz(val);\n"
                              "    asm(\"syscall #0\", val);\n"
                              "} while (val != 1);\n"
                              "}";

int main()
{
    // don't use danpa_alloc, fool ! cleanup_memory will mess up the output !
    setvbuf(stdout, malloc(16384), _IOFBF, 16384); // fully buffered stdout

    clock_t time_start, time_end;
    time_start = clock();

    const char* filename = "tests.dps";
    //const char* filename = "program_shell.dps";
    //const char* filename = "program_linkedlist.dps";
    const char* out_name = "D:/Compiegne C++/Projets C++/DanpaAssembler/build/asm.dpa";

    const char* source_buffer = (const char*)read_file(filename);
    if (!source_buffer)
    {
        fprintf(stderr, "could not read input file '%s'", filename);
        return -1;
    }

    init_pp();
    init_builtins();

    token_list_t tokens;
    DYNARRAY_INIT(tokens, 1024);
    tokenize_program(&tokens, source_buffer, filename);
    token_t eof;
    eof.type = TOKEN_EOF;
    DYNARRAY_ADD(tokens, eof);

    set_parser_token_list(tokens.ptr);

    types_init();

    program_t prog;
    parse_program(&prog);

    semanal_program(&prog);
    for (int i = 0; i < 15; ++i) // multiple ast optimization passes
         ast_optimize_program(&prog);

    print_program(&prog);

    FILE* output = fopen(out_name, "wb");

    generate_program(&prog);
    for (int i = 0; i < 15; ++i) // multiple asm optimization passes
        optimize_asm(instruction_list);

    print_code_output(instruction_list, output);

    fclose(output);
    cleanup_memory();

    time_end = clock();
    printf("elasped time : %ldms\n", (time_end - time_start) * 1000 / CLOCKS_PER_SEC);
    fflush(stdout);
}
