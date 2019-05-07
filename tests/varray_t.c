#include "scunit.h"
#include "varray.h"


#define MAX( x, y ) ( ((x) > (y)) ? (x) : (y) )


struct test {
    char c;
    int i;
};


static size_t _next_pow_2( size_t x ) {
    x -= 1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}


TEST( int ) {
    const size_t initial_cap = 4;

    int *a;
    varray_init( a, initial_cap );

    ASSERT_EQ( 0, varray_len( a ) );
    ASSERT_EQ( initial_cap, varray_cap( a ) );

    for( size_t i = 0; i < 1500; i++ ) {
        varray_push( a, i * 2 );

        ASSERT_EQ( i * 2, a[i] );
        ASSERT_EQ( i + 1, varray_len( a ) );
        ASSERT_EQ( MAX( initial_cap, _next_pow_2( i + 1 ) ), varray_cap( a ) );
    }

    varray_release( a );
}

TEST( struct ) {
    const size_t initial_cap = 32;

    struct test *a;
    varray_init( a, initial_cap );

    ASSERT_EQ( 0, varray_len( a ) );
    ASSERT_EQ( initial_cap, varray_cap( a ) );

    for( size_t i = 0; i < 1500; i++ ) {
        struct test elem = { .c = '0' + i % 70, .i = i };
        varray_push( a, elem );

        ASSERT_EQ( i + 1, varray_len( a ) );
        ASSERT_EQ( MAX( initial_cap, _next_pow_2( i + 1 ) ), varray_cap( a ) );
    }

    for( size_t i = 0; i < varray_len( a ); i++ ) {
        char c = '0' + i % 70;
        ASSERT_EQ( c, a[i].c );
        ASSERT_EQ( i, a[i].i );
    }

    varray_release( a );
}
