/**
 * Entrypoint for the command line tool.
 */

#include "parser.h"
#include <stdio.h>


struct handler_ctx {
    /** Level of nested elements to build the indentation. */
    int nesting_level;
};


static void _print_indentation( struct handler_ctx *ctx ) {
    for( int i = 0; i < ctx->nesting_level; i++ ) {
        printf( "  " );
    }
}


static void _default_error_handler( void *ctx, const char *error_msg, int line, int column ) {
    printf( "Error: %s at %d:%d\n", error_msg, line, column );
}
static bool _default_object_start_handler( void *ctx ) {
    _print_indentation( ctx );
    printf( "- object\n" );

    struct handler_ctx *hctx = ctx;
    hctx->nesting_level += 1;
    return true;
}
static bool _default_object_key_handler( void *ctx, const char *key ) {
    _print_indentation( ctx );
    printf( "+ %s = ", key );
    return true;
}
static bool _default_object_end_handler( void *ctx ) {
    struct handler_ctx *hctx = ctx;
    hctx->nesting_level -= 1;
    return true;
}
static bool _default_array_start_handler( void *ctx ) {
    _print_indentation( ctx );
    printf( "- array\n" );

    struct handler_ctx *hctx = ctx;
    hctx->nesting_level += 1;
    return true;
}
static bool _default_array_end_handler( void *ctx ) {
    struct handler_ctx *hctx = ctx;
    hctx->nesting_level -= 1;
    return true;
}
static bool _default_integer_handler( void *ctx, integer_t integer ) {
    _print_indentation( ctx );
    printf( "%lu\n", integer );
    return true;
}
static bool _default_fraction_handler( void *ctx, fraction_t fraction ) {
    _print_indentation( ctx );
    printf( "%f\n", fraction );
    return true;
}
static bool _default_string_handler( void *ctx, const char *string ) {
    _print_indentation( ctx );
    printf( "%s\n", string );
    return true;
}
static bool _default_null_handler( void *ctx ) {
    _print_indentation( ctx );
    printf( "%s\n", "null" );
    return true;
}
static bool _default_boolean_handler( void *ctx, bool boolean ) {
    _print_indentation( ctx );
    printf( "%s\n", boolean ? "true" : "false" );
    return true;
}


/**
 * Constructs and returns the JSON handler.
 */
static json_handler_t _get_handler( struct handler_ctx *ctx ) {
    return ( json_handler_t ) HANDLER_INIT( ctx,
                                            _default_error_handler,
                                            _default_object_start_handler,
                                            _default_object_key_handler,
                                            _default_object_end_handler,
                                            _default_array_start_handler,
                                            _default_array_end_handler,
                                            _default_integer_handler,
                                            _default_fraction_handler,
                                            _default_string_handler,
                                            _default_null_handler,
                                            _default_boolean_handler );
} 


static ssize_t _read_stdin( void *ctx, void *data, size_t data_len ) {
    return fread( data, 1, data_len, stdin );
}


int main( int argc, const char *argv[] ) {
    struct handler_ctx ctx = {
        .nesting_level = 0,
    };
    json_handler_t handler = _get_handler( &ctx );

    return( json_parse( &handler, _read_stdin, stdin ) == true );
}
