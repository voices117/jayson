#ifndef JSON_TYPES_H
#define JSON_TYPES_H

#include <stdbool.h>


/** Type used to represent JSON fractions. */
typedef double fraction_t;
/** Type used to represent JSON integers. */
typedef long integer_t;

#ifndef ssize_t
#define ssize_t int
#endif

#endif
