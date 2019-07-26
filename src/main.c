#include <stdio.h>

#include "lexer.h"
#include "parser.h"
#include "ast_printer.h"
#include "code_generator.h"

const char* example_program =
        "int main()\n"
        "{\n"
        //"   if (errno + EFAULT)\n"
        //"       return format(\" test (%s)\", argc);\n"
        "int tom;\n"
        "int bob = 42;\n"
        "if (bob == 42)\n"
        "{\n"
        "   bob = 53.4;\n"
        "   int ted = 5*bob;\n"
        "}\n"
        "else if (bob == 54)\n"
        "   tom = 28;\n"
        "else\n"
        "   bob = 66;\n"
        "   return 1 + 2*bob + 3 == 585;\n"
        //"invalid/ \n"
        //"return 2*bob;\n"
        "}";

void print_token_type(token_t* token)
{
    switch (token->type)
    {
        case KEYWORD_IF:
            printf("if"); break;
        case KEYWORD_ELSE:
            printf("else"); break;
        case KEYWORD_WHILE:
            printf("while"); break;
        case KEYWORD_DO:
            printf("do"); break;
        case KEYWORD_FOR:
            printf("for"); break;
        case KEYWORD_RETURN:
            printf("return"); break;
        case TOK_IDENTIFIER:
            printf("ident (%s)", token->data.str); break;
        case TOK_OPERATOR:
            printf("%s", operators_str[token->data.op]); break;
        case TOK_INTEGER_LITERAL:
            printf("constant (%d)", token->data.integer); break;
        case TOK_FLOAT_LITERAL:
            printf("constant (%f)", token->data.fp); break;
        case TOK_STRING_LITERAL:
            printf("string (\"%s\")", token->data.str); break;
        case TOK_OPEN_PARENTHESIS:
            printf("("); break;
        case TOK_CLOSE_PARENTHESIS:
            printf(")"); break;
        case TOK_OPEN_BRACE:
            printf("{"); break;
        case TOK_CLOSE_BRACE:
            printf("}"); break;
        case TOK_COMMA:
            printf(","); break;
        case TOK_SEMICOLON:
            printf(";"); break;
        case TOK_ASSIGNMENT_OP:
            printf("="); break;

        default:
            printf("unknown (%d)\n", token->type); break;
    }
}

int main()
{
    token_t* token = tokenize_program(example_program, "<source>");
    set_parser_token_list(token);

    while (token->type != TOKEN_EOF)
    {
        printf("Token '");
        print_token_type(token);
        printf("'\n");

        ++token;
    }

    DYNARRAY(struct { int a; int b; }) test;
    DYNARRAY_INIT(test, 4);
    DYNARRAY_ADD(test, {8, 2});

    DYNARRAY(int) test2;
    DYNARRAY_INIT(test2, 4);
    DYNARRAY_ADD(test2, 256);


    typeof(*test.ptr) val = {8, 2};

    printf("%d %d | %d\n", test.ptr[0].a, test.ptr[0].b, test2.ptr[0]);
    printf("%d %d\n", val.a, val.b);

    program_t prog;
    parse_program(&prog);

    print_program(&prog);

    generate_program(&prog);
}
