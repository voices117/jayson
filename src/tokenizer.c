#include "utils.h"
#include "tokenizer.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


#define DEBUG


/* parser constants */
#define NUMERIC "0123456789"
#define ALPHANUM "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" NUMERIC
#define STRING ALPHANUM "+-*/=&!@#$%^&()[]{}:.,~'_<>?` "
#define HEX NUMERIC "abcdefABCDEF"
#define BLANK " \t\n\r"


/* parser's FSM state ids */
typedef enum
{
  parser_state_init,
  parser_state_number,
  parser_state_string,
  parser_state_boolean,
  parser_state_null,
  parser_state_escape,
  parser_state_unicode,
  parser_state_not_found,
  parser_state_error,
  parser_state_last

} parser_state_t;


/* state description for the parser's FSM */
typedef struct
{
  /* valid characters for the state */
  const char* elems;

  /* state that the FSM enters after */
  parser_state_t next_state;

  /* callback triggered for every element in this state (or NULL to do nothing) */
  parser_cb_t cb;

#ifdef DEBUG
  /* string representation of the next state (debug only) */
  const char *_next_state_str;
#endif

} fsm_transition_t;


/* start a new state definition in the FSM states table */
#define STATE_INIT( state )\
  [ parser_state_ ## state ] = ( fsm_transition_t[] ){

/* ends the state definition started by STATE_INIT */
#define STATE_END\
  { 0 } },


/* when debug is on, transitions store the next state's name as a C string */
#ifdef DEBUG
  #define DEBUG_NEXT_STATE_STR( _next_state ) ._next_state_str = STR( _next_state ),
#else
  #define DEBUG_NEXT_STATE_STR( _next_state )
#endif


/* defines transition rules for a state (see STATE_INIT) */
#define TRANSITION( _elems, _cb, _next_state )        \
  {                                              \
    DEBUG_NEXT_STATE_STR( _next_state )          \
    .elems = _elems,                             \
    .cb = _cb,                                   \
    .next_state = parser_state_ ## _next_state   \
  }


#define INDENT( n )\
  do {\
    for( size_t i = 0; i < ( n ); i++ )\
      printf( "  " );\
  } while( 0 )


#define CAST( ctx )\
  ( ( tokenizer_ctx_t *) ctx )


bool _object_new( void *ctx, const char *c )
{
  printf( "{\n" );
  return true;
}


bool _object_end( void *ctx, const char *c )
{
  printf("}\n");
  return true;
}


bool _array_new( void *ctx, const char *c )
{
  printf( "[\n" );
  return true;
}


bool _array_end( void *ctx, const char *c )
{
  printf( "]\n" );
  return true;
}


bool _string_new( void *ctx, const char *c )
{
  CAST( ctx )->beginning = c + 1;
  CAST( ctx )->len = 0;
  return true;
}


bool _number_new( void *ctx, const char *c )
{
  CAST( ctx )->beginning = c;
  CAST( ctx )->len = 1;
  return true;
}


bool _colon( void *ctx, const char *c )
{
  printf(":\n");
  return true;
}


bool _comma( void *ctx, const char *c )
{
  printf( ",\n" );
  return true;
}


bool _boolean_new( void *ctx, const char *c )
{
  CAST( ctx )->beginning = c;
  CAST( ctx )->len = 1;
  return true;
}


bool _null_new( void *ctx, const char *c )
{
  CAST( ctx )->beginning = c;
  CAST( ctx )->len = 1;
  return true;
}


bool _null_end( void *ctx, const char *c )
{
  return true;
}


bool _put_char( void *ctx, const char *c )
{
  CAST( ctx )->len++;
  return true;
}


bool _number_end( void *ctx, const char *c )
{
  printf( "%.*s\n", CAST( ctx )->len, CAST( ctx )->beginning );
  return true;
}


bool _string_end( void *ctx, const char *c )
{
  printf( "%.*s\n", CAST( ctx )->len, CAST( ctx )->beginning );
  return true;
}


bool _bool_end( void *ctx, const char *c )
{
  const char *bool_value = "true";
  if( *CAST( ctx )->beginning == 'f' )
    bool_value = "false";

  if( CAST( ctx )->len != strlen( bool_value ) )
    return false;
  if( memcmp( CAST( ctx )->beginning, bool_value, CAST( ctx )->len ) != 0 )
    return false;

  printf( "%s\n", bool_value );
  return true;
}


bool _put_quote( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _put_backslash( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _put_solidus( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _put_backspace( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _put_formfeed( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _put_newline( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _put_carriage_return( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _put_tab( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


bool _update_unicode( void *ctx, const char *c )
{
  CAST( ctx )->len += 2;
  return true;
}


/* FSM transitions table */
const fsm_transition_t *_states[] =
{
  /* initial state */
  STATE_INIT( init )
    TRANSITION( BLANK, NULL, init ),
    TRANSITION( "{", _object_new, init ),
    TRANSITION( "}", _object_end, init ),
    TRANSITION( "[", _array_new, init ),
    TRANSITION( "]", _array_end, init ),
    TRANSITION( "\"", _string_new, string ),
    TRANSITION( NUMERIC, _number_new, number ),
    TRANSITION( ":", _colon, init ),
    TRANSITION( ",", _comma, init ),
    TRANSITION( "tf", _boolean_new, boolean ),
    TRANSITION( "n", _null_new, null ),
  STATE_END

  /* expects a number value */
  STATE_INIT( number )
    TRANSITION( NUMERIC, _put_char, number ),
    TRANSITION( "", _number_end, init ),
  STATE_END

  /* expects string characters */
  STATE_INIT( string )
    TRANSITION( STRING, _put_char, string ),
    TRANSITION( "\\", NULL, escape ),
    TRANSITION( "\"", _string_end, init ),
  STATE_END

  /* characters required to spell "true" and "false" */
  STATE_INIT( boolean )
    TRANSITION( "rueals", _put_char, boolean ),
    TRANSITION( "", _bool_end, init ),
  STATE_END

  /* characters required to spell "null" */
  STATE_INIT( null )
    TRANSITION( "ul", _put_char, null ),
    TRANSITION( "", _null_end, init ),
  STATE_END

  /* escapes the next character */
  STATE_INIT( escape )
    TRANSITION( "\"", _put_quote, string ),
    TRANSITION( "\\", _put_backslash, string ),
    TRANSITION( "/", _put_solidus, string ),
    TRANSITION( "b", _put_backspace, string ),
    TRANSITION( "f", _put_formfeed, string ),
    TRANSITION( "n", _put_newline, string ),
    TRANSITION( "r", _put_carriage_return, string ),
    TRANSITION( "t", _put_tab, string ),
    TRANSITION( "u", NULL, unicode ),
  STATE_END

  /* gets hexadecimal characters */
  STATE_INIT( unicode )
    TRANSITION( HEX, _update_unicode, unicode ),
  STATE_END
};


/** Processes one character from the input stream and acts according to the transitions table.
 *
 */
parser_state_t process_char( const char *p,
                             void *ctx,
                             parser_state_t current,
                             const fsm_transition_t *state_transitions )
{
  size_t i = 0;

  /* goes through each transition */
  while( state_transitions[ i ].elems != NULL )
  {
    /* an empty string acts as a wild card (any character is a match) */
    if( state_transitions[ i ].elems[0] == '\0' )
      goto found;

    /* goes through each element */
    for( size_t j = 0; j < strlen( state_transitions[ i ].elems ); j++ )
      if( *p == state_transitions[ i ].elems[ j ] )
        goto found;

    /* goes to the next transition */
    i++;
  }

#ifdef DEBUG
  printf( "\n" );
#endif

  /* if it gets this far, it means the character did not match any transition */
  return parser_state_not_found;

found:
  if( state_transitions[ i ].cb != NULL )
    if( !state_transitions[ i ].cb( ctx, p ) )
      return parser_state_error;

  /* returns the next state */
  return state_transitions[ i ].next_state;
}


/** Yields tokens from a string.
 *
 */
tok_error_t tokenize( const char *str, const char **token, size_t *token_len )
{
  /* gets a moving pointer */
  const char *p = str;
  parser_state_t state = parser_state_init;

  size_t char_num = 0;

  tokenizer_ctx_t ctx = {
    .beginning = NULL,
    .len = 0,
    .indent = 0
  };

  /* runs the FSM */
  while( *p != '\0' )
  {
    /* goes through all transitions of the current state */
    parser_state_t next_state = process_char( p, &ctx, state, _states[ state ] );
    if( next_state == parser_state_not_found )
    {
      printf( "Unexpected char \"%s\" in position %zu.\n", p, char_num );
      return tok_error_bad_input;
    }
    else if( next_state == parser_state_error )
    {
      printf( "Invalid expression found: %.*s\n", ctx.len, ctx.beginning );
      return tok_error_bad_input;
    }

    p++;
    char_num++;
    state = next_state;
  }

  return tok_error_no_error;
}
