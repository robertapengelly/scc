/******************************************************************************
 * @file            cc.c
 *****************************************************************************/
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "cc.h"
#include    "expr.h"
#include    "int64.h"
#include    "lex.h"
#include    "lib.h"
#include    "parse.h"
#include    "pp.h"
#include    "report.h"
#include    "token.h"

struct cc_state *state = 0;
const char *program_name = 0;

static void cleanup (void) {

    if (state->ofp && state->ofp != stdout) { fclose (state->ofp); }
    if (state->ifp && state->ifp != stdin) { fclose (state->ifp); }
    
    if (get_error_count () > 0) {
    
        if (state->ofile) {
            remove (state->ofile);
        }
    
    }
    
    cleanup_tempfiles ();

}

int main (int argc, char **argv) {

    const char *p;
    char *root;
    
    if (argc && *argv) {
    
        program_name = *argv;
        
        if ((p = strrchr (program_name, '/')) || (p = strrchr (program_name, '\\'))) {
            program_name = (p + 1);
        }
    
    }
    
    atexit (cleanup);
    lex_init ();
    
    state = xmalloc (sizeof (*state));
    parse_args (argc, argv, 1);
    
    if (!state->ifile) {
    
        report_at (program_name, 0, REPORT_ERROR, "no input file specified");
        return EXIT_FAILURE;
    
    }
    
    if (!state->ofile) {
    
        char *ifile = (char *) state->ifile, *temp;
        
        if (strcmp (ifile, "-") == 0) {
        
            report_at (program_name, 0, REPORT_ERROR, "-o needed when input is from standard input");
            return EXIT_FAILURE;
        
        }
        
        /*if ((p = strrchr (ifile, '/')) || (p = strrchr (ifile, '\\'))) {
            ifile = (p + 1);
        }*/
        
        if ((p = strrchr (ifile, '.'))) {
        
            temp = xmalloc (p - ifile + 3);
            
            if (state->mode == CC_MODE_COMPILE) {
                sprintf (temp, "%.*s.s", (int) (p - ifile), ifile);
            } else {
                sprintf (temp, "%.*s.i", (int) (p - ifile), ifile);
            }
        
        } else {
        
            if ((p = strrchr (state->ifile, '/')) || (p = strrchr (state->ifile, '\\'))) {
            
                unsigned int len = p - state->ifile;
                
                root = xmalloc (len + 2);
                sprintf (root, "%.*s/", (int) len, state->ifile);
                
                add_include_path (root);
                free (root);
            
            }
            
            temp = xmalloc (strlen (ifile) + 3);
            
            if (state->mode == CC_MODE_COMPILE) {
                sprintf (temp, "%s.s", ifile);
            } else {
                sprintf (temp, "%s.i", ifile);
            }
        
        }
        
        state->ofile = temp;
    
    }
    
    state->ofp = stdout;
    
    if (state->ofile) {
    
        if (!(state->ofp = fopen (state->ofile, "w"))) {
            return EXIT_FAILURE;
        }
    
    }
    
    if (preprocess_init ()) {
        return EXIT_FAILURE;
    }
    
    if (state->mode == CC_MODE_PREPROCESS) {
    
        preprocess_file (state->ifile);
        return (get_error_count () > 0 ? EXIT_FAILURE : EXIT_SUCCESS);
    
    }
    
    if (init_tokenizer (state->ifile)) {
        return EXIT_FAILURE;
    }
    
    if (state->bits == 32) {
        compile_translation_unit32 ();
    } else if (state->bits == 64) {
        compile_translation_unit64 ();
    }
    
    return (get_error_count () > 0 ? EXIT_FAILURE : EXIT_SUCCESS);

}
