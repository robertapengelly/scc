/******************************************************************************
 * @file            vector.h
 *****************************************************************************/
#ifndef     _VECTOR_H
#define     _VECTOR_H

struct vector {

    void **data;
    long capacity, length;

};

void vec_adjust (struct vector *vec, long length);
void vec_push (struct vector *vec, void *elem);

void *vec_pop (struct vector *vec);

#endif      /* _VECTOR_H */
