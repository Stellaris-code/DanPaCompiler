#ifndef PREPROCESSER_H
#define PREPROCESSER_H

#include "lexer.h"

typedef struct hash_table_t hash_table_t;

extern hash_table_t macro_definitions;

void init_pp();

const char* handle_preprocessing_directives(token_list_t* tokens, source_location_t *loc);
void do_macro_expansion(token_list_t* tokens, token_list_t* expanded_list, int test_for_defined);

#endif // PREPROCESSER_H
