#include "hash_table.h"
#include "hash.h"

#include "alloc.h"

#include <string.h>

hash_table_t mk_hash_table(size_t bucket_count)
{
    hash_table_t table;
    table.bucket_count = bucket_count;
    table.count = 0;
    table.buckets = danpa_alloc(sizeof(hash_node_t*)*bucket_count);
    memset(table.buckets, 0, sizeof(hash_node_t*)*bucket_count);

    return table;
}

void hash_table_insert(hash_table_t* table, const char* key, hash_value_t val)
{
    int hash = str_hash(key) % table->bucket_count;

    hash_node_t* new_node = danpa_alloc(sizeof(hash_node_t));
    new_node->key = key;
    new_node->value = val;
    new_node->next_node = NULL;

    if (table->buckets[hash] == NULL)
        table->buckets[hash] = new_node;
    else
    {
        hash_node_t* chain = table->buckets[hash];
        while (chain->next_node != NULL)
            chain = chain->next_node;

        chain->next_node = new_node;
    }

    ++table->count;
}

hash_value_t* hash_table_get(hash_table_t* table, const char* key)
{
    int hash = str_hash(key) % table->bucket_count;

    hash_node_t* chain = table->buckets[hash];
    while (chain)
    {
        // match
        if (strcmp(chain->key, key) == 0)
            return &chain->value;

        chain = chain->next_node;
    }

    return NULL;
}

void hash_table_remove(hash_table_t* table, const char* key)
{
    int hash = str_hash(key) % table->bucket_count;

    hash_node_t* previous = NULL;
    hash_node_t* chain = table->buckets[hash];
    while (chain)
    {
        // match
        if (strcmp(chain->key, key) == 0)
        {
            if (previous)
                previous->next_node = chain->next_node;
            else
                table->buckets[hash] = chain->next_node;
            //free(chain);

            break;
        }

        previous = chain;
        chain = chain->next_node;
    }

    --table->count;
}

void  hash_table_clear(hash_table_t* table)
{
    for (int i = 0; i < table->bucket_count; ++i)
        if (table->buckets[i])
        {
            hash_node_t* chain = table->buckets[i];
            while (chain)
            {
                hash_node_t* old = chain;
                chain = chain->next_node;
                //free(old);
            }

            table->buckets[i] = NULL;
        }

    table->bucket_count = 0;
    table->count = 0;
}

void hash_table_iterate(hash_table_t* table, void (*callback)(hash_node_t*))
{
    for (int i = 0; i < table->bucket_count; ++i)
        if (table->buckets[i])
        {
            hash_node_t* chain = table->buckets[i];
            while (chain)
            {
                callback(chain);
                chain = chain->next_node;
            }
        }
}
