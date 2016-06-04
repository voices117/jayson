#include "tokenizer.h"
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


int main( int argc, const char *argv[] )
{
  if( tokenize( "{\"some_key\": 123, \"some_other_key\": [\"wasted\\notherline\", true]}", NULL, 0 ) != tok_error_no_error )
    return 1;

  return 0;
}
