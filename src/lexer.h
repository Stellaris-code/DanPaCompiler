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

typedef enum token_type_t
{
    TOKEN_EOF = 0,
    TOKEN_INVALID = 0,

    KEYWORD_IF = 1,
    KEYWORD_ELSE,
    KEYWORD_WHILE,
    KEYWORD_DO,
    KEYWORD_FOR,
    KEYWORD_RETURN,

    TOK_IDENTIFIER,

    TOK_OPERATOR,
    TOK_ASSIGNMENT_OP,

    TOK_INTEGER_LITERAL,
    TOK_FLOAT_LITERAL,
    TOK_STRING_LITERAL,

    TOK_OPEN_PARENTHESIS,
    TOK_CLOSE_PARENTHESIS,
    TOK_OPEN_BRACE,
    TOK_CLOSE_BRACE,
    TOK_COMMA,
    TOK_SEMICOLON,

    TOKEN_ENUM_END
} token_type_t;
extern const char* tokens_str[TOKEN_ENUM_END];

typedef enum operator_type_t
{
    // binary ops

    OP_ADD = 0,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQUAL,
    OP_GT,
    OP_GE,
    OP_LT,
    OP_LE,

        // unary ops
    OP_INC,
    OP_DEC,

    OP_ENUM_END
} operator_type_t;

extern const char operators_str[OP_ENUM_END][4];

typedef union token_data_t
{
    int integer;
    float fp;
    int op;
    const char* str;
} token_data_t;

typedef struct token_t
{
    token_type_t type;
    token_data_t data;

    const char* filename;
    int row, col;
} token_t;


// returns owned pointer
token_t *tokenize_program(const char* source, const char* filename);

#endif // LEXER_H
