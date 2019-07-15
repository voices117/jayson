#include "stream.h"
#include <string.h>


void stream_init( stream_t *s, stream_read_cb_t in_cb, void *in_cb_ctx ) {
    memset( s, 0, sizeof( *s ) );
    s->in_cb = in_cb;
    s->in_cb_ctx = in_cb_ctx;
}

bool stream_get( stream_t *s, uint8_t *c ) {
    if( s->byte_put != 0 ) {
        *c = s->byte_put;
        s->byte_put = 0;
        return true;
    }

    if( s->finished || s->error )
        return false;

    if( s->bytes_left == 0 ) {
        ssize_t bytes_read = s->in_cb( s->in_cb_ctx, s->buffer, sizeof( s->buffer ) );
        if( bytes_read < 0 ) {
            s->error = true;
            return false;
        } else if( bytes_read == 0 ) {
            s->finished = true;
            return false;
        } else {
            s->bytes_left = bytes_read;
            s->bytes_read = 0;
        }
    }

    *c = s->buffer[s->bytes_read++];
    s->bytes_left -= 1;
    if( *c == '\n' ) {
        s->line += 1;
        s->column = 0;
    } else {
        s->column += 1;
    }
    return true;
}

bool stream_put( stream_t *s, uint8_t c ) {
    if( s->error || c == 0 )
        return false;

    /* checks if a byte was put but never consumed */
    if( s->byte_put != 0 )
        return false;

    s->byte_put = c;
    return true;
}
