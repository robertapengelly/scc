/******************************************************************************
 * @file            report.c
 *****************************************************************************/
#include    <stdarg.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "cc.h"
#include    "ll.h"
#include    "report.h"

extern char *xstrdup (const char *__p);
unsigned long errors = 0;

#ifndef     __PDOS__
#if     defined (_WIN32)
# include   <windows.h>
static int OriginalConsoleColor = -1;
#endif

static void reset_console_color (void) {

#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)

    fprintf (stderr, "\033[0m");

#elif   defined (_WIN32)

    HANDLE hStdError = GetStdHandle (STD_ERROR_HANDLE);
    
    if (OriginalConsoleColor == -1) { return; }
    
    SetConsoleTextAttribute (hStdError, OriginalConsoleColor);
    OriginalConsoleColor = -1;

#endif

}

static void set_console_color (int color) {

#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)

    fprintf (stderr, "\033[%dm", color);

#elif   defined (_WIN32)

    HANDLE hStdError = GetStdHandle (STD_ERROR_HANDLE);
    WORD wColor;
    
    if (OriginalConsoleColor == -1) {
    
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        
        if (!GetConsoleScreenBufferInfo (hStdError, &csbi)) {
            return;
        }
        
        OriginalConsoleColor = csbi.wAttributes;
    
    }
    
    wColor = (OriginalConsoleColor & 0xF0) + (color & 0xF);
    SetConsoleTextAttribute (hStdError, wColor);

#endif

}
#endif

extern int has_include_stack (void);
extern void print_include_stack (void);

static void output_message (const char *filename, unsigned long lineno, unsigned long idx, int type, const char *fmt, va_list ap) {

    if (has_include_stack ()) {
        print_include_stack ();
    }
    
    if (filename) {
    
        if (lineno == 0) {
            fprintf (stderr, "%s: ", filename);
        } else {
            fprintf (stderr, "%s:", filename);
        }
    
    }
    
    if (lineno > 0) {
    
        if (idx == 0) {
            fprintf (stderr, "%lu: ", lineno);
        } else {
            fprintf (stderr, "%lu:", lineno);
        }
    
    }
    
    if (idx > 0) {
        fprintf (stderr, "%lu: ", idx);
    }
    
    if (type == REPORT_ERROR || type == REPORT_FATAL_ERROR) {
    
#ifndef     __PDOS__
        set_console_color (COLOR_ERROR);
#endif
        
        if (type == REPORT_ERROR) {
            fprintf (stderr, "error:");
        } else {
            fprintf (stderr, "fatal error:");
        }
    
    } else if (type == REPORT_INTERNAL_ERROR) {
    
#ifndef     __PDOS__
        set_console_color (COLOR_INTERNAL_ERROR);
#endif
        
        fprintf (stderr, "internal error:");
    
    } else if (type == REPORT_WARNING) {
    
#ifndef     __PDOS__
        set_console_color (COLOR_WARNING);
#endif
        
        fprintf (stderr, "warning:");
    
    }
    
#ifndef     __PDOS__
    reset_console_color ();
#endif
    
    fprintf (stderr, " ");
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
    
    if (type != REPORT_WARNING) {
        ++errors;
    }

}

unsigned int get_error_count (void) {
    return errors;
}

void report_at (const char *filename, unsigned long lineno, int type, const char *fmt, ...) {

    va_list ap;
    
    va_start (ap, fmt);
    output_message (filename, lineno, 0, type, fmt, ap);
    va_end (ap);
    
    if (state->max_errors > 0 && errors >= state->max_errors) {
        exit (EXIT_FAILURE);
    }

}

static char *get_actual_line (const char *filename, unsigned long lineno) {

    char *line, *line_end, *ret;
    void *load_line_internal_data;
    
    unsigned long newlines;
    unsigned long new_line_number = 1;
    
    FILE *fp;
    
    if ((fp = fopen (filename, "r"))) {
    
        load_line_internal_data = load_line_create_internal_data (&new_line_number);
        
        while (!load_line (&line, &line_end, 0, 0, &newlines, fp, &load_line_internal_data)) {
        
            if (new_line_number == lineno) {
            
                if (line[line_end - line] == '\n') {
                    line[line_end - line] = '\0';
                }
                
                ret = xstrdup (line);
                
                load_line_destroy_internal_data (load_line_internal_data);
                fclose (fp);
                
                return ret;
            
            }
            
            new_line_number += newlines + 1;
        
        }
        
        load_line_destroy_internal_data (load_line_internal_data);
        fclose (fp);
    
    }
    
    return 0;

}

void report_line_at (const char *filename, unsigned long lineno, int type, const char *str, const char *caret, const char *fmt, ...) {

    unsigned long ident = 1, idx = 0, len, i;
    va_list ap;
    
    char *actual_line = get_actual_line (filename, lineno);
    
    if (str && caret) {
        idx = (caret - str) + 1;
    }
    
    va_start (ap, fmt);
    output_message (filename, lineno, idx, type, fmt, ap);
    va_end (ap);
    
    if (str && caret) {
    
        if (actual_line) {
        
            if (lineno > 0) {
                ident = fprintf (stderr, " %8lu | ", lineno);
            } else {
                ident = fprintf (stderr, "%*s", 12, "");
            }
            
            fprintf (stderr, "%s", actual_line);
            
            if (actual_line[strlen (actual_line) - 1] != '\n') {
                fprintf (stderr, "\n");
            }
            
            len = (idx + ident) - 1;
            
            for (i = 0; i < ident; i++) {
                putc (' ', stderr);
            }
            
            for (; i < len; i++) {
                putc ('-', stderr);
            }
            
            fprintf (stderr, "^\n");
            free (actual_line);
        
        }
    
    }
    
    if (state->max_errors > 0 && errors >= state->max_errors) {
        exit (EXIT_FAILURE);
    }

}
