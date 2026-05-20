/******************************************************************************
 * @file            vector.c
 *****************************************************************************/
#include    <stddef.h>
#include    <stdlib.h>

#include    "vector.h"

extern void *xrealloc (void *__ptr, unsigned long __size);

void vec_adjust (struct vector *vec, long length) {

    if (vec->capacity <= length) {
    
        if (vec->capacity == 0) {
            vec->capacity = 16;
        } else {
            vec->capacity <<= 1;
        }
        
        vec->data = xrealloc (vec->data, sizeof (*(vec->data)) * vec->capacity);
    
    }

}

void *vec_pop (struct vector *vec) {

    if (!vec || !vec->length) {
        return 0;
    }
    
    return vec->data[--vec->length];

}

void vec_push (struct vector *vec, void *elem) {

    vec_adjust (vec, vec->length);
    vec->data[vec->length++] = elem;

}
