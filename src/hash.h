#ifndef HASH_H_INCLUDED
#define HASH_H_INCLUDED

#include <stdint.h>

uint64_t str_hash(const char *str)
{
    uint64_t hash = 5381;
    uint64_t c;

    while ((c = (uint64_t)*str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

#endif // HASH_H_INCLUDED
