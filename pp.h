/******************************************************************************
 * @file            pp.h
 *****************************************************************************/
#ifndef     _PP_H
#define     _PP_H

struct cond {

    char *directive, *filename;
    
    int ignore_line;
    unsigned long line_number;
    
    int need_else;
    int has_else;

};

int preprocess_init (void);
void preprocess_file (const char *ifile);

struct pp_pseudo_op_entry {

    const char *name;
    int (*handler) (char *start, char **pp);

};

extern int ignore_line;

struct pp_pseudo_op_entry *find_cond_directive (char *name);
struct pp_pseudo_op_entry *find_directive (char *name);

#include    "vector.h"

extern struct vector vec_ifstack;
extern struct vector vec_include_paths;

struct vector *preprocess_line (char **line, char *real_start, char *real_caret, int in_macro);

#endif      /* _PP_H */
