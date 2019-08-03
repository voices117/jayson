#ifndef PARSER_H
#define PARSER_H

#include "json_types.h"
#include "stream.h"


/** Helper macro to initilize a JSON handler. */
#define HANDLER_INIT( _ctx, \
                      _error, \
                      _object_start, \
                      _object_key, \
                      _object_end, \
                      _array_start, \
                      _array_end, \
                      _integer, \
                      _fraction, \
                      _string, \
                      _null, \
                      _boolean ) \
    { \
        .ctx = _ctx, \
        .error = _error, \
        .object_start = _object_start, \
        .object_key = _object_key, \
        .object_end = _object_end, \
        .array_start = _array_start, \
        .array_end = _array_end, \
        .integer = _integer, \
        .fraction = _fraction, \
        .string = _string, \
        .null = _null, \
        .boolean = _boolean \
    }


/** Callbacks that handle different JSON events. */
typedef struct {
    /** User defined handler context passed to every event. */
    void *ctx;
    /** Called when there's an error in the input data. */
    void ( *error )( void *ctx, const char *error_msg, int line, int column );
    /** Called when an object starts. */
    bool ( *object_start )( void *ctx );
    /** Called when an object key is found. */
    bool ( *object_key )( void *ctx, const char *key );
    /** Called when an object is closed. */
    bool ( *object_end )( void *ctx );
    /** Called when an array starts. */
    bool ( *array_start )( void *ctx );
    /** Called when an array ends. */
    bool ( *array_end )( void *ctx );
    /** Called when an integer is parsed. */
    bool ( *integer )( void *ctx, integer_t integer );
    /** Called when a fraction is parsed. */
    bool ( *fraction )( void *ctx, fraction_t fraction );
    /** Called when a string is parsed. */
    bool ( *string )( void *ctx, const char *string );
    /** Called when a null is parsed. */
    bool ( *null )( void *ctx );
    /** Called when a string is parsed. */
    bool ( *boolean )( void *ctx, bool boolean );

} json_handler_t;

/** Callback that feeds raw input to the parser. */
typedef ssize_t ( *json_read_cb_t )( void *ctx, void *data, size_t data_len );


bool json_parse( json_handler_t *handler, json_read_cb_t read_cb, void *read_cb_ctx );


#endif
