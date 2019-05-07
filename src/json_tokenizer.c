#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "json_tokenizer.h"
#include "varray.h"


#define ASIZE( x ) ( sizeof( x ) / sizeof( (x)[0] ) )
#define CHECK_TYPE( type, var ) ( 0 ? ( ( void (*)( type ) )NULL )( var ) : 0 )


typedef enum {
    state_id_init,
    state_id_string,
    state_id_numeric,
    state_id_fraction_first_digit,
    state_id_fraction,
    state_id_escape,
    state_id_error,
    state_id_end,

    state_id_last
} state_id_t;

struct action_ctx {
    json_token_t *token;
    tokenizer_t *tokenizer;
};

typedef bool ( *transition_action_cb_t )( struct action_ctx *ctx, char c );
typedef bool ( *transition_eof_action_cb_t )( struct action_ctx *ctx );

typedef struct {
    const char *values;
    size_t values_len;
    state_id_t next_state;
    transition_action_cb_t action;
} transition_t;

typedef struct {
    state_id_t next_state;
    transition_eof_action_cb_t action;
} transition_eof_t;

typedef struct {
    transition_t *transitions;
    size_t num_transitions;
    transition_eof_t transition_eof;
} state_t;


#define STATE( name, _transition_eof, ... ) \
    [state_id_##name] = { \
        .transitions = ( transition_t [] ){ __VA_ARGS__ }, \
        .num_transitions = ASIZE( ( ( transition_t [] ){ __VA_ARGS__ } ) ), \
        .transition_eof = _transition_eof, \
    }

#define TRANSITION( _next_state, _values, _action ) \
    { \
        .next_state = state_id_##_next_state, \
        .values = _values, \
        .values_len = sizeof( _values ) - 1, \
        .action = _action \
    }

#define TRANSITION_EOF( _next_state, _action ) { .next_state = state_id_##_next_state, .action = _action }

#define ANY NULL

static bool _action_token_object_open( struct action_ctx *ctx, char c );
static bool _action_token_object_close( struct action_ctx *ctx, char c );
static bool _action_token_array_open( struct action_ctx *ctx, char c );
static bool _action_token_array_close( struct action_ctx *ctx, char c );
static bool _action_token_colon( struct action_ctx *ctx, char c );
static bool _action_token_comma( struct action_ctx *ctx, char c );
static bool _action_string_init( struct action_ctx *ctx, char c );
static bool _action_numeric_init( struct action_ctx *ctx, char c );
static bool _action_token_string( struct action_ctx *ctx, char c );
static bool _action_string_store( struct action_ctx *ctx, char c );
static bool _action_string_do_escape( struct action_ctx *ctx, char c );
static bool _action_store_digit( struct action_ctx *ctx, char c );
static bool _action_fraction( struct action_ctx *ctx, char c );
static bool _action_token_integer_and_unget( struct action_ctx *ctx, char c );
static bool _action_token_integer( struct action_ctx *ctx );
static bool _action_token_fraction( struct action_ctx *ctx );
static bool _action_token_fraction_and_unget( struct action_ctx *ctx, char c );
static bool _action_unget( struct action_ctx *ctx, char c );
static bool _action_token_eof( struct action_ctx *ctx );
static bool _action_error_eof( struct action_ctx *ctx );
static bool _action_error_invalid_control_character( struct action_ctx *ctx, char c );


static const state_t _states[state_id_last] = {
    STATE( init,
        TRANSITION_EOF( end, _action_token_eof ),
        TRANSITION( init,    "\r\n\t ", NULL ),
        TRANSITION( end,     "{", _action_token_object_open ),
        TRANSITION( end,     "}", _action_token_object_close ),
        TRANSITION( end,     "[", _action_token_array_open ),
        TRANSITION( end,     "]", _action_token_array_close ),
        TRANSITION( end,     ":", _action_token_colon ),
        TRANSITION( end,     ",", _action_token_comma ),
        TRANSITION( string,  "\"", _action_string_init ),
        TRANSITION( numeric, "0123456789", _action_numeric_init ),
    ),
    STATE( string,
        TRANSITION_EOF( error, _action_error_eof ),
        TRANSITION( escape, "\\", NULL ),
        TRANSITION( end,    "\"", _action_token_string ),
        TRANSITION( error, "\n\r", _action_error_invalid_control_character ),
        TRANSITION( string, ANY, _action_string_store ),
    ),
    STATE( escape,
        TRANSITION_EOF( error, _action_error_eof ),
        TRANSITION( string, "nt\\rbf/", _action_string_do_escape ),
        TRANSITION( string, ANY, _action_string_store ),
    ),
    STATE( numeric,
        TRANSITION_EOF( end, _action_token_integer ),
        TRANSITION( numeric,              "0123456789", _action_store_digit ),
        TRANSITION( fraction_first_digit, ".", _action_fraction ),
        TRANSITION( end,                  ANY, _action_token_integer_and_unget ),
    ),
    STATE( fraction_first_digit,
        TRANSITION_EOF( error, _action_error_eof ),
        TRANSITION( fraction, "0123456789", _action_store_digit ),
    ),
    STATE( fraction,
        TRANSITION_EOF( end, _action_token_fraction ),
        TRANSITION( fraction, "0123456789", _action_store_digit ),
        TRANSITION( end,      ANY, _action_token_fraction_and_unget ),
    ),
};


static bool _action_token_object_open( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_object_open;
    return true;
}

static bool _action_token_object_close( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_object_close;
    return true;
}

static bool _action_token_array_open( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_array_open;
    return true;
}

static bool _action_token_array_close( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_array_close;
    return true;
}

static bool _action_token_colon( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_colon;
    return true;
}

static bool _action_token_comma( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_comma;
    return true;
}

static bool _action_string_init( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_string;
    varray_init( ctx->token->value.string, 64 );
    if( ctx->token->value.string == NULL ) {
        *ctx->token = TOKEN_ERROR( "Malloc error" );
        return false;
    }
    return true;
}

static bool _action_numeric_init( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_integer;
    ctx->token->value.integer = 0;
    varray_len( ctx->tokenizer->buffer ) = 0;
    varray_push( ctx->tokenizer->buffer, c );
    return true;
}

static bool _action_token_string( struct action_ctx *ctx, char c ) {
    assert( ctx->token->type == json_token_string );
    assert( c == '"' );
    varray_push( ctx->token->value.string, '\0' );
    return true;
}

static bool _action_string_store( struct action_ctx *ctx, char c ) {
    assert( ctx->token->type == json_token_string );
    varray_push( ctx->token->value.string, c );
    return true;
}

static bool _action_string_do_escape( struct action_ctx *ctx, char c ) {
    switch( c ) {
        case 'n':
            varray_push( ctx->token->value.string, '\n' );
            break;
        case 't':
            varray_push( ctx->token->value.string, '\t' );
            break;
        case '\\':
            varray_push( ctx->token->value.string, '\\' );
            break;
        case 'r':
            varray_push( ctx->token->value.string, '\r' );
            break;
        case 'b':
            varray_push( ctx->token->value.string, '\b' );
            break;
        case 'f':
            varray_push( ctx->token->value.string, '\f' );
            break;
        case '/':
            varray_push( ctx->token->value.string, '/' );
            break;
        default:
            *ctx->token = TOKEN_ERROR( "Unexpected escape character" );
            return false;
    }
    return true;
}

static bool _action_store_digit( struct action_ctx *ctx, char c ) {
    varray_push( ctx->tokenizer->buffer, c );
    return true;
}

static bool _action_fraction( struct action_ctx *ctx, char c ) {
    ctx->token->type = json_token_fraction;
    varray_push( ctx->tokenizer->buffer, c );
    return true;
}

static bool _action_token_integer_and_unget( struct action_ctx *ctx, char c ) {
    stream_put( ctx->tokenizer->stream, c );
    return _action_token_integer( ctx );
}

static bool _action_token_integer( struct action_ctx *ctx ) {
    errno = 0;
    varray_push( ctx->tokenizer->buffer, '\0' );
    ctx->token->value.integer = strtol( ctx->tokenizer->buffer, NULL, 10 );
    if( errno != 0 ) {
        *ctx->token = TOKEN_ERROR( "Integer conversion failed" );
        return false;
    }
    return true;
}

static bool _action_token_fraction( struct action_ctx *ctx ) {
    errno = 0;
    varray_push( ctx->tokenizer->buffer, '\0' );
    ctx->token->value.fraction = strtod( ctx->tokenizer->buffer, NULL );
    if( errno != 0 ) {
        *ctx->token = TOKEN_ERROR( "Fraction conversion failed" );
        return false;
    }
    return true;
}

static bool _action_token_fraction_and_unget( struct action_ctx *ctx, char c ) {
    stream_put( ctx->tokenizer->stream, c );
    return _action_token_fraction( ctx );
}

static bool _action_error_invalid_control_character( struct action_ctx *ctx, char c ) {
    token_release( ctx->token );
    *ctx->token = TOKEN_ERROR( "Invalid control character" );
    return false;
}

static bool _action_unget( struct action_ctx *ctx, char c ) {
    stream_put( ctx->tokenizer->stream, c );
    return true;
}

static bool _action_token_eof( struct action_ctx *ctx ) {
    token_release( ctx->token );
    *ctx->token = TOKEN_EOF;
    return true;
}

static bool _action_error_eof( struct action_ctx *ctx ) {
    token_release( ctx->token );
    *ctx->token = TOKEN_ERROR( "Unexpected end of file" );
    return false;
}


static state_id_t _fsm_step( const state_t *state, uint8_t c, state_id_t current, tokenizer_t *t, json_token_t *token ) {
    struct action_ctx ctx = {
        .token = token,
        .tokenizer = t,
    };

    /* finds a transition matching the current character */
    for( size_t i = 0; i< state->num_transitions; i++ ) {
        transition_t transition = state->transitions[i];
        if( transition.values == ANY || memchr( transition.values, c, transition.values_len ) != NULL ) {
            if( transition.action != NULL && !transition.action( &ctx, c ) ) {
                return state_id_error;
            }
            return transition.next_state;
        }
    }

    /* no transition matched */
    token_release( token );
    *token = TOKEN_ERROR( "Unexpected character" );
    return state_id_error;
}


static json_token_t _run_fsm( tokenizer_t *t ) {
    uint8_t c;
    state_id_t state = state_id_init;
    json_token_t token = TOKEN_NONE;
    struct action_ctx ctx = {
        .token = &token,
        .tokenizer = t,
    };

    while( stream_get( t->stream, &c ) ) {
        state = _fsm_step( &_states[state], c, state, t, &token );
        if( state == state_id_error ) {
            assert( token.type == json_token_error );
            return token;
        } else if( state == state_id_end ) {
            return token;
        }
    }

    if( t->stream->error ) {
        token_release( &token );
        return TOKEN_ERROR( "Input error" );
    }

    /* executes the end of file transition */
    if( _states[state].transition_eof.action ) {
        if( !_states[state].transition_eof.action( &ctx ) ) {
            assert( token.type == json_token_error );
            return token;
        }
    }
    state = _states[state].transition_eof.next_state;
    if( state != state_id_end ) {
        token_release( &token );
        return TOKEN_ERROR( "Unexpected end of file" );
    }

    return token;
}


void tokenizer_init( tokenizer_t *t, stream_t *stream ) {
    t->stream = stream;
    varray_init( t->buffer, 64 );
}

void tokenizer_release( tokenizer_t *t ) {
    varray_release( t->buffer );
}

json_token_t tokenizer_get_next( tokenizer_t *t ) {
    return _run_fsm( t );
}

void token_release( json_token_t *token ) {
    switch( token->type ) {
        case json_token_comma:
        case json_token_object_open:
        case json_token_object_close:
        case json_token_array_open:
        case json_token_array_close:
        case json_token_integer:
        case json_token_fraction:
        case json_token_colon:
        case json_token_error:
        case json_token_none:
        case json_token_eof:
            break;
        case json_token_string:
            varray_release( token->value.string );
    }
}
