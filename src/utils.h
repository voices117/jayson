#ifndef UTILS_H
#define UTILS_H


/**
 * This module contains general utilities required across the library.
 */


/* array size (aka length) */
#define ASIZE( array )  ( sizeof( array ) / sizeof( ( array )[ 0 ] ) )

/* converts param into a C string */
#define STR( param ) # param

/* get the minimum between "a" and "b" (or "a" if are equal) */
#define MIN( a, b )\
 ( ( ( a ) <= ( b ) ) ? ( a ) : ( b ) )

/* get the maximum between "a" and "b" (or "a" if are equal) */
#define MAX( a, b )\
 ( ( ( a ) >= ( b ) ) ? ( a ) : ( b ) )


#endif
