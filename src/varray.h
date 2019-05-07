#ifndef VARRAY_H_
#define VARRAY_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


typedef struct
{
    uint32_t cap;
    uint32_t len;
    uint8_t data[];
} varray_t;


#define _vasize( ptr, n ) ( sizeof( varray_t ) + sizeof( *(ptr) ) * (n) )
#define _header( ptr ) ( ( varray_t *)( ( uint8_t *)(ptr) - offsetof( varray_t, data ) ) )
#define _len( ptr ) ( _header( ptr )->len )
#define _cap( ptr ) ( _header( ptr )->cap )
#define _resize( ptr, n ) ( ptr = _varray_resize( ptr, n, sizeof( *ptr ) ) )
#define _resize_if_req( ptr ) ( ( _len( ptr ) >= _cap( ptr ) ) ? _resize( ptr, _cap( ptr ) * 2 ) : ptr )

#define varray_init( ptr, n ) ( ptr = NULL, _resize( ptr, n ), _len( ptr ) = 0 )
#define varray_release( ptr ) ( free( _header( ptr ) ), (ptr) = NULL )

#define varray_len( ptr ) ( _header( ptr )->len )
#define varray_cap( ptr ) ( _header( ptr )->cap )
#define varray_push( ptr, elem ) ( _resize_if_req( ptr ), (ptr)[varray_len( ptr )++] = (elem) )
#define varray_pop( ptr ) ( (ptr)[--varray_len( ptr )] )


static inline void *_varray_resize( void *ptr, size_t n, size_t elem_size ) {
    varray_t *va = ptr ? _header( ptr ) : NULL;
    va = ( varray_t *)realloc( va, sizeof( varray_t ) + elem_size * n );
    va->cap = n;
    return va->data;
}


#endif
