#include <stdlib.h>
#include <string.h>
#include "json_tokenizer.h"
#include "scunit.h"
#include "varray.h"


#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )
#define ASIZE( x ) ( sizeof( x ) / sizeof( ( x )[0] ) )


#define ASSERT_TOKEN_STR( expected_cstr, token ) \
    do { \
        ASSERT_EQ( json_token_string, ( token ).type ); \
        ASSERT_EQ( 0, strcmp( ( token ).value.string, expected_cstr ) ); \
        varray_release( ( token ).value.string ); \
    } while(0)

#define ASSERT_TOKEN_ERROR( expected_error_cstr, token ) \
    do { \
        ASSERT_EQ( json_token_error, ( token ).type ); \
        if( 0 != strcmp( ( token ).value.error_msg, expected_error_cstr ) ) {\
            printf( "Obtained: %s\n", ( token ).value.error_msg );\
            ASSERT_EQ( 0, strcmp( ( token ).value.error_msg, expected_error_cstr ) );\
        }\
    } while(0)

#define ASSERT_TOKEN_FRACTION( expected_value, token ) \
    do { \
        ASSERT_EQ( json_token_fraction, ( token ).type ); \
        ASSERT_TRUE( expected_value + 0.00001 > ( token ).value.fraction );\
        ASSERT_TRUE( expected_value - 0.00001 < ( token ).value.fraction );\
    } while(0)


struct buffer {
    const char *data;
    size_t data_len;
    const char *ptr;
};

#define BUFFER( cstr ) \
    struct buffer buffer = { \
        .data = cstr, \
        .data_len = strlen( cstr ), \
        .ptr = cstr, \
    }

#define CSTR_STREAM( var_name, cstr ) \
    BUFFER( cstr );\
    stream_t var_name; \
    STREAM_INIT( &var_name, _stream_cstr_in_cb, &buffer )


static ssize_t _stream_cstr_in_cb( struct buffer *b, void *data, size_t data_len ) {
    size_t bytes_to_output = MIN( data_len, b->data_len - ( b->ptr - b->data ) );
    memcpy( data, b->ptr, bytes_to_output );
    b->ptr += bytes_to_output;
    return bytes_to_output;
}


TEST( String ) {
    /* incomplete string */
    {
        const char *input = "\"this is a string with no closing quote";
        CSTR_STREAM( s, input );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_ERROR( "Unexpected end of file", token );

        tokenizer_release( &tokenizer );
    }
    /* empty string */
    {
        const char *input = "\"\"";
        BUFFER( input );
        
        stream_t s;
        STREAM_INIT( &s, _stream_cstr_in_cb, &buffer );
        json_token_t token;

        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_STR( "", token );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
    /* good string */
    {
        const char *input = "\"this is just a very long string\"";
        BUFFER( input );
        
        stream_t s;
        STREAM_INIT( &s, _stream_cstr_in_cb, &buffer );
        json_token_t token;

        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_STR( "this is just a very long string", token );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
    /* spaces */
    {
        const char *input = "  \n\n \t  \n   \"wrapped by spaces\"  \t  \n \n";
        BUFFER( input );
        
        stream_t s;
        STREAM_INIT( &s, _stream_cstr_in_cb, &buffer );
        json_token_t token;

        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_STR( "wrapped by spaces", token );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
    /* escapes */
    {
        CSTR_STREAM( s, " \"\\nthis string \\f\\bhas a new line \\n \\\"character\\\" \\\\\\\\ and\\t others \\r \\n\\n\\n\\n\"    \n\t" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_STR( "\nthis string \f\bhas a new line \n \"character\" \\\\ and\t others \r \n\n\n\n", token );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
    /* no control characters inside string */
    {
        CSTR_STREAM( s, " \"cannot have newline \n inside\" " );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_ERROR( "Invalid control character", token );

        tokenizer_release( &tokenizer );
    }
}


TEST( integer ) {
    /* invalid */
    {
        CSTR_STREAM( s, "123456a" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_integer, token.type );
        ASSERT_EQ( 123456, token.value.integer );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_ERROR( "Unexpected character", token );
        
        tokenizer_release( &tokenizer );
    }
    /* zero */
    {
        CSTR_STREAM( s, "0" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_integer, token.type );
        ASSERT_EQ( 0, token.value.integer );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
    /* integer */
    {
        CSTR_STREAM( s, "1234567890" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_integer, token.type );
        ASSERT_EQ( 1234567890, token.value.integer );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
    {
        CSTR_STREAM( s, "8192[" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_integer, token.type );
        ASSERT_EQ( 8192, token.value.integer );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_array_open, token.type );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
    /* spaces */
    {
        CSTR_STREAM( s, "   \n\n\n  \t  1234567890    \n\t" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_integer, token.type );
        ASSERT_EQ( 1234567890, token.value.integer );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_eof, token.type );

        tokenizer_release( &tokenizer );
    }
}


TEST( fraction ) {
    /* no decimal part */
    {
        CSTR_STREAM( s, "123." );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_ERROR( "Unexpected end of file", token );

        tokenizer_release( &tokenizer );
    }
    /* invalid decimal part */
    {
        CSTR_STREAM( s, "123.a" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_ERROR( "Unexpected character", token );

        tokenizer_release( &tokenizer );
    }
    /* invalid integer part */
    {
        CSTR_STREAM( s, "1a23.0" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        /* the first token stops when reading the 'a' */
        token = tokenizer_get_next( &tokenizer );
        ASSERT_EQ( json_token_integer, token.type );

        /* the 'a' by itself is not a valid token */
        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_ERROR( "Unexpected character", token );

        tokenizer_release( &tokenizer );
    }
    /* zero */
    {
        CSTR_STREAM( s, "0.0" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_FRACTION( 0.0, token );

        tokenizer_release( &tokenizer );
    }
    /* zero */
    {
        CSTR_STREAM( s, "0000.0000" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_FRACTION( 0.0, token );

        tokenizer_release( &tokenizer );
    }
    /* valid decimal part */
    {
        CSTR_STREAM( s, "1230.0456789" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_FRACTION( 1230.0456789, token );

        tokenizer_release( &tokenizer );
    }
    /* only decimal part */
    {
        CSTR_STREAM( s, "0.000124" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_FRACTION( 0.000124, token );

        tokenizer_release( &tokenizer );
    }
    /* zero decimal part */
    {
        CSTR_STREAM( s, "100.0" );

        json_token_t token;
        tokenizer_t tokenizer;
        tokenizer_init( &tokenizer, &s );

        token = tokenizer_get_next( &tokenizer );
        ASSERT_TOKEN_FRACTION( 100.0, token );

        tokenizer_release( &tokenizer );
    }
}


TEST( Mix ) {
    const char *input = "{, :   [\"this\", \"is\", \"a\", \n \"test\",   123, 0] }:::";
    BUFFER( input );
    
    stream_t s;
    STREAM_INIT( &s, _stream_cstr_in_cb, &buffer );
    json_token_t token;

    tokenizer_t tokenizer;
    tokenizer_init( &tokenizer, &s );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_object_open, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_comma, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_colon, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_array_open, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_TOKEN_STR( "this", token );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_comma, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_TOKEN_STR( "is", token );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_comma, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_TOKEN_STR( "a", token );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_comma, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_TOKEN_STR( "test", token );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_comma, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_integer, token.type );
    ASSERT_EQ( 123, token.value.integer );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_comma, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_integer, token.type );
    ASSERT_EQ( 0, token.value.integer );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_array_close, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_object_close, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_colon, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_colon, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_colon, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_eof, token.type );

    token = tokenizer_get_next( &tokenizer );
    ASSERT_EQ( json_token_eof, token.type );

    tokenizer_release( &tokenizer );
}
