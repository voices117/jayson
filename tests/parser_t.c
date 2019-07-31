#include "parser.h"
#include "scunit.h"
#include "varray.h"
#include <string.h>


#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )
#define ASIZE( x ) ( sizeof( x ) / sizeof( ( x )[0] ) )


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

#define DEFAULT_HANDLER( _ctx ) \
    HANDLER_INIT( \
        _ctx, \
        _default_error_handler, \
        _default_object_start_handler, \
        _default_object_key_handler, \
        _default_object_end_handler, \
        _default_array_start_handler, \
        _default_array_end_handler, \
        _default_integer_handler, \
        _default_fraction_handler, \
        _default_string_handler, \
        _default_boolean_handler )

#define ASSERT_EVENT_SEQUENCE( obtained_varray, ... ) \
    do { \
        enum parser_event expected[] = { __VA_ARGS__ }; \
        ASSERT_EQ( ASIZE( expected ), varray_len( obtained_varray ) ); \
        ASSERT_EQ( 0, memcmp( expected, obtained_varray, sizeof( expected ) ) ); \
    } while( 0 )


#define ASSERT_PARSED_SEQUENCE( json_cstr, ... ) \
    do { \
        struct test_handler_ctx thc = { 0 }; \
        varray_init( thc.events, 10 ); \
        json_handler_t handler = DEFAULT_HANDLER( &thc ); \
        BUFFER( json_cstr ); \
        if( json_parse( &handler, _read_from_buffer, &buffer ) ) { \
            ASSERT_EVENT_SEQUENCE( thc.events, __VA_ARGS__ ); \
        } else { \
            printf( "[ PARSE ERROR ] %s at %d:%d\n", thc.error_msg, thc.error_line, thc.error_column ); \
            ASSERT_FALSE( true ); \
        } \
        varray_release( thc.events ); \
    } while( 0 )


static ssize_t _stream_cstr_in_cb( struct buffer *b, void *data, size_t data_len ) {
    size_t bytes_to_output = MIN( data_len, b->data_len - ( b->ptr - b->data ) );
    memcpy( data, b->ptr, bytes_to_output );
    b->ptr += bytes_to_output;
    return bytes_to_output;
}

/**
 * JSON handlers
 */
enum parser_event {
    event_error,
    event_object_start,
    event_object_key,
    event_object_end,
    event_array_start,
    event_array_end,
    event_integer,
    event_fraction,
    event_string,
    event_boolean,
};

struct test_handler_ctx {
    /** Var array of obtained parser events. */
    enum parser_event *events;
    /** Obtained error message (in case an error was found) or \c NULL. */
    const char *error_msg;
    /** Expected error line (in case an error was found). */
    int error_line;
    /** Expected error column (in case an error was found). */
    int error_column;
};

static void _default_error_handler( void *ctx, const char *error_msg, int line, int column ) {
    struct test_handler_ctx *thc = ctx;

    varray_push( thc->events, event_error );
    thc->error_msg = error_msg;
    thc->error_line = line;
    thc->error_column = column;
}
static bool _default_object_start_handler( void *ctx ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_object_start );
    return true;
}
static bool _default_object_key_handler( void *ctx, const char *key ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_object_key );
    return true;
}
static bool _default_object_end_handler( void *ctx ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_object_end );
    return true;
}
static bool _default_array_start_handler( void *ctx ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_array_start );
    return true;
}
static bool _default_array_end_handler( void *ctx ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_array_end );
    return true;
}
static bool _default_integer_handler( void *ctx, integer_t integer ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_integer );
    return true;
}
static bool _default_fraction_handler( void *ctx, fraction_t fraction ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_fraction );
    return true;
}
static bool _default_string_handler( void *ctx, const char *string ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_string );
    return true;
}
static bool _default_boolean_handler( void *ctx, bool boolean ) {
    struct test_handler_ctx *thc = ctx;
    varray_push( thc->events, event_boolean );
    return true;
}

static ssize_t _read_from_buffer( void *ctx, void *data, size_t data_len ) {
    struct buffer *b = ctx;

    size_t bytes_to_output = MIN( data_len, b->data_len - ( b->ptr - b->data ) );
    memcpy( data, b->ptr, bytes_to_output );
    b->ptr += bytes_to_output;
    return bytes_to_output;
}

TEST( Invalid ) {
    
}

TEST( SimpleElements ) {
    ASSERT_PARSED_SEQUENCE( "123456", event_integer );
    ASSERT_PARSED_SEQUENCE( "\"this is just a string\"", event_string );
    ASSERT_PARSED_SEQUENCE( "1234.12345", event_fraction );
    ASSERT_PARSED_SEQUENCE( "true", event_boolean );
    ASSERT_PARSED_SEQUENCE( "false", event_boolean );
}

TEST( Object ) {
    ASSERT_PARSED_SEQUENCE( "{}",
                            event_object_start,
                            event_object_end );
    ASSERT_PARSED_SEQUENCE( "{\"string\": \"value\"}",
                            event_object_start,
                            event_object_key,
                            event_string,
                            event_object_end );
    ASSERT_PARSED_SEQUENCE( "{\"integer\": 123456}",
                            event_object_start,
                            event_object_key,
                            event_integer,
                            event_object_end );
    ASSERT_PARSED_SEQUENCE( "{\"fraction\": 3.1415}",
                            event_object_start,
                            event_object_key,
                            event_fraction,
                            event_object_end );
    ASSERT_PARSED_SEQUENCE( "{\"boolean\": false}",
                            event_object_start,
                            event_object_key,
                            event_boolean,
                            event_object_end );
    ASSERT_PARSED_SEQUENCE( "{\"boolean\": true}",
                            event_object_start,
                            event_object_key,
                            event_boolean,
                            event_object_end );
    ASSERT_PARSED_SEQUENCE( "{\"fraction\": 3.1415, \"integer\": 1024}",
                            event_object_start,
                            event_object_key,
                            event_fraction,
                            event_object_key,
                            event_integer,
                            event_object_end );
    ASSERT_PARSED_SEQUENCE( "{\"object\": {\"child\": {}}}",
                            event_object_start,
                            event_object_key,
                            event_object_start,
                            event_object_key,
                            event_object_start,
                            event_object_end,
                            event_object_end,
                            event_object_end );
}

TEST( Error ) {
    {
        struct test_handler_ctx thc = { 0 };
        varray_init( thc.events, 10 );
        json_handler_t handler = DEFAULT_HANDLER( &thc );

        BUFFER( "{}" );
        ASSERT_TRUE( json_parse( &handler, _read_from_buffer, &buffer ) );
        ASSERT_EVENT_SEQUENCE( thc.events, event_object_start, event_object_end );
        varray_release( thc.events );
    }
}
