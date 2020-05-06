/*
lexer.h

Copyright (c) 14 Yann BOUCHER (yann)

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
#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "operators.h"
#include "source_location.h"
#include "dynarray.h"

typedef enum token_type_t
{
    TOKEN_EOF = 0,
    TOKEN_INVALID = 0,

    KEYWORD_IF = 1,
    KEYWORD_ELSE,
    KEYWORD_WHILE,
    KEYWORD_DO,
    KEYWORD_FOR,
    KEYWORD_FOREACH,
    KEYWORD_RETURN,
    KEYWORD_TYPEDEF,
    KEYWORD_STRUCT,
    KEYWORD_ASM,
    KEYWORD_SIZEOF,
    KEYWORD_BREAK,
    KEYWORD_CONTINUE,
    KEYWORD_MATCH,
    KEYWORD_NULL,
    KEYWORD_NEW,

    TOK_IDENTIFIER,

    TOK_OPERATOR,
    TOK_ASSIGNMENT_OP,
    TOK_ADD_ASSIGNMENT_OP,
    TOK_SUB_ASSIGNMENT_OP,
    TOK_MUL_ASSIGNMENT_OP,
    TOK_DIV_ASSIGNMENT_OP,
    TOK_MOD_ASSIGNMENT_OP,
    TOK_CAT_ASSIGNMENT_OP,

    TOK_INTEGER_LITERAL, TOK_ASSIGNMENT_END = TOK_INTEGER_LITERAL,
    TOK_FLOAT_LITERAL,
    TOK_STRING_LITERAL,

    TOK_OPEN_PARENTHESIS,
    TOK_CLOSE_PARENTHESIS,
    TOK_OPEN_BRACE,
    TOK_CLOSE_BRACE,
    TOK_OPEN_BRACKET,
    TOK_CLOSE_BRACKET,
    TOK_COMMA,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_DOT,
    TOK_ARROW,
    TOK_QUESTION,
    TOK_ELLIPSIS,
    TOK_SLICE_DOTS,
    TOK_MATCH_OP,
    TOK_HASH, // for preprocessing purposes

    TOKEN_ENUM_END
} token_type_t;
extern const char* tokens_str[TOKEN_ENUM_END];

typedef union token_data_t
{
    int integer;
    float fp;
    int op;
    const char* str;
} token_data_t;

typedef struct token_t
{
    token_data_t data;
    token_type_t type;
    source_location_t location;
    int length;
} token_t;

typedef DYNARRAY(token_t) token_list_t;

typedef struct macro_def_t
{
    token_t* macro_ident;
    int variadic;
    DYNARRAY(token_t) args;
    token_list_t macro_tokens;
} macro_def_t;

typedef enum lexer_flags
{
    STOP_ON_NEWLINE = (1<<0),
    STOP_ON_PREPROC = (1<<1),
    STARTS_ON_NEWLINE = (1<<2),
    LEX_SINGLE_TOKEN = (1<<3)
} lexer_flags;

static inline int is_newline(const char* ptr)
{
    return *ptr == '\n' || strncmp(ptr, "\r\n", 2) == 0;
}
static inline void skip_newline(const char** ptr)
{
    if (strncmp(*ptr, "\r\n", 2) == 0 || strncmp(*ptr, "\n\r", 2) == 0)
        *ptr += 2;
    else
        ++*ptr;
}

// returns owned pointer
void tokenize_program(token_list_t* tokens, const char* source, const char* filename);

const char* match_identifier(const char* ptr, token_t* tok);
const char* match_number_literal(const char* ptr, token_t* tok);
const char* match_string_literal(const char* ptr, token_t* tok);
const char* match_delimiter(const char* ptr, token_t* tok);
const char *do_tokenization(token_list_t* tokens, source_location_t* loc, int flags);

// returns 1 if it's on a new line
static inline int skip_whitespace(source_location_t* loc, int consume_newlines)
{
    int on_new_line = 0;

    // skip all whitespace
    while (isspace(*loc->ptr) || (*loc->ptr == '\\' && is_newline(loc->ptr+1)))
    {
        if (*loc->ptr == '\\' && is_newline(loc->ptr+1))
        {
            ++loc->ptr; // skip to newline

            skip_newline(&loc->ptr);
            update_loc_newline(loc, loc->ptr);
        }
        else if (is_newline(loc->ptr))
        {
            on_new_line = 1;
            if (!consume_newlines)
                break;

            skip_newline(&loc->ptr);
            update_loc_newline(loc, loc->ptr);
        }
        else
        {
            ++loc->ptr;
        }
    }

    return on_new_line;
}

#endif // LEXER_H
