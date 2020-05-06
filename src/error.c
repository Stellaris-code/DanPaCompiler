/*
error.c

Copyright (c) 16 Yann BOUCHER (yann)

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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "error.h"

#include "lexer.h"
#include "color.h"

static int in_error_block = 0;
static int error_block_will_exit = 0;

void error_begin()
{
    ++in_error_block;
}
void error_end()
{
    assert(in_error_block != 0);
    --in_error_block;
    if (in_error_block == 0 && error_block_will_exit)
    {
        exit(1);
    }
}

static void common_handler(const char* type, source_location_t where, int length, const char *fmt, va_list* args)
{
    if (where.macro_invok_token)
    {
        if (where.macro_invok_type == INCLUDED_TOKEN)
            info(where.macro_invok_token->location, where.macro_invok_token->length, "in file included from :\n");
        else if (where.macro_invok_type == MACRO_TOKEN)
            info(where.macro_invok_token->location, where.macro_invok_token->length, "in expansion of macro '%s' :\n", where.macro_invok_token->data.str);
    }

    char code[length + 1];
    memcpy(code, where.ptr, length);
    code[length] = '\0';

    fprintf(stderr, ESC_FONT_BOLD "%s:%d:%d: %s", where.filename, where.line, (int)(where.ptr - where.line_ptr + 1), type);
    vfprintf(stderr, fmt, *args);
    printf(ESC_FONT_NORMAL);
    // show the line as context
    // don't show the identation
    while (isspace(*where.line_ptr))
        ++where.line_ptr;
    printf("        "); // instead put a small indentation of 8 spaces

    const char* line_ptr = where.line_ptr;
    while (!is_newline(line_ptr) && *line_ptr != '\0')
    {
        putchar(*line_ptr);
        ++line_ptr;
    }
    putchar('\n');

    // underline the errornous part
    line_ptr = where.line_ptr;
    printf("        "); // same indentation
    while (line_ptr < where.ptr)
    {
        if (isspace(*line_ptr))
            putchar(*line_ptr);
        else
            putchar(' ');
        ++line_ptr;
    }
    printf(ESC_FG(0, 255, 0) ESC_FONT_BOLD);
    int first_char = 1;
    while (line_ptr < where.ptr + length)
    {
        putchar(first_char ? '^' : '~');
        ++line_ptr;
        first_char = 0;
    }
    printf("\n" ESC_POP_COLOR ESC_FONT_NORMAL);
}

void error(source_location_t where, int length, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    common_handler(ESC_FG(255, 0, 0) "error : " ESC_POP_COLOR, where, length, fmt, &args);

    va_end(args);

    if (in_error_block == 0)
        exit(1);
    else
        error_block_will_exit = 1;
}

void warn (source_location_t where, int length, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    common_handler(ESC_FG(255, 255, 0) "warning : " ESC_POP_COLOR, where, length, fmt, &args);

    va_end(args);
}

void info (source_location_t where, int length, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    common_handler(ESC_FG(73, 241, 253) "note : " ESC_POP_COLOR, where, length, fmt, &args);

    va_end(args);
}
