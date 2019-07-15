#ifndef JSON_TOKENIZER_H_
#define JSON_TOKENIZER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "json_types.h"
#include "stream.h"


/** JSON token type. */
typedef enum {
    json_token_comma,
    json_token_object_open,
    json_token_object_close,
    json_token_array_open,
    json_token_array_close,
    json_token_string,
    json_token_integer,
    json_token_fraction,
    json_token_boolean,
    json_token_colon,
    json_token_error,
    json_token_none,
    json_token_eof,
} json_token_type_t;

/** JSON token. */
typedef struct {
    /** Token type. */
    json_token_type_t type;

    /** Token value. */
    union {
        char *string;
        integer_t integer;
        fraction_t fraction;
        bool boolean;
        const char *error_msg;
    } value;
} json_token_t;

/** Initializes the stream API. */
#define TOKEN_STREAM_INIT( s, data ) do { ( s )->next = _next; ( s )->_data = data; } while( 0 )

/** Creates an error token with the given message. */
#define TOKEN_ERROR( msg ) ( ( json_token_t ) { .type = json_token_error, .value.error_msg = msg } )

/** Creates an EOF token. */
#define TOKEN_EOF ( ( json_token_t ) { .type = json_token_eof } )

/** Creates an none token. */
#define TOKEN_NONE ( ( json_token_t ) { .type = json_token_none } )

typedef unsigned char byte;

typedef struct {
    /** Stream that feeds inpu to the tokenizer. */
    stream_t *stream;

    /** varray that stores temporary data. */
    char *buffer;
} tokenizer_t;

void tokenizer_init( tokenizer_t *t, stream_t *stream );
json_token_t tokenizer_get_next( tokenizer_t *t );
void tokenizer_release( tokenizer_t *t );

void token_release( json_token_t *token );

#endif
