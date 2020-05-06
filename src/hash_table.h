#ifndef HASH_TABLE_H_INCLUDED
#define HASH_TABLE_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>

#include "lexer.h"

typedef union hash_value_t
{
    void* ptr;
    void(*fn_ptr)(const char*, void*);
    int idx;
    const char* str;
    macro_def_t macro_def;
} hash_value_t;

typedef struct hash_node_t
{
    const char* key;
    union  hash_value_t value;
    struct hash_node_t* next_node;
} hash_node_t;

typedef struct hash_table_t
{
    size_t bucket_count;
    int count;
    hash_node_t** buckets;
} hash_table_t;

hash_table_t mk_hash_table(size_t bucket_count);

void          hash_table_insert(hash_table_t* table, const char* key, hash_value_t val);
hash_value_t* hash_table_get(hash_table_t* table, const char* key);
void          hash_table_remove(hash_table_t* table, const char* key);
void          hash_table_clear(hash_table_t* table);
void          hash_table_iterate(hash_table_t* table, void (*callback)(hash_node_t*));

#endif // HASH_TABLE_H_INCLUDED
