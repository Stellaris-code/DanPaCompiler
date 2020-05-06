#include "code_printer.h"


void print_code_output(instruction_t* list, FILE* stream)
{
    while (list)
    {
        // output the code
        for (int i = 0; i < list->labels.size; ++i)
            fprintf(stream, "%s:\n", list->labels.ptr[i]);
        fprintf(stream, "%s %s %s\n", list->opcode, list->operand, (list->comment ?: ""));

        list = list->next;
    }
}
