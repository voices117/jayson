#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "fsm.h"
#include "json_tokenizer.h"
#include "varray.h"


#define ASIZE( x ) ( sizeof( x ) / sizeof( (x)[0] ) )
#define CHECK_TYPE( type, var ) ( 0 ? ( ( void (*)( type ) )NULL )( var ) : 0 )

/** States defined in the FSM that tokenizes the input. */
typedef enum {
    state_id_error = FSM_ERROR_STATE,
    state_id_init = FSM_INITIAL_STATE,
    state_id_end = FSM_END_STATE,
    state_id_string,
    state_id_numeric,
    state_id_fraction_first_digit,
    state_id_fraction,
    state_id_escape,
    state_id_false,
    state_id_true,

    state_id_last
} fsm_state_id_t;

/** Tokenizer context for the FSM. */
struct fsm_ctx {
    /** Token being parsed. */
    json_token_t token;
    /** Tokenizer instance. */
    tokenizer_t *tokenizer;
    /** Used when parsing boolean values to know which character must be matched next. */
    int boolean_index;
};

/** Defines an entry in the array of states that define the FSM.
 *
 *  @param name State name.
 *  @param _transition_eof Transition that handles the EOF.
 *  @param ... List of transitions that compose the state (see \c TRANSITION).
 */
#define STATE( name, _transition_eof, ... ) \
    [state_id_##name] = { \
        .transitions = ( transition_t [] ){ __VA_ARGS__ }, \
        .num_transitions = ASIZE( ( ( transition_t [] ){ __VA_ARGS__ } ) ), \
        .transition_eof = _transition_eof, \
    }

/** Defines a transition for an FSM state
 *
 *  @param _next_state Next state if the transition is taken.
 *  @param _values Array of values that trigger the transition.
 *  @param _action Callback executed when the transition is taken (or \c NULL).
 */
#define TRANSITION( _next_state, _values, _action ) \
    { \
        .next_state = state_id_##_next_state, \
        .values = _values, \
        .values_len = sizeof( _values ) - 1, \
        .action = ( transition_action_cb_t )_action, \
    }

/** Defines an EOF transition for an FSM state
 *
 *  @param _next_state Next state if the transition is taken.
 *  @param _action Callback executed when the transition is taken (or \c NULL).
 */
#define TRANSITION_EOF( _next_state, _action ) \
    { \
        .next_state = state_id_##_next_state, \
        .action = ( transition_eof_action_cb_t )_action, \
    }


/**
 * FSM transition actions.
 */
static bool _action_token_object_open( struct fsm_ctx *ctx, char c );
static bool _action_token_object_close( struct fsm_ctx *ctx, char c );
static bool _action_token_array_open( struct fsm_ctx *ctx, char c );
static bool _action_token_array_close( struct fsm_ctx *ctx, char c );
static bool _action_token_colon( struct fsm_ctx *ctx, char c );
static bool _action_token_comma( struct fsm_ctx *ctx, char c );
static bool _action_string_init( struct fsm_ctx *ctx, char c );
static bool _action_numeric_init( struct fsm_ctx *ctx, char c );
static bool _action_token_string( struct fsm_ctx *ctx, char c );
static bool _action_string_store( struct fsm_ctx *ctx, char c );
static bool _action_string_do_escape( struct fsm_ctx *ctx, char c );
static bool _action_store_digit( struct fsm_ctx *ctx, char c );
static bool _action_fraction( struct fsm_ctx *ctx, char c );
static bool _action_token_integer_and_unget( struct fsm_ctx *ctx, char c );
static bool _action_token_integer( struct fsm_ctx *ctx );
static bool _action_token_fraction( struct fsm_ctx *ctx );
static bool _action_token_fraction_and_unget( struct fsm_ctx *ctx, char c );
static bool _action_unget( struct fsm_ctx *ctx, char c );
static bool _action_token_eof( struct fsm_ctx *ctx );
static bool _action_error_eof( struct fsm_ctx *ctx );
static bool _action_error_invalid_control_character( struct fsm_ctx *ctx, char c );
static bool _action_boolean_false_init( struct fsm_ctx *ctx, char c );
static bool _action_check_false( struct fsm_ctx *ctx, char c );
static bool _action_token_false( struct fsm_ctx *ctx, char c );
static bool _action_boolean_true_init( struct fsm_ctx *ctx, char c );
static bool _action_check_true( struct fsm_ctx *ctx, char c );
static bool _action_token_true( struct fsm_ctx *ctx, char c );

/**
 * FSM states.
 */
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
        TRANSITION( false, "f", _action_boolean_false_init ),
        TRANSITION( true, "t", _action_boolean_true_init ),
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
    STATE( true,
        TRANSITION_EOF( error, _action_error_eof ),
        TRANSITION( true, "ru", _action_check_true ),
        TRANSITION( end,  "e", _action_token_true ),
    ),
    STATE( false,
        TRANSITION_EOF( error, _action_error_eof ),
        TRANSITION( false, "als", _action_check_false ),
        TRANSITION( end,   "e", _action_token_false ),
    ),
};


static bool _action_token_object_open( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_object_open;
    return true;
}

static bool _action_token_object_close( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_object_close;
    return true;
}

static bool _action_token_array_open( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_array_open;
    return true;
}

static bool _action_token_array_close( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_array_close;
    return true;
}

static bool _action_token_colon( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_colon;
    return true;
}

static bool _action_token_comma( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_comma;
    return true;
}

static bool _action_string_init( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_string;
    varray_init( ctx->token.value.string, 64 );
    if( ctx->token.value.string == NULL ) {
        ctx->token = TOKEN_ERROR( "Malloc error" );
        return false;
    }
    return true;
}

static bool _action_numeric_init( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_integer;
    ctx->token.value.integer = 0;
    varray_len( ctx->tokenizer->buffer ) = 0;
    varray_push( ctx->tokenizer->buffer, c );
    return true;
}

static bool _action_token_string( struct fsm_ctx *ctx, char c ) {
    assert( ctx->token.type == json_token_string );
    assert( c == '"' );
    varray_push( ctx->token.value.string, '\0' );
    return true;
}

static bool _action_string_store( struct fsm_ctx *ctx, char c ) {
    assert( ctx->token.type == json_token_string );
    varray_push( ctx->token.value.string, c );
    return true;
}

static bool _action_string_do_escape( struct fsm_ctx *ctx, char c ) {
    switch( c ) {
        case 'n':
            varray_push( ctx->token.value.string, '\n' );
            break;
        case 't':
            varray_push( ctx->token.value.string, '\t' );
            break;
        case '\\':
            varray_push( ctx->token.value.string, '\\' );
            break;
        case 'r':
            varray_push( ctx->token.value.string, '\r' );
            break;
        case 'b':
            varray_push( ctx->token.value.string, '\b' );
            break;
        case 'f':
            varray_push( ctx->token.value.string, '\f' );
            break;
        case '/':
            varray_push( ctx->token.value.string, '/' );
            break;
        default:
            ctx->token = TOKEN_ERROR( "Unexpected escape character" );
            return false;
    }
    return true;
}

static bool _action_store_digit( struct fsm_ctx *ctx, char c ) {
    varray_push( ctx->tokenizer->buffer, c );
    return true;
}

static bool _action_fraction( struct fsm_ctx *ctx, char c ) {
    ctx->token.type = json_token_fraction;
    varray_push( ctx->tokenizer->buffer, c );
    return true;
}

static bool _action_token_integer_and_unget( struct fsm_ctx *ctx, char c ) {
    stream_put( ctx->tokenizer->stream, c );
    return _action_token_integer( ctx );
}

static bool _action_token_integer( struct fsm_ctx *ctx ) {
    errno = 0;
    varray_push( ctx->tokenizer->buffer, '\0' );
    ctx->token.value.integer = strtol( ctx->tokenizer->buffer, NULL, 10 );
    if( errno != 0 ) {
        ctx->token = TOKEN_ERROR( "Integer conversion failed" );
        return false;
    }
    return true;
}

static bool _action_token_fraction( struct fsm_ctx *ctx ) {
    errno = 0;
    varray_push( ctx->tokenizer->buffer, '\0' );
    ctx->token.value.fraction = strtod( ctx->tokenizer->buffer, NULL );
    if( errno != 0 ) {
        ctx->token = TOKEN_ERROR( "Fraction conversion failed" );
        return false;
    }
    return true;
}

static bool _action_token_fraction_and_unget( struct fsm_ctx *ctx, char c ) {
    stream_put( ctx->tokenizer->stream, c );
    return _action_token_fraction( ctx );
}

static bool _action_error_invalid_control_character( struct fsm_ctx *ctx, char c ) {
    token_release( &ctx->token );
    ctx->token = TOKEN_ERROR( "Invalid control character" );
    return false;
}

static bool _action_unget( struct fsm_ctx *ctx, char c ) {
    stream_put( ctx->tokenizer->stream, c );
    return true;
}

static bool _action_token_eof( struct fsm_ctx *ctx ) {
    token_release( &ctx->token );
    ctx->token = TOKEN_EOF;
    return true;
}

static bool _action_error_eof( struct fsm_ctx *ctx ) {
    token_release( &ctx->token );
    ctx->token = TOKEN_ERROR( "Unexpected end of file" );
    return false;
}

static bool _action_boolean_false_init( struct fsm_ctx *ctx, char c ) {
    /* starts at 1 because the first character was matched when the token was identified */
    ctx->boolean_index = 1;
    return true;
}

static bool _action_check_false( struct fsm_ctx *ctx, char c ) {
    /* checks the character is correct */
    if( ctx->boolean_index >= strlen( "false" ) || "false"[ctx->boolean_index] != c ) {
        ctx->token = TOKEN_ERROR( "Unexpected character" );
        return false;
    }
    ctx->boolean_index += 1;
    return true;
}

static bool _action_token_false( struct fsm_ctx *ctx, char c ) {
    ctx->boolean_index += 1;
    if( ctx->boolean_index != strlen( "false" ) ) {
        ctx->token = TOKEN_ERROR( "Unexpected character" );
        return false;
    }
    ctx->token.type = json_token_boolean;
    ctx->token.value.boolean = false;
    return true;
}

static bool _action_boolean_true_init( struct fsm_ctx *ctx, char c ) {
    /* starts at 1 because the first character was matched when the token was identified */
    ctx->boolean_index = 1;
    return true;
}

static bool _action_check_true( struct fsm_ctx *ctx, char c ) {
    /* checks the character is correct */
    if( ctx->boolean_index >= strlen( "true" ) || "true"[ctx->boolean_index] != c ) {
        ctx->token = TOKEN_ERROR( "Unexpected character" );
        return false;
    }
    ctx->boolean_index += 1;
    return true;
}

static bool _action_token_true( struct fsm_ctx *ctx, char c ) {
    ctx->boolean_index += 1;
    if( ctx->boolean_index != strlen( "true" ) ) {
        ctx->token = TOKEN_ERROR( "Unexpected character" );
        return false;
    }
    ctx->token.type = json_token_boolean;
    ctx->token.value.boolean = true;
    return true;
}


void tokenizer_init( tokenizer_t *t, stream_t *stream ) {
    t->stream = stream;
    varray_init( t->buffer, 64 );
}

void tokenizer_release( tokenizer_t *t ) {
    varray_release( t->buffer );
}

json_token_t tokenizer_get_next( tokenizer_t *t ) {
    struct fsm_ctx ctx = {
        .token = TOKEN_NONE,
        .tokenizer = t,
    };
    state_id_t end_state = fsm_run( _states, t->stream, &ctx );

    switch( end_state ) {
        case FSM_ERROR_NO_MATCH:
            token_release( &ctx.token );
            return TOKEN_ERROR( "Unexpected character" );
        case FSM_ERROR_TRANSITION:
            assert( ctx.token.type == json_token_error );
            return ctx.token;
        case FSM_ERROR_STREAM:
            token_release( &ctx.token );
            return TOKEN_ERROR( "Input error" );
        case FSM_ERROR_STATE:
            assert( ctx.token.type == json_token_error );
            return ctx.token;
        case FSM_END_STATE:
            return ctx.token;
    }

    /* this shouldn't be reached */
    assert( false );
    return ctx.token;
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
        case json_token_boolean:
        case json_token_colon:
        case json_token_error:
        case json_token_none:
        case json_token_eof:
            break;
        case json_token_string:
            varray_release( token->value.string );
    }
}
