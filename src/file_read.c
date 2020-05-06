#include "file_read.h"

#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

uint8_t* read_file(const char* filename)
{
    FILE *input = fopen(filename, "rb");
    if (!input)
        return NULL;

    fseek(input, 0, SEEK_END);
    long fsize = ftell(input);
    rewind(input);  /* same as rewind(f); */

    if (fsize <= 0)
    {
        return NULL;
    }

    uint8_t* source_buffer = (uint8_t*)danpa_alloc(fsize + 2);
    fread(source_buffer, 1, fsize, input);
    source_buffer[fsize] = '\n'; // automatically add an ending newline
    source_buffer[fsize+1] = '\0';
    fclose(input);



    return source_buffer;
}
