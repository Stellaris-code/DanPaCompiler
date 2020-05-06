#include "preprocessor.h"

#include <string.h>
#include <assert.h>

#include "lexer.h"
#include "pp_expr_parser.h"
#include "hash_table.h"
#include "file_read.h"
#include "error.h"

hash_table_t macro_definitions;

void handle_if_chain(token_list_t* tokens, source_location_t* loc);

void init_pp()
{
    macro_definitions = mk_hash_table(64);
}

const char* handle_preprocessing_directives(token_list_t* tokens, source_location_t* loc)
{
    if (*loc->ptr != '#')
        return NULL;
    ++loc->ptr;

    while (isspace(*loc->ptr))
        ++loc->ptr;

    if (strncmp(loc->ptr, "include", 7) == 0)
    {
        loc->ptr += 7;
        skip_whitespace(loc, 0);

        token_t* filename_tok = danpa_alloc(sizeof(token_t));

        const char* next;
        if ((next = match_string_literal(loc->ptr, filename_tok)))
        {
            filename_tok->location = *loc; --filename_tok->location.ptr;
            filename_tok->length = strlen(filename_tok->data.str) + 2; // quotes

            loc->ptr = next;

            const char* included_file = (const char*)read_file(filename_tok->data.str);
            if (!included_file)
                error(*loc, strlen(filename_tok->data.str) + 2, "could not open include file '%s'\n", filename_tok->data.str);

            // add the included file's tokens
            const int old_size = tokens->size;
            source_location_t include_loc;
            include_loc.line = 1;
            include_loc.line_ptr = include_loc.ptr = included_file;
            include_loc.filename = filename_tok->data.str;
            include_loc.macro_invok_type = 0; include_loc.macro_invok_token = 0;
            do_tokenization(tokens, &include_loc, STARTS_ON_NEWLINE);
            const int new_tok_count = tokens->size - old_size;
            for (int i = 0; i < new_tok_count; ++i)
            {
                tokens->ptr[tokens->size-new_tok_count + i].location.macro_invok_token = filename_tok;
                tokens->ptr[tokens->size-new_tok_count + i].location.macro_invok_type = INCLUDED_TOKEN;
            }

            // skip until EOL
            while (*loc->ptr && !is_newline(loc->ptr))
                ++loc->ptr;
        }
        else
        {
            error(*loc, 1, "expected filename for include\n");
        }
    }
    else if (strncmp(loc->ptr, "define", 6) == 0)
    {
        loc->ptr += 6;
        skip_whitespace(loc, 0);

        token_t* macro_tok = danpa_alloc(sizeof(token_t));

        const char* next;
        if ((next = match_identifier(loc->ptr, macro_tok)))
        {
            macro_tok->location = *loc;
            macro_tok->length = strlen(macro_tok->data.str);

            loc->ptr = next;

            hash_value_t* hash_val;
            if ((hash_val = hash_table_get(&macro_definitions, macro_tok->data.str)))
            {
                token_t* tok = hash_val->macro_def.macro_ident;
                error_begin();
                error(*loc, macro_tok->length, "redefinition of macro '%s'\n", macro_tok->data.str);
                info(tok->location, tok->length, "first defined here\n");
                error_end();
            }

            macro_def_t macro_def;
            macro_def.variadic = 0;
            DYNARRAY_INIT(macro_def.args, 4);

            // arguments
            if (*loc->ptr == '(')
            {
                ++loc->ptr;
                skip_whitespace(loc, 0);

                if (*loc->ptr != ')')
                {
                    do
                    {
                        token_t ellipsis;
                        if ((next = match_delimiter(loc->ptr, &ellipsis)) &&
                            ellipsis.type == TOK_ELLIPSIS)
                        {
                            loc->ptr = next;
                            macro_def.variadic = 1;
                            break;
                        }

                        token_t arg;
                        next = match_identifier(loc->ptr, &arg);
                        if (next == NULL)
                            error(*loc, 1, "expected macro argument\n");

                        arg.location = *loc;
                        arg.length = strlen(arg.data.str);

                        loc->ptr = next;

                        DYNARRAY_ADD(macro_def.args, arg);
                        skip_whitespace(loc, 0);
                        if (*loc->ptr == ',')
                        {
                            ++loc->ptr;
                            skip_whitespace(loc, 0);
                            continue;
                        }
                        else
                            break;
                    } while (1);
                    skip_whitespace(loc, 0);
                    if (*loc->ptr != ')')
                        error(*loc, 1, "expected ')'\n");
                }
                ++loc->ptr;
            }

            DYNARRAY_INIT(macro_def.macro_tokens, 32);
            macro_def.macro_ident = macro_tok;

            loc->ptr = do_tokenization(&macro_def.macro_tokens, loc, STOP_ON_NEWLINE);

            hash_table_insert(&macro_definitions, macro_tok->data.str, (hash_value_t)macro_def);
            // advance to the next line
            while (*loc->ptr && !is_newline(loc->ptr))
                ++loc->ptr;
        }
        else
        {
            error(*loc, 1, "expected macro name\n");
        }
    }
    else if (strncmp(loc->ptr, "ifdef" , 5) == 0 ||
             strncmp(loc->ptr, "ifndef", 6) == 0 ||
             strncmp(loc->ptr, "if"    , 2) == 0)
    {
        handle_if_chain(tokens, loc);
    }
    else if (strncmp(loc->ptr, "error", 5) == 0)
    {
        source_location_t directive_loc = *loc;
        directive_loc.ptr--;

        loc->ptr += 5;
        skip_whitespace(loc, 0);

        token_t msg_tok;

        const char* next;
        if ((next = match_string_literal(loc->ptr, &msg_tok)))
        {
            loc->ptr = next;
            error(directive_loc, loc->ptr - directive_loc.ptr, "%s\n", msg_tok.data.str);
        }
        else
            error(*loc, 1, "expected error message\n");
    }
    else if (strncmp(loc->ptr, "warning", 7) == 0)
    {
        source_location_t directive_loc = *loc;
        directive_loc.ptr--;

        loc->ptr += 7;
        skip_whitespace(loc, 0);

        token_t msg_tok;

        const char* next;
        if ((next = match_string_literal(loc->ptr, &msg_tok)))
        {
            loc->ptr = next;
            warn(directive_loc, loc->ptr - directive_loc.ptr, "%s\n", msg_tok.data.str);
        }
        else
            error(*loc, 1, "expected warning message\n");
    }
    else
        error(*loc, 1, "unknown macro directive\n");

    return loc->ptr;
}

typedef struct if_contents_t
{
    enum
    {
        PP_IF_EXPR,
        PP_IFDEF,
        PP_IFNDEF
    } cond_type;
    source_location_t arg_loc;
    int               arg_len;
    token_list_t condition;
    token_list_t tokens;

    DYNARRAY(struct if_contents_t*) elifs;
    struct if_contents_t* else_branch;
} if_contents_t;

typedef enum parse_if_flags
{
    Nothing = 0,
    StopOnEndif,
    StopOnElifElseEndif
} parse_if_flags;


// UGLIEST FUNCTION EVEEEER
if_contents_t* parse_if_chain(source_location_t* loc, parse_if_flags flags)
{        
    if_contents_t* contents = danpa_alloc(sizeof(if_contents_t));
    DYNARRAY_INIT(contents->condition, 4);
    DYNARRAY_INIT(contents->tokens, 256);
    DYNARRAY_INIT(contents->elifs, 4);
    contents->else_branch = NULL;

    if (strncmp(loc->ptr, "ifdef"      , 5) == 0)
        contents->cond_type = PP_IFDEF;
    else if (strncmp(loc->ptr, "ifndef", 6) == 0)
        contents->cond_type = PP_IFNDEF;
    else if (strncmp(loc->ptr, "if"    , 2) == 0)
        contents->cond_type = PP_IF_EXPR;

    // skip to args
    while (*loc->ptr && !isspace(*loc->ptr))
        ++loc->ptr;

    skip_whitespace(loc, 0);

    contents->arg_loc = *loc;
    if (NULL == do_tokenization(&contents->condition, loc, STOP_ON_NEWLINE))
        error(*loc, 1, "expected macro condition\n");
    contents->arg_len = loc->ptr - contents->arg_loc.ptr;

    skip_newline(&loc->ptr);
    update_loc_newline(loc, loc->ptr);

    do
    {
        do
        {
            do_tokenization(&contents->tokens, loc, STOP_ON_PREPROC);
            ++loc->ptr;
            skip_whitespace(loc, 0);

            if (strncmp(loc->ptr, "ifdef" , 5) == 0 ||
                strncmp(loc->ptr, "ifndef", 6) == 0 ||
                strncmp(loc->ptr, "if"    , 2) == 0)
                handle_if_chain(&contents->tokens, loc);
        } while (*loc->ptr &&
                 (strncmp(loc->ptr, "endif", 5) != 0 &&
                  strncmp(loc->ptr, "elif", 4 ) != 0 &&
                  strncmp(loc->ptr, "else", 4 ) != 0));

        if (flags == StopOnElifElseEndif)
            goto out;
        if (strncmp(loc->ptr, "endif", 5) == 0 && flags == StopOnEndif)
            goto out;

        if (strncmp(loc->ptr, "elif", 4) == 0)
        {
            DYNARRAY_ADD(contents->elifs, parse_if_chain(loc, StopOnElifElseEndif));
            contents->elifs.ptr[contents->elifs.size-1]->cond_type = PP_IF_EXPR;
        }

        if (flags == StopOnElifElseEndif)
            goto out;
        if (strncmp(loc->ptr, "endif", 5) == 0 && flags == StopOnEndif)
            goto out;
        if (strncmp(loc->ptr, "else", 4) == 0)
        {
            contents->else_branch = parse_if_chain(loc, StopOnEndif);
        }
        if (flags == StopOnElifElseEndif || flags == StopOnEndif)
            goto out;
    } while (strncmp(loc->ptr, "endif", 5) != 0);

    // skip the endif
    while (*loc->ptr && !is_newline(loc->ptr))
        ++loc->ptr;
    skip_newline(&loc->ptr);
    update_loc_newline(loc, loc->ptr);

    out:
    return contents;
}

int test_if_condition(const if_contents_t* if_contents)
{
    if (if_contents->cond_type == PP_IF_EXPR)
    {
        token_list_t token_list;
        token_list_t* tokens = &token_list;
        DYNARRAY_INIT(*tokens, if_contents->condition.size);
        DYNARRAY_RESIZE(*tokens, if_contents->condition.size);
        memcpy(tokens->ptr, if_contents->condition.ptr, if_contents->condition.size * sizeof(token_t));

        token_list_t copy_token_list;
        DYNARRAY_INIT(copy_token_list, 16);
        // expand argument
        for (int i = 0; i < 16; ++i)
        {
            DYNARRAY_RESIZE(copy_token_list, 0);
            do_macro_expansion(tokens, &copy_token_list, 1);

            DYNARRAY_RESIZE(*tokens, copy_token_list.size);
            memcpy(tokens->ptr, copy_token_list.ptr, copy_token_list.size*sizeof(token_t));
        }

        return pp_evaluate_expr(tokens);
    }
    else
    {
        if (if_contents->condition.size != 1 || if_contents->condition.ptr[0].type != TOK_IDENTIFIER)
            error(if_contents->arg_loc, if_contents->arg_len, "expected macro name\n");

        const char* name = if_contents->condition.ptr[0].data.str;

        if (hash_table_get(&macro_definitions, name))
            return if_contents->cond_type == PP_IFDEF;
        else
            return if_contents->cond_type == PP_IFNDEF;
    }
}

void handle_if_chain(token_list_t* tokens, source_location_t* loc)
{
    if_contents_t* if_chain = parse_if_chain(loc, 0);

    if (test_if_condition(if_chain))
    {
        for (int k = 0; k < if_chain->tokens.size; ++k)
            DYNARRAY_ADD(*tokens, if_chain->tokens.ptr[k]);
    }
    else
    {
        for (int i = 0; i < if_chain->elifs.size; ++i)
        {
            if (test_if_condition(if_chain->elifs.ptr[i]))
            {
                for (int k = 0; k < if_chain->elifs.ptr[i]->tokens.size; ++k)
                    DYNARRAY_ADD(*tokens, if_chain->elifs.ptr[i]->tokens.ptr[k]);

                goto out;
            }
        }

        // no condition succeded, let's do the else if it exists
        if (if_chain->else_branch)
        {
            for (int k = 0; k < if_chain->else_branch->tokens.size; ++k)
                DYNARRAY_ADD(*tokens, if_chain->else_branch->tokens.ptr[k]);
        }
    }

out:
    return;
}

void do_macro_expansion(token_list_t* tokens, token_list_t* expanded_list, int test_for_defined)
{
    for (int i = 0; i < tokens->size; ++i)
    {
        macro_def_t def;
        hash_value_t* val;

        // handle 'defined(...)' keywords if in macro expr
        if (test_for_defined && tokens->ptr[i].type == TOK_IDENTIFIER && strncmp(tokens->ptr[i].data.str, "defined", 7) == 0)
        {
            if ((i+3 < tokens->size) && tokens->ptr[i+1].type == TOK_OPEN_PARENTHESIS
                && tokens->ptr[i+2].type == TOK_IDENTIFIER
                && tokens->ptr[i+3].type == TOK_CLOSE_PARENTHESIS)
            {
                // don't expand 'defined(...)' !
                for (int j = 0; j < 4; ++j)
                    DYNARRAY_ADD(*expanded_list, tokens->ptr[i+j]);
                i += 4;
            }
            else
                error(tokens->ptr[i].location, tokens->ptr[i].length, "expected macro name after 'defined'\n");
        }
        else if (tokens->ptr[i].type == TOK_IDENTIFIER && strcmp(tokens->ptr[i].data.str, "__FILE__") == 0)
        {
            token_t file_tok = tokens->ptr[i];
            file_tok.type = TOK_STRING_LITERAL;
            file_tok.data.str = tokens->ptr[i].location.filename;
            DYNARRAY_ADD(*expanded_list, file_tok);
        }
        else if (tokens->ptr[i].type == TOK_IDENTIFIER && strcmp(tokens->ptr[i].data.str, "__LINE__") == 0)
        {
            token_t line_tok = tokens->ptr[i];
            line_tok.type = TOK_INTEGER_LITERAL;
            line_tok.data.integer = tokens->ptr[i].location.line;
            DYNARRAY_ADD(*expanded_list, line_tok);
        }
        else if (tokens->ptr[i].type == TOK_IDENTIFIER && (val = hash_table_get(&macro_definitions, tokens->ptr[i].data.str)))
        {
            def = val->macro_def;

            token_t* macro_tok = &tokens->ptr[i];
            macro_tok->location.macro_invok_token = danpa_alloc(sizeof(token_t));
            *macro_tok->location.macro_invok_token = *def.macro_ident;
            macro_tok->location.macro_invok_type = MACRO_TOKEN;

            DYNARRAY(token_list_t) call_args;
            DYNARRAY_INIT(call_args, 4);
            if (tokens->ptr[i+1].type == TOK_OPEN_PARENTHESIS)
            {
                int parenthesis_depth = 1;
                ++i;

                if (tokens->ptr[i+1].type != TOK_CLOSE_PARENTHESIS)
                {
                    token_list_t arg_tokens;
                    DYNARRAY_INIT(arg_tokens, 4);
                    while (1)
                    {
                        if (i >= tokens->size)
                            error(macro_tok->location, macro_tok->length, "expected comma or ')'\n");

                        if (tokens->ptr[i+1].type == TOK_COMMA && parenthesis_depth==1)
                        {
                            ++i;
                            DYNARRAY_RESIZE(call_args, call_args.size + 1);
                            DYNARRAY_INIT  (call_args.ptr[call_args.size-1], arg_tokens.size);
                            DYNARRAY_RESIZE(call_args.ptr[call_args.size-1], arg_tokens.size);
                            memcpy(call_args.ptr[call_args.size-1].ptr, arg_tokens.ptr, arg_tokens.size*sizeof(token_t));

                            DYNARRAY_RESIZE(arg_tokens, 0);
                            continue;
                        }
                        else if (tokens->ptr[i+1].type == TOK_OPEN_PARENTHESIS)
                        {
                            ++parenthesis_depth;
                        }
                        else if (tokens->ptr[i+1].type == TOK_CLOSE_PARENTHESIS)
                        {
                            --parenthesis_depth;
                            if (parenthesis_depth < 0)
                                error(tokens->ptr[i+1].location, tokens->ptr[i+1].length, "unexpected ')'\n");
                        }
                        if (parenthesis_depth == 0)
                        {
                            ++i;
                            DYNARRAY_ADD(call_args, arg_tokens);
                            break;
                        }

                        DYNARRAY_ADD(arg_tokens, tokens->ptr[i+1]);
                        ++i;
                    }
                }
                else
                    ++i;
            }

            if ((def.variadic == 0 && call_args.size != def.args.size) ||
                (def.variadic == 1 && call_args.size <  def.args.size))
                error(macro_tok->location, macro_tok->length, "invalid macro argument count\n");

            for (int j = 0; j < def.macro_tokens.size; ++j)
            {
                int is_arg = 0;
                if (j != def.macro_tokens.size-1 &&
                    def.macro_tokens.ptr[j].type == TOK_HASH &&
                    def.macro_tokens.ptr[j+1].type == TOK_IDENTIFIER)
                {
                    ++j;

                    for (int k = 0; k < def.args.size; ++k)
                        if (strcmp(def.args.ptr[k].data.str, def.macro_tokens.ptr[j].data.str) == 0 && call_args.ptr[k].size)
                        {
                            const char* start = call_args.ptr[k].ptr[0].location.ptr;
                            const char* end   = call_args.ptr[k].ptr[call_args.ptr[k].size-1].location.ptr + call_args.ptr[k].ptr[call_args.ptr[k].size-1].length;

                            char* stringified = danpa_alloc(end-start + 1);
                            memcpy(stringified, start, end-start);
                            stringified[end-start] = '\0';

                            token_t count_tok = def.macro_tokens.ptr[j];
                            count_tok.type = TOK_STRING_LITERAL;
                            count_tok.data.str = stringified;
                            DYNARRAY_ADD(*expanded_list, count_tok);

                            is_arg = 1;
                        }
                }
                else if (def.macro_tokens.ptr[j].type == TOK_IDENTIFIER)
                {
                    // replace arguments
                    if (def.variadic == 1 && strcmp(def.macro_tokens.ptr[j].data.str, "__VA_ARGS__") == 0)
                    {

                        // ignore the named arguments
                        for (int k = def.args.size; k < call_args.size; ++k)
                        {
                            for (int q = 0; q < call_args.ptr[k].size; ++q)
                            {
                                call_args.ptr[k].ptr[q].location.macro_invok_token = danpa_alloc(sizeof(token_t));
                                *call_args.ptr[k].ptr[q].location.macro_invok_token = def.macro_tokens.ptr[j];
                                call_args.ptr[k].ptr[q].location.macro_invok_type = MACRO_TOKEN;
                                DYNARRAY_ADD(*expanded_list, call_args.ptr[k].ptr[q]);
                            }
                            if (k != call_args.size-1)
                            {
                                token_t comma = def.macro_tokens.ptr[j];
                                comma.type = TOK_COMMA;
                                DYNARRAY_ADD(*expanded_list, comma);
                            }
                        }
                        // if there was no variadic arguments sent, delete the last comma
                        if (def.args.size >= call_args.size)
                        {
                            assert(expanded_list->ptr[expanded_list->size-1].type == TOK_COMMA);
                            DYNARRAY_POP(*expanded_list);
                        }

                        is_arg = 1;
                    }
                    else if (def.variadic == 1 && strcmp(def.macro_tokens.ptr[j].data.str, "__VA_COUNT__") == 0)
                    {
                        token_t count_tok = def.macro_tokens.ptr[j];
                        count_tok.type = TOK_INTEGER_LITERAL;
                        count_tok.data.integer = call_args.size;
                        DYNARRAY_ADD(*expanded_list, count_tok);

                        is_arg = 1;
                    }
                    else
                    {
                        for (int k = 0; k < def.args.size; ++k)
                            if (strcmp(def.args.ptr[k].data.str, def.macro_tokens.ptr[j].data.str) == 0)
                            {
                                for (int q = 0; q < call_args.ptr[k].size; ++q)
                                {

                                    call_args.ptr[k].ptr[q].location.macro_invok_token = danpa_alloc(sizeof(token_t));
                                    *call_args.ptr[k].ptr[q].location.macro_invok_token = def.macro_tokens.ptr[j];
                                    call_args.ptr[k].ptr[q].location.macro_invok_type = MACRO_ARG_TOKEN;
                                    DYNARRAY_ADD(*expanded_list, call_args.ptr[k].ptr[q]);

                                    /*
                                    DYNARRAY_ADD(*expanded_list, def.macro_tokens.ptr[j]);
                                    expanded_list->ptr[expanded_list->size-1].location.macro_invok_token  = danpa_alloc(sizeof(token_t));
                                    *expanded_list->ptr[expanded_list->size-1].location.macro_invok_token = *macro_tok;
                                    expanded_list->ptr[expanded_list->size-1].location.macro_invok_type = MACRO_TOKEN;
                                    */
                                }
                                is_arg = 1;
                                break;
                            }
                    }
                }
                if (is_arg)
                    continue;
                DYNARRAY_ADD(*expanded_list, def.macro_tokens.ptr[j]);
                // if not an argument :
                expanded_list->ptr[expanded_list->size-1].location.macro_invok_token  = danpa_alloc(sizeof(token_t));
                *expanded_list->ptr[expanded_list->size-1].location.macro_invok_token = *macro_tok;
                expanded_list->ptr[expanded_list->size-1].location.macro_invok_type = MACRO_TOKEN;
            }
        }
        else
            DYNARRAY_ADD(*expanded_list, tokens->ptr[i]);
    }
}
