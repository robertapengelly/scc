/******************************************************************************
 * @file            cc.h
 *****************************************************************************/
#ifndef     _CC_H
#define     _CC_H

#include    <stdint.h>
#include    <stdio.h>

#include    "list.h"
#include    "vector.h"

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
