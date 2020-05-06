/*
lexer.c

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

#include "lexer.h"
#include "error.h"
#include "dynarray.h"
#include "alloc.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "preprocessor.h"

        const char* tokens_str[TOKEN_ENUM_END] =
        {
            "<eof>",
            "if",
            "else",
            "while",
            "do",
            "for",
            "foreach",
            "return",
            "typedef",
            "struct",
            "asm",
            "sizeof",
            "break",
            "continue",
            "match",
            "null",
            "new",
            "<identifier>",
            "<operator>",
            "=",
            "+=",
            "-=",
            "*=",
            "/=",
            "%=",
            "~=",
            "<integer constant>",
            "<float constant>",
            "<string constant>",
            "(",
            ")",
            "{",
            "}",
            "[",
            "]",
            ",",
            ":",
            ";",
            ".",
            "->",
            "?",
            "...",
            "..",
            "=>",
            "#"
};

static inline int is_first_ident_char(char c)
{
    return isalpha(c) || c == '_';
}

static inline int is_ident_char(char c)
{
    return isalnum(c) || c == '_';
}

const char* match_operator(const char* ptr, token_t* tok)
{
    if (strncmp(ptr, "+=", 2) == 0)
    {
        tok->type = TOK_ADD_ASSIGNMENT_OP;
        return ptr + 2;
    }
    if (strncmp(ptr, "-=", 2) == 0)
    {
        tok->type = TOK_SUB_ASSIGNMENT_OP;
        return ptr + 2;
    }
    if (strncmp(ptr, "*=", 2) == 0)
    {
        tok->type = TOK_MUL_ASSIGNMENT_OP;
        return ptr + 2;
    }
    if (strncmp(ptr, "/=", 2) == 0)
    {
        tok->type = TOK_DIV_ASSIGNMENT_OP;
        return ptr + 2;
    }
    if (strncmp(ptr, "%=", 2) == 0)
    {
        tok->type = TOK_MOD_ASSIGNMENT_OP;
        return ptr + 2;
    }
    if (strncmp(ptr, "~=", 2) == 0)
    {
        tok->type = TOK_CAT_ASSIGNMENT_OP;
        return ptr + 2;
    }

    int longest_match = 0;
    for (int i = 0; i < OP_ENUM_END; ++i)
    {
        const int len = strlen(operators[i].str);
        if (strncmp(ptr, operators[i].str, len) == 0 && len > longest_match)
        {
            tok->type = TOK_OPERATOR;
            tok->data.op = (operator_type_t)i;

            longest_match = len;
        }
    }
    if (longest_match != 0)
        return ptr + longest_match;

    if (strncmp(ptr, "=", 1) == 0)
    {
        tok->type = TOK_ASSIGNMENT_OP;
        return ptr + 1;
    }

    return NULL;
}

const char* match_keyword(const char* ptr, token_t* tok)
{
    if (strncmp(ptr, "if", 2) == 0 && !is_ident_char(ptr[2]))
    {
        tok->type = KEYWORD_IF;
        return ptr + 2;
    }
    if (strncmp(ptr, "in", 2) == 0 && !is_ident_char(ptr[2]))
    {
        tok->type = TOK_OPERATOR;
        tok->data.op = OP_IN;
        return ptr + 2;
    }
    if (strncmp(ptr, "do", 2) == 0 && !is_ident_char(ptr[2]))
    {
        tok->type = KEYWORD_DO;
        return ptr + 2;
    }
    if (strncmp(ptr, "for", 3) == 0 && !is_ident_char(ptr[3]))
    {
        tok->type = KEYWORD_FOR;
        return ptr + 3;
    }
    if (strncmp(ptr, "asm", 3) == 0 && !is_ident_char(ptr[3]))
    {
        tok->type = KEYWORD_ASM;
        return ptr + 3;
    }
    if (strncmp(ptr, "new", 3) == 0 && !is_ident_char(ptr[3]))
    {
        tok->type = KEYWORD_NEW;
        return ptr + 3;
    }
    if (strncmp(ptr, "else", 4) == 0 && !is_ident_char(ptr[4]))
    {
        tok->type = KEYWORD_ELSE;
        return ptr + 4;
    }
    if (strncmp(ptr, "null", 4) == 0 && !is_ident_char(ptr[4]))
    {
        tok->type = KEYWORD_NULL;
        return ptr + 4;
    }
    if (strncmp(ptr, "while", 5) == 0 && !is_ident_char(ptr[5]))
    {
        tok->type = KEYWORD_WHILE;
        return ptr + 5;
    }
    if (strncmp(ptr, "break", 5) == 0 && !is_ident_char(ptr[5]))
    {
        tok->type = KEYWORD_BREAK;
        return ptr + 5;
    }
    if (strncmp(ptr, "match", 5) == 0 && !is_ident_char(ptr[5]))
    {
        tok->type = KEYWORD_MATCH;
        return ptr + 5;
    }
    if (strncmp(ptr, "return", 6) == 0 && !is_ident_char(ptr[6]))
    {
        tok->type = KEYWORD_RETURN;
        return ptr + 6;
    }
    if (strncmp(ptr, "struct", 6) == 0 && !is_ident_char(ptr[6]))
    {
        tok->type = KEYWORD_STRUCT;
        return ptr + 6;
    }
    if (strncmp(ptr, "sizeof", 6) == 0 && !is_ident_char(ptr[6]))
    {
        tok->type = KEYWORD_SIZEOF;
        return ptr + 6;
    }
    if (strncmp(ptr, "foreach", 7) == 0 && !is_ident_char(ptr[7]))
    {
        tok->type = KEYWORD_FOREACH;
        return ptr + 7;
    }
    if (strncmp(ptr, "typedef", 7) == 0 && !is_ident_char(ptr[7]))
    {
        tok->type = KEYWORD_TYPEDEF;
        return ptr + 7;
    }
    if (strncmp(ptr, "continue", 8) == 0 && !is_ident_char(ptr[8]))
    {
        tok->type = KEYWORD_CONTINUE;
        return ptr + 8;
    }

    return NULL;
}

const char* match_identifier(const char* ptr, token_t* tok)
{
    const char* first_char = ptr;
    int ident_len = 0;

    // first character must be [a-zA-Z_]
    if (!is_first_ident_char(*first_char))
    {
        return NULL;
    }

    while (is_ident_char(*ptr))
    {
        ++ident_len;
        ++ptr;
    }

    char* token_ident_str = (char*)danpa_alloc(ident_len+1);
    memcpy(token_ident_str, first_char, ident_len);
    token_ident_str[ident_len] = '\0';

    tok->type = TOK_IDENTIFIER;
    tok->data.str = token_ident_str;

    return ptr;
}

const char* match_number_literal(const char* ptr, token_t* tok)
{
    const char* start = ptr;
    int literal_len = 0;
    int is_float = 0, is_hex = 0;

    if (*ptr == '-' || *ptr == '+')
    {
        return NULL; // leave unary operations to the parser
    }
    // hex prefix
    if (strncmp(ptr, "0x", 2) == 0)
    {
        ptr += 2;
        is_hex = 1;
    }

    while ((is_hex ? isxdigit(*ptr) : isdigit(*ptr)))
    {
        ++literal_len;
        ++ptr;
    }
    if (*ptr == '.' && ptr[1] != '.')
    {
        is_float = 1;
        if (is_hex)
            return NULL; // hex numbers can't be floats
        ++literal_len;
        ++ptr;
    }
    while ((is_hex ? isxdigit(*ptr) : isdigit(*ptr)))
    {
        ++literal_len;
        ++ptr;
    }

    // no digits found
    if (ptr == start)
        return NULL;

    errno = 0;

    if (!is_float)
    {
        char* endptr = NULL;
        int number = strtol(start, &endptr, 0);
        if (start == endptr)
            return NULL;
        assert(errno == 0); // should never happen

        tok->type = TOK_INTEGER_LITERAL;
        tok->data.integer = number;

        return ptr;
    }
    else
    {
        char* endptr = NULL;
        double number = strtod(start, &endptr);
        if (start == endptr)
            return NULL;
        assert(errno == 0); // should never happen

        tok->type = TOK_FLOAT_LITERAL;
        tok->data.fp = number;

        return ptr;
    }
}

// assumes we've already consumed the first '"'
const char* end_of_string_lit(const char* str)
{
    while (*str)
    {
        if (str[0] == '"' && str[-1] != '\\')
            return str;
        ++str;
    }

    return NULL;
}

const char* match_string_literal(const char* ptr, token_t* tok)
{
    if (*ptr != '"')
        return NULL;

    ++ptr;
    const char* literal_start = ptr;
    const char* literal_end   = end_of_string_lit(ptr);
    if (literal_end == NULL)
        return NULL;
    int literal_len = literal_end - literal_start;

    char* tok_str = (char*)danpa_alloc(literal_len+1);

    memcpy(tok_str, literal_start, literal_len);
    tok_str[literal_len] = '\0';

    tok->type = TOK_STRING_LITERAL;
    tok->data.str = tok_str;

    ptr = literal_end + 1;

    return ptr;
}

const char* match_delimiter(const char* ptr, token_t* tok)
{
    if (strncmp(ptr, "(", 1) == 0)
    {
        tok->type = TOK_OPEN_PARENTHESIS;
        return ptr + 1;
    }
    if (strncmp(ptr, ")", 1) == 0)
    {
        tok->type = TOK_CLOSE_PARENTHESIS;
        return ptr + 1;
    }
    if (strncmp(ptr, "{", 1) == 0)
    {
        tok->type = TOK_OPEN_BRACE;
        return ptr + 1;
    }
    if (strncmp(ptr, "}", 1) == 0)
    {
        tok->type = TOK_CLOSE_BRACE;
        return ptr + 1;
    }
    if (strncmp(ptr, "[", 1) == 0)
    {
        tok->type = TOK_OPEN_BRACKET;
        return ptr + 1;
    }
    if (strncmp(ptr, "]", 1) == 0)
    {
        tok->type = TOK_CLOSE_BRACKET;
        return ptr + 1;
    }
    if (strncmp(ptr, ",", 1) == 0)
    {
        tok->type = TOK_COMMA;
        return ptr + 1;
    }
    if (strncmp(ptr, ":", 1) == 0)
    {
        tok->type = TOK_COLON;
        return ptr + 1;
    }
    if (strncmp(ptr, ";", 1) == 0)
    {
        tok->type = TOK_SEMICOLON;
        return ptr + 1;
    }
    if (strncmp(ptr, "...", 3) == 0)
    {
        tok->type = TOK_ELLIPSIS;
        return ptr + 3;
    }
    if (strncmp(ptr, "..", 2) == 0)
    {
        tok->type = TOK_SLICE_DOTS;
        return ptr + 2;
    }
    if (strncmp(ptr, ".", 1) == 0)
    {
        tok->type = TOK_DOT;
        return ptr + 1;
    }
    if (strncmp(ptr, "?", 1) == 0)
    {
        tok->type = TOK_QUESTION;
        return ptr + 1;
    }
    if (strncmp(ptr, "->", 2) == 0)
    {
        tok->type = TOK_ARROW;
        return ptr + 2;
    }
    if (strncmp(ptr, "=>", 2) == 0)
    {
        tok->type = TOK_MATCH_OP;
        return ptr + 2;
    }
    if (strncmp(ptr, "#", 1) == 0)
    {
        tok->type = TOK_HASH;
        return ptr + 1;
    }

    return NULL;
}

const char* consume_comment(const char* ptr, source_location_t* loc)
{
    if (strncmp(ptr, "/*", 2) == 0)
    {
        while (*ptr && strncmp(ptr, "*/", 2) != 0)
        {
            if (is_newline(ptr))
            {
                skip_newline(&ptr);
                update_loc_newline(loc, ptr);
            }
            else
                ++ptr;
        }

        ptr += 2;
        return ptr;
    }
    else if (strncmp(ptr, "//", 2) == 0)
    {
        while (*ptr && !is_newline(ptr))
            ++ptr;

        return ptr;
    }

    return NULL;
}

#define MATCH_TOKEN(next, token) \
)
const char *do_tokenization(token_list_t* tokens, source_location_t* loc, int flags)
{
    int first_line = 1;
    int on_new_line;
    while (*loc->ptr)
    {
        on_new_line = skip_whitespace(loc, !(flags & STOP_ON_NEWLINE));
        if (first_line && (flags & STARTS_ON_NEWLINE))
        {
            first_line = 0;
            on_new_line = 1;
        }
        if (is_newline(loc->ptr))
            break;
        if (*loc->ptr == '#' && (flags & STOP_ON_PREPROC))
            break;

        if (*loc->ptr == '\0')
            return NULL;

        token_t token;

        loc->macro_invok_token = NULL;

        const char* next;
        // handle token concatenation
        if (strncmp(loc->ptr, "##", 2) == 0)
        {
            loc->ptr += 2;
            token_list_t next_token_list; DYNARRAY_INIT(next_token_list, 1);
            loc->ptr = do_tokenization(&next_token_list, loc, LEX_SINGLE_TOKEN);
            assert(next_token_list.size > 0);

            if (tokens->size == 0)
            {
                error(*loc, 2, "invalid token concatenation operator usage\n");
            }
            token_t* prev_token = &tokens->ptr[tokens->size-1];
            token_t* next_token = &next_token_list.ptr[0];
            int next_token_len = next_token->length;
            char* concat_token = danpa_alloc(prev_token->length+next_token->length+1);
            memcpy(concat_token, prev_token->location.ptr, prev_token->length);
            memcpy(concat_token+prev_token->length, next_token->location.ptr, next_token->length);
            concat_token[prev_token->length+next_token->length] = '\0';

            source_location_t mock_loc = prev_token->location;
            mock_loc.ptr = concat_token;
            DYNARRAY_RESIZE(next_token_list, 0); // reuse next_token_list
            if (do_tokenization(&next_token_list, &mock_loc, LEX_SINGLE_TOKEN) == NULL)
            {
                error(*loc, prev_token->length+next_token->length+2, "invalid token concatenation (%s)\n", concat_token);
            }
            assert(next_token_list.size);
            token = next_token_list.ptr[0];
            token.location = prev_token->location;
            token.length = prev_token->length+2+next_token_len;

            tokens->ptr[tokens->size-1] = token;
        }
        // preprocessor directives
        else if (on_new_line && *loc->ptr == '#' && (next = handle_preprocessing_directives(tokens, loc)))
        {
            loc->ptr = next;
        }
        // comments
        else if (*loc->ptr == '/' && (next = consume_comment(loc->ptr, loc)))
        {
            loc->ptr = next;
        }
        // regular tokens
        else if ((next = match_keyword(loc->ptr, &token))        ||
                (next = match_delimiter(loc->ptr, &token))      ||
                (next = match_identifier(loc->ptr, &token))     ||
                (next = match_number_literal(loc->ptr, &token)) ||
                (next = match_operator(loc->ptr, &token))       ||
                (next = match_string_literal(loc->ptr, &token)))
        {
            token.location = *loc;
            token.length = next - loc->ptr;

            DYNARRAY_ADD(*tokens, token);
            loc->ptr = next;
        }
        else
        {
            error(*loc, 1, "unexpected token '%c' (0x%x) found\n", loc->ptr[0], loc->ptr[0]);
        }

        if (flags & LEX_SINGLE_TOKEN)
            break;
    }
    /*
    if (*loc->ptr == '\0')
        return NULL;
    */

    return loc->ptr;
}

void tokenize_program(token_list_t* tokens, const char *source, const char *filename)
{
    source_location_t loc;
    loc.filename = filename;
    loc.ptr = source;
    loc.line = 1;
    loc.line_ptr = source;
    loc.macro_invok_token = NULL;

    do_tokenization(tokens, &loc, STARTS_ON_NEWLINE);

    token_list_t expanded_list;
    DYNARRAY_INIT(expanded_list, tokens->size);
    do_macro_expansion(tokens, &expanded_list, 0);

    // do up to 16 layers of macro expansion
    for (int i = 0; i < 16; ++i)
    {
        DYNARRAY_RESIZE(expanded_list, 0);
        do_macro_expansion(tokens, &expanded_list, 0);

        DYNARRAY_RESIZE(*tokens, expanded_list.size);
        memcpy(tokens->ptr, expanded_list.ptr, expanded_list.size*sizeof(token_t));
    }
}
