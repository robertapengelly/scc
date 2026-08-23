/******************************************************************************
 * @file            cc.h
 *****************************************************************************/
#ifndef     _CC_H
#define     _CC_H

#include    <stdint.h>
#include    <stdio.h>

#include    "list.h"
#include    "vector.h"

#define     WARN_ERROR                  (1U << 0)
#define     WARN_NO_RETURN              (1U << 1)
#define     WARN_SIGNEDNESS             (1U << 2)
#define     WARN_CONVERSION             (1U << 3)
#define     WARN_DUP_DEFINE             (1U << 4)
#define     WARN_UNUSED                 (1U << 5)
#define     WARN_SHADOW                 (1U << 6)
#define     WARN_UNREACHABLE            (1U << 7)
#define     WARN_FALLTHROUGH            (1U << 8)
#define     WARN_OLD_STYLE              (1U << 9)
#define     WARN_INITIALIZER            (1U << 10)

#define     WARN_DEFAULT                (WARN_DUP_DEFINE | WARN_UNREACHABLE)
#define     WARN_ALL                    (WARN_DEFAULT | WARN_NO_RETURN | WARN_SIGNEDNESS | WARN_UNUSED | WARN_FALLTHROUGH | WARN_INITIALIZER)
#define     WARN_EXTRA                  (WARN_ALL | WARN_CONVERSION | WARN_OLD_STYLE)

struct cc_state {
    
    const char *ifile, *ofile;
    FILE *ifp, *ofp;
    
    struct list *pplist;
    struct vector vec_hist;
    
    int traditional_linemarkers;
    int no_linemarkers;
    int no_leading_underscore;
    
    int pedantic;
    int mode;
    
    int bits, std;
    int long64;
    int syntax;
    int warn;
    
    unsigned long max_errors;
    long version;

};

#define     CC_MODE_COMPILE             0
#define     CC_MODE_PREPROCESS          1

extern struct cc_state *state;
extern const char *program_name;

#define     ASM_SYNTAX_MASM             (1 << 0)
#define     ASM_SYNTAX_INTEL            (1 << 1)
#define     ASM_SYNTAX_ATT              (1 << 2)
#define     ASM_SYNTAX_NASM             (1 << 3)

#endif      /* _CC_H */
