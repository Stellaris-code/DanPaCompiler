#ifndef SOURCE_LOCATION_H
#define SOURCE_LOCATION_H

typedef struct token_t token_t;

typedef struct source_location_t
{
    const char* filename;
    const char* line_ptr;
    const char* ptr;
    struct token_t* macro_invok_token;
    enum
    {
        INCLUDED_TOKEN,
        MACRO_TOKEN,
        MACRO_ARG_TOKEN,
    } macro_invok_type;
    int line;
} source_location_t;

static inline void update_loc_newline(source_location_t* loc, const char* line_start)
{
    ++loc->line;
    loc->ptr = loc->line_ptr = line_start;
}

#endif // SOURCE_LOCATION_H
