/******************************************************************************
 * @file            lib.h
 *****************************************************************************/
#ifndef     _LIB_H
#define     _LIB_H

struct hist_entry {

    const char *filename;
    unsigned long line_number;

};

char *xstrdup (const char *__p);
char *xstrndup (const char *__p, unsigned long __len);

void *xmalloc (unsigned long __size);
void *xrealloc (void *__ptr, unsigned long __size);

char *skip_whitespace (char *__p);
char *symname (char **pp);

void add_include_path (const char *__p);
void dynarray_add (void *ptab, long *nb_ptr, void *data);
void parse_args (int argc, char **argv, int optind);


const char *get_filename (void);
unsigned long get_line_number (void);

void set_filename (const char *__filename);
void set_line_number (unsigned long __line_number);

void get_filename_and_line_number (const char **__filename_p, unsigned long *__line_number_p);
void set_filename_and_line_number (const char *__filename, unsigned long __line_number);


const char *get_real_filename (void);
unsigned long get_real_line_number (void);

void set_real_filename (const char *__filename);
void set_real_line_number (unsigned long __line_number);


int has_include_stack (void);
void print_include_stack (void);


char *get_current_directory (void);
int is_pragma_igored (const char *filename);

#include    <stdio.h>
FILE *open_scc_temp_file (char *name, size_t name_size);
FILE *scc_tmpfile (void);

#endif      /* _LIB_H */
