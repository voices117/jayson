#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef ssize_t
#define ssize_t int
#endif


/** Defines a fake anonymous \c ssize_t variable. */
#define _TMP_SSIZE_T ( *( ( ssize_t * )1 ) )

/** Compile time check that \c p is a pointer. */
#define _CHECK_PTR( p ) ( &( *p ) )

/** Helper macro to check at compile time that the callback and it's context are compatible. */
#define _CHECK_CB( cb, ctx ) ( 0 ? _TMP_SSIZE_T = cb( _CHECK_PTR( ctx ), NULL, 0 ), ( stream_read_cb_t )cb : ( stream_read_cb_t )cb )

/** Callback that feeds raw input to the stream. */
typedef ssize_t ( *stream_read_cb_t )( void *ctx, void *data, size_t data_len );

/** Stream type. */
typedef struct {
    /** \c true if there is no more input available. */
    bool finished;

    /** \c true if the input callback returned error. */
    bool error;

    /** Stream input callback. */
    stream_read_cb_t in_cb;

    /** Stream input callback context. */
    void *in_cb_ctx;

    /** Number of bytes consumed from the internal buffer. */
    size_t bytes_read;

    /** Number of bytes available in the internal buffer. */
    size_t bytes_left;

    /** A byte put in the stream (or 0 if not set). */
    uint8_t byte_put;

    /** Internal buffer that holds data obtained from the input callback. */
    uint8_t buffer[1024];
} stream_t;


#define STREAM_INIT( s, in_cb, in_cb_ctx ) stream_init( s, _CHECK_CB( in_cb, in_cb_ctx ), in_cb_ctx )

void stream_init( stream_t *s, stream_read_cb_t in_cb, void *in_cb_ctx );
bool stream_get( stream_t *s, uint8_t *c );
bool stream_put( stream_t *s, uint8_t c );

#endif
