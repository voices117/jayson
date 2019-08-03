#include <assert.h>
#include "fsm.h"
#include "json_tokenizer.h"
#include "parser.h"
#include "varray.h"


#define ASIZE( x ) ( sizeof( x ) / sizeof( (x)[0] ) )


/** Defines an entry in the array of states that define the FSM.
 *
 *  @param name State name.
 *  @param ... List of transitions that compose the state (see \c TRANSITION).
 */
#define STATE( name, ... ) \
    [parser_state_##name] = { \
        .transitions = ( transition_t [] ){ __VA_ARGS__ }, \
        .num_transitions = ASIZE( ( ( transition_t [] ){ __VA_ARGS__ } ) ), \
    }

/** Defines a transition for an FSM state
 *
 *  @param _next_state Next state if the transition is taken.
 *  @param _value JSON token that trigger the transition.
 *  @param _action Callback executed when the transition is taken (or \c NULL).
 */
#define TRANSITION( _next_state, _value, _action ) \
    { \
        .next_state = parser_state_##_next_state, \
        .values = ( const char [] ){ json_token_##_value }, \
        .values_len = 1, \
        .action = ( transition_action_cb_t )_action, \
    }

/** Defines a transition for any input token.
 *
 *  @param _next_state Next state if the transition is taken.
 *  @param _action Callback executed when the transition is taken (or \c NULL).
 */
#define TRANSITION_ANY( _next_state, _action ) \
    { \
        .next_state = parser_state_##_next_state, \
        .values = NULL, \
        .values_len = 1, \
        .action = ( transition_action_cb_t )_action, \
    }


/** Type of container. */
typedef enum {
    container_type_object,
    container_type_array,
} container_type_t;

/** Context used when calling the FSM. */
typedef struct {
    /** Stack of container types (var array). */
    container_type_t *container_types;
    /** Stack of JSON tokens (var array). */
    json_token_t *tokens;
    /** Handler. */
    json_handler_t *handler;
    /** Input stream. */
    stream_t *stream;
    /** JSON tokenizer. */
    tokenizer_t *tokenizer;
    /** Error message (or \c NULL is no error). */
    const char *error;
} fsm_ctx_t;


static bool _run_fsm( fsm_ctx_t *parser_ctx, tokenizer_t *tokenizer, state_id_t fsm_state );

static bool _action_object_start( fsm_ctx_t *ctx, char c );
static bool _action_object_close( fsm_ctx_t *ctx, char c );
static bool _action_object_key( fsm_ctx_t *ctx, char c );

static bool _action_array_start( fsm_ctx_t *ctx, char c );
static bool _action_array_close( fsm_ctx_t *ctx, char c );

static bool _action_string( fsm_ctx_t *ctx, char c );
static bool _action_integer( fsm_ctx_t *ctx, char c );
static bool _action_fraction( fsm_ctx_t *ctx, char c );
static bool _action_null( fsm_ctx_t *ctx, char c );
static bool _action_boolean( fsm_ctx_t *ctx, char c );

static bool _action_eof_unexpected( fsm_ctx_t *ctx );

static bool _action_recursive_parse( fsm_ctx_t *ctx, char c );
static bool _action_array_start_recursive( fsm_ctx_t *ctx, char c );
static bool _action_object_start_recursive( fsm_ctx_t *ctx, char c );


/** States defined in the FSM that handles tokens. */
typedef enum {
    parser_state_error = FSM_ERROR_STATE,
    parser_state_init = FSM_INITIAL_STATE,
    parser_state_end = FSM_END_STATE,
    parser_state_string,
    parser_state_integer,
    parser_state_fraction,
    parser_state_boolean,
    parser_state_object_start,
    parser_state_object_key,
    parser_state_object_after_key,
    parser_state_object_after_value,
    parser_state_array,
    parser_state_array_after_value,
    parser_state_array_value,

    state_id_last
} parser_state_t;


#define ELEMENT( next_state, _array_start_action, _object_start_action ) \
    TRANSITION( next_state, string,      _action_string ), \
    TRANSITION( next_state, integer,     _action_integer ), \
    TRANSITION( next_state, fraction,    _action_fraction ), \
    TRANSITION( next_state, null,        _action_null ), \
    TRANSITION( next_state, boolean,     _action_boolean )

static const state_t _states[state_id_last] = {
    STATE( init,
        ELEMENT( end, _action_array_start, _action_object_start ),
        TRANSITION( object_key, object_open, _action_object_start ),
        TRANSITION( array,      array_open,  _action_array_start ),
        TRANSITION( error,      eof,         _action_eof_unexpected ),
    ),
    STATE( object_key,
        TRANSITION( end,              object_close, _action_object_close ),
        TRANSITION( object_after_key, string,       _action_object_key ),
        TRANSITION( error,            eof,          _action_eof_unexpected ),
    ),
    STATE( object_after_key,
        TRANSITION( object_after_value, colon, _action_recursive_parse ),
        TRANSITION( error,              eof,   _action_eof_unexpected ),
    ),
    STATE( object_after_value,
        TRANSITION( object_key, comma, NULL ),
        TRANSITION( end,        object_close, _action_object_close ),
        TRANSITION( error,      eof,          _action_eof_unexpected ),
    ),
    STATE( array,
        ELEMENT( array_after_value, _action_array_start_recursive, _action_object_start_recursive ),
        TRANSITION( array_after_value, object_open, _action_object_start_recursive ),
        TRANSITION( array_after_value, array_open,  _action_array_start_recursive ),
        TRANSITION( end,               array_close, _action_array_close ),
        TRANSITION( error,             eof,         _action_eof_unexpected ),
    ),
    STATE( array_after_value,
        TRANSITION( array_value, comma,       NULL ),
        TRANSITION( end,         array_close, _action_array_close ),
        TRANSITION( error,       eof,         _action_eof_unexpected ),
    ),
    STATE( array_value,
        ELEMENT( array_after_value, _action_array_start_recursive, _action_object_start_recursive ),
        TRANSITION( array_after_value, object_open, _action_object_start_recursive ),
        TRANSITION( array_after_value, array_open,  _action_array_start_recursive ),
    ),
};


static bool _action_object_start( fsm_ctx_t *ctx, char c ) {
    bool rv = ctx->handler->object_start( ctx->handler->ctx );

    /* pushes the continer into the stack */
    varray_push( ctx->container_types, container_type_object );
    return rv;
}

static bool _action_object_close( fsm_ctx_t *ctx, char c ) {
    /* checks we are actually inside an object */
    if( varray_len( ctx->container_types ) == 0 ) {
        return false;
    }

    bool rv = ctx->handler->object_end( ctx->handler->ctx );
    ( void )varray_pop( ctx->container_types );
    return rv;
}

static bool _action_object_key( fsm_ctx_t *ctx, char c ) {
    return ctx->handler->object_key( ctx->handler->ctx, varray_last( ctx->tokens ).value.string );
}

static bool _action_array_start( fsm_ctx_t *ctx, char c ) {
    if( !ctx->handler->array_start( ctx->handler->ctx ) ) {
        return false; 
    }
    /* pushes the continer into the stack */
    varray_push( ctx->container_types, container_type_array );
    return true;
}

static bool _action_array_start_recursive( fsm_ctx_t *ctx, char c ) {
    if( !_action_array_start( ctx, c ) ) {
        return false;
    }

    /* does a recursive call to parse contained elements */
    return _run_fsm( ctx, ctx->tokenizer, parser_state_array );
}

static bool _action_object_start_recursive( fsm_ctx_t *ctx, char c ) {
    if( !_action_object_start( ctx, c ) ) {
        return false;
    }

    /* does a recursive call to parse contained elements */
    return _run_fsm( ctx, ctx->tokenizer, parser_state_object_key );
}

static bool _action_array_close( fsm_ctx_t *ctx, char c ) {
    /* checks we are actually inside an array */
    if( varray_len( ctx->container_types ) == 0 ) {
        return false;
    }

    bool rv = ctx->handler->array_end( ctx->handler->ctx );
    ( void )varray_pop( ctx->container_types );
    return rv;
}

static bool _action_string( fsm_ctx_t *ctx, char c ) {
    return ctx->handler->string( ctx->handler->ctx, varray_last( ctx->tokens ).value.string );
}

static bool _action_integer( fsm_ctx_t *ctx, char c ) {
    return ctx->handler->integer( ctx->handler->ctx, varray_last( ctx->tokens ).value.integer );
}

static bool _action_fraction( fsm_ctx_t *ctx, char c ) {
    return ctx->handler->fraction( ctx->handler->ctx, varray_last( ctx->tokens ).value.fraction );
}

static bool _action_null( fsm_ctx_t *ctx, char c ) {
    return ctx->handler->null( ctx->handler->ctx );
}

static bool _action_boolean( fsm_ctx_t *ctx, char c ) {
    return ctx->handler->boolean( ctx->handler->ctx, varray_last( ctx->tokens ).value.boolean );
}

static bool _action_recursive_parse( fsm_ctx_t *ctx, char c ) {
    /* runs the FSM recursively to parse any type of JSON element */
    return _run_fsm( ctx, ctx->tokenizer, parser_state_init );
}

static bool _action_eof_unexpected( fsm_ctx_t *ctx ) {
    ctx->error = "Unexpected end of file";
    return true;
}

static bool _step_fsm( fsm_ctx_t *parser_ctx, json_token_type_t type, state_id_t fsm_state ) {
    fsm_state = fsm_step( &_states[fsm_state], type, fsm_state, parser_ctx );
    switch( fsm_state ) {
        case FSM_ERROR_NO_MATCH:
            parser_ctx->error = "Unexpected token";
            return false;
        case FSM_ERROR_TRANSITION:
        case FSM_ERROR_STATE:
            assert( parser_ctx->error != NULL );
            return false;
        case FSM_ERROR_STREAM:
            parser_ctx->error = "Input error";
            return false;
        case FSM_END_STATE:
        default:
            break;
    }
    return true;
}

static bool _run_fsm( fsm_ctx_t *parser_ctx, tokenizer_t *tokenizer, state_id_t fsm_state ) {
    json_token_type_t type;
    do {
        varray_push( parser_ctx->tokens, tokenizer_get_next( tokenizer ) );
        type = varray_last( parser_ctx->tokens ).type;

        fsm_state = fsm_step( &_states[fsm_state], type, fsm_state, parser_ctx );
        switch( fsm_state ) {
            case FSM_ERROR_NO_MATCH:
                parser_ctx->error = "Unexpected token";
                goto error;
            case FSM_ERROR_TRANSITION:
            case FSM_ERROR_STATE:
                assert( parser_ctx->error != NULL );
                goto error;
            case FSM_ERROR_STREAM:
                parser_ctx->error = "Input error";
                goto error;
            case FSM_END_STATE:
            default:
                break;
        }
        token_release( &varray_pop( parser_ctx->tokens ) );
    } while( type != json_token_error && fsm_state != FSM_END_STATE );
    return ( fsm_state == FSM_END_STATE );

error:
    token_release( &varray_pop( parser_ctx->tokens ) );
    return false;
}


bool json_parse( json_handler_t *handler, json_read_cb_t read_cb, void *read_cb_ctx ) {
    /* initializes the stream for the tokenizer */
    stream_t stream;
    STREAM_INIT( &stream, read_cb, read_cb_ctx );    

    /* initializes the tokenizer */
    tokenizer_t tokenizer;
    tokenizer_init( &tokenizer, &stream );

    /* initializes the parser context */
    fsm_ctx_t parser_ctx;
    varray_init( parser_ctx.container_types, 5 );
    varray_init( parser_ctx.tokens, 5 );
    parser_ctx.handler = handler;
    parser_ctx.stream = &stream;
    parser_ctx.tokenizer = &tokenizer;

    /* runs the JSON FSM */
    bool success = _run_fsm( &parser_ctx, &tokenizer, parser_state_init );
    if( !success ) {
        assert( parser_ctx.error != NULL );
        parser_ctx.handler->error( parser_ctx.handler->ctx, parser_ctx.error, stream.line + 1, stream.column + 1 );
    }

    tokenizer_release( &tokenizer );
    varray_release( parser_ctx.container_types );
    varray_release( parser_ctx.tokens );
    return success;
}
