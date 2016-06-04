#ifndef TOKENIZER_H
#define TOKENIZER_H


#include <stdbool.h>
#include <string.h>


/* module error codes */
typedef enum
{
  tok_error_no_error,
  tok_error_bad_input

} tok_error_t;


/* state callback type */
typedef bool ( *parser_cb_t )( void *ctx, const char *c );


typedef struct
{
  const char *beginning;
  size_t len;
  size_t indent;

} tokenizer_ctx_t;


///* set of callbacks to use when each token is parsed */
//typedef struct
//{
//  /* called when an object starts */
//  parser_cb_t on_object;

//  /* called when an object ends */
//  parser_cb_t on_object_end;

//  /* called when an array starts */
//  parser_cb_t on_array;

//  /* called when an array ends */
//  parser_cb_t on_array_end;

//  /* called when a number is read */
//  parser_cb_t on_number;

//  /* called when a string is read */
//  parser_cb_t on_string;

//  /* called when a boolean is read */
//  parser_cb_t on_boolean;

//  /* called when a boolean is read */


//} tokenizer_cbs_t;


/* Function prototypes */
tok_error_t tokenize( const char *str, const char **token, size_t *token_len );


#endif
