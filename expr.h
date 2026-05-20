/******************************************************************************
 * @file            expr.h
 *****************************************************************************/
#ifndef     _EXPR_H
#define     _EXPR_H

#include    "int64.h"
#include    "token.h"

int expr_const (enum token_kind *k);
int64_s expr_const64 (enum token_kind *k);

#endif      /* _EXPR_H */
