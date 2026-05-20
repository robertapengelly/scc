/******************************************************************************
 * @file            pp.c
 *****************************************************************************/
#include    <ctype.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "cc.h"
#include    "eval.h"
#include    "hashtab.h"
#include    "lex.h"
#include    "lib.h"
#include    "ll.h"
#include    "macro.h"
#include    "pp.h"
#include    "report.h"
#include    "token.h"
#include    "vector.h"

static void hash_line (const char *filename, unsigned long line_number);
static int flags = 0;

static int iflevel = 0;
int ignore_line = 0;

struct vector vec_include_paths = { 0 };
struct vector vec_ifstack = { 0 };

static int handler_if (char *start, char **pp) {

    struct cond *cond;
    int ret = 1;
    
    if (!ignore_line) {
    
        cond = xmalloc (sizeof (*cond));
        
        cond->ignore_line = ignore_line;
        cond->directive = xstrdup ("if");
        
        cond->filename = xstrdup (get_filename ());
        cond->line_number = get_line_number ();
        
        vec_push (&vec_ifstack, cond);
        ret = !eval (start, pp);
        
        cond->need_else = !ret;
    
    } else {
        iflevel++;
    }
    
    return ret;

}

static int handler_ifdef (char *start, char **pp) {

    struct cond *cond;
    
    char *sname, *caret;
    int ret = 1;
    
    if (!ignore_line) {
    
        cond = xmalloc (sizeof (*cond));
        
        cond->ignore_line = ignore_line;
        cond->directive = xstrdup ("ifdef");
        
        cond->filename = xstrdup (get_filename ());
        cond->line_number = get_line_number ();
        
        vec_push (&vec_ifstack, cond);
        *pp = skip_whitespace (*pp);
        
        if (is_name_beginner ((int) **pp)) {
        
            caret = (*pp);
            
            while (is_name_part ((int) **pp)) {
                (*pp)++;
            }
            
            sname = xstrndup (caret, *pp - caret);
            ret = (find_macro (sname) == NULL);
            
            free (sname);
        
        }
        
        *pp = skip_whitespace (*pp);
        
        if (!is_end_of_line[(int) **pp]) {
            report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, *pp, "extra tokens at end of #ifdef directive");
        }
        
        cond->need_else = !ret;
    
    } else {
        iflevel++;
    }
    
    return ret;

}

static int handler_ifndef (char *start, char **pp) {

    struct cond *cond;
    
    char *sname, *caret;
    int ret = 1;
    
    if (!ignore_line) {
    
        cond = xmalloc (sizeof (*cond));
        
        cond->ignore_line = ignore_line;
        cond->directive = xstrdup ("ifndef");
        
        cond->filename = xstrdup (get_filename ());
        cond->line_number = get_line_number ();
        
        vec_push (&vec_ifstack, cond);
        *pp = skip_whitespace (*pp);
        
        if (is_name_beginner ((int) **pp)) {
        
            caret = (*pp);
            
            while (is_name_part ((int) **pp)) {
                (*pp)++;
            }
            
            sname = xstrndup (caret, *pp - caret);
            ret = !(find_macro (sname) == NULL);
            
            free (sname);
        
        }
        
        *pp = skip_whitespace (*pp);
        
        if (!is_end_of_line[(int) **pp]) {
            report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, *pp, "extra tokens at end of #ifndef directive");
        }
        
        cond->need_else = !ret;
    
    } else {
        iflevel++;
    }
    
    return ret;

}

static int handler_elif (char *start, char **pp) {

    struct cond *cond;
    int ret = 1;
    
    if (!iflevel) {
    
        if (vec_ifstack.length == 0) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, skip_whitespace (start + 1), "#elif without #if");
            return ret;
        
        } else {
        
            cond = vec_ifstack.data[vec_ifstack.length - 1];
            
            if (cond->has_else > 0) {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, skip_whitespace (start + 1), "#elif after #else");
                return ret;
            
            }
        
        }
        
        if (ignore_line) {
            ret = !eval (start, pp);
        }
        
        cond->need_else = !ret || !ignore_line;
    
    }
    
    return ret;

}

static int handler_else (char *start, char **pp) {

    struct cond *cond;
    int ret = 1;
    
    if (!iflevel) {
    
        if (vec_ifstack.length == 0) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, skip_whitespace (start + 1), "#else without #if");
            return ret;
        
        } else {
        
            cond = vec_ifstack.data[vec_ifstack.length - 1];
            
            if (cond->has_else > 0) {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, skip_whitespace (start + 1), "#else after #else");
                return ret;
            
            }
            
            cond->has_else++;
        
        }
        
        *pp = skip_whitespace (*pp);
        
        if (!is_end_of_line[(int) **pp]) {
            report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, *pp, "extra tokens at end of #else directive");
        }
        
        ret = cond->need_else;
    
    }
    
    return ret;

}

static int handler_endif (char *start, char **pp) {

    struct cond *cond;
    int ret = 1;
    
    if (!iflevel) {
    
        if ((cond = vec_pop (&vec_ifstack))) {
        
            ret = cond->ignore_line;
            
            free (cond->filename);
            free (cond->directive);
            
            free (cond);
        
        } else {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, skip_whitespace (start + 1), "#endif without #if");
        }
        
        *pp = skip_whitespace (*pp);
        
        if (!is_end_of_line[(int) **pp]) {
            report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, *pp, "extra tokens at end of #endif directive");
        }
    
    } else {
        iflevel--;
    }
    
    return ret;

}

static struct pp_pseudo_op_entry cond_pseudo_op_table[] = {

    {   "if",           &handler_if,            },
    {   "ifdef",        &handler_ifdef          },
    {   "ifndef",       &handler_ifndef         },
    {   "elif",         &handler_elif           },
    {   "else",         &handler_else           },
    {   "endif",        &handler_endif          },
    
    {   0,              0                       }

};

static struct hashtab hashtab_cond_pseudo_ops = { 0 };
static int includes = 0;

static void install_cond_pseudo_op_table (struct pp_pseudo_op_entry *table) {

    struct pp_pseudo_op_entry *entry;
    struct hashtab_name *key;
    
    for (entry = table; entry->name; entry++) {
    
        if (hashtab_get_key (&hashtab_cond_pseudo_ops, entry->name)) {
        
            report_at (program_name, 0, REPORT_ERROR, "duplicate entry '%s'", entry->name);
            continue;
        
        }
        
        if (!(key = hashtab_alloc_name (entry->name))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for '%s'", entry->name);
            continue;
        
        }
        
        hashtab_put (&hashtab_cond_pseudo_ops, key, entry);
    
    }

}

struct pp_pseudo_op_entry *find_cond_directive (char *name) {

    struct hashtab_name *key;
    struct pp_pseudo_op_entry *entry;
    
    if ((key = hashtab_get_key (&hashtab_cond_pseudo_ops, name))) {
    
        if ((entry = hashtab_get (&hashtab_cond_pseudo_ops, key))) {
            return entry;
        }
    
    }
    
    return 0;

}


static struct hashtab hashtab_pseudo_ops = { 0 };

static int handler_define (char *start, char **pp) {

    add_macro (start, pp, 1);
    return 1;

}

static int handler_error (char *start, char **pp) {

    unsigned int len = strlen (*pp);
    char *temp, *type = "error";
    
    if ((*pp)[len - 1] == '\n') {
        (*pp)[len - 1] = '\0';
    }
    
    temp = xmalloc (1 + strlen (type) + 1 + strlen (*pp) + 1);
    sprintf (temp, "#%s %s", type, *pp);
    
    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, skip_whitespace (start + 1), "%s", temp);
    free (temp);
    
    return 1;

}

static int handler_include (char *start, char **pp) {

    const char *orig_fn = get_filename ();
    unsigned long orig_ln = get_line_number ();
    
    char *caret, *sname, ch;
    int i;
    
    char *inc_path, *tmp;
    FILE *fp;
    
    struct hashtab_name *key;
    struct macro *m;
    
    char *current_directory,*p;
    int len;
    
    if (**pp != '"' && **pp != '<') {
    
        report_line_at (orig_fn, orig_ln, REPORT_ERROR, start, *pp, "#include expects \"FILENAME\" or <FILENAME>");
        return 1;
    
    }
    
    ch = (**pp == '"' ? '"' : '>');
    caret = (*pp)++;
    
    while (!is_end_of_line[(int) **pp]) {
    
        if (**pp == ch) { break; }
        (*pp)++;
    
    }
    
    if (**pp != ch) {
    
        report_line_at (orig_fn, orig_ln, REPORT_ERROR, start, caret, "#include expects \"FILENAME\" or <FILENAME>");
        return 1;
    
    } else {
        (*pp)++;
    }
    
    sname = xstrndup (caret + 1, *pp - caret - 2);
    
    if (ch == '"') {
    
#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)
        if (sname[0] == '/') {
#else
        if ((isalpha ((int) sname[0]) && sname[1] == ':') || sname[0] == '/' || sname[0] == '\\') {
#endif
        
            if ((fp = fopen (sname, "r"))) {
            
                flags = 1;
                fclose (fp);
                
                includes++;
                preprocess_file (sname);
                
                includes--;
                
                flags = 2;
                goto end;
            
            }
            
            goto error;
        
        } else if ((p = strrchr (orig_fn, '/')) || (p = strrchr (orig_fn, '\\'))) {
        
            len = p - orig_fn;
            
            tmp = xmalloc (len + 1 + strlen (sname) + 1);
            sprintf (tmp, "%.*s/%s", len, orig_fn, sname);
            
            if ((fp = fopen (tmp, "r"))) {
            
                flags = 1;
                fclose (fp);
                
                includes++;
                preprocess_file (tmp);
                
                includes--;
                free (tmp);
                
                flags = 2;
                goto end;
            
            }
            
            free (tmp);
        
        }
        
#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)
        if (orig_fn[0] == '/') {
#else
        if ((isalpha ((int) orig_fn[0]) && orig_fn[1] == ':') || orig_fn[0] == '/' || orig_fn[0] == '\\') {
#endif
        
            if ((current_directory = get_current_directory ())) {
            
                if (strcmp (current_directory, "") == 0) {
                
                    tmp = xmalloc (strlen (sname) + 1);
                    sprintf (tmp, "%s", sname);
                
                } else {
                
                    tmp = xmalloc (strlen (current_directory) + 1 + strlen (sname) + 1);
                    sprintf (tmp, "%s/%s", current_directory, sname);
                
                }
                
                free (current_directory);
                
                if ((fp = fopen (tmp, "r"))) {
                
                    flags = 1;
                    fclose (fp);
                    
                    includes++;
                    preprocess_file (tmp);
                    
                    includes--;
                    free (tmp);
                    
                    flags = 2;
                    goto end;
                
                }
                
                free (tmp);
            
            }
        
        } else {
        
            if ((fp = fopen (sname, "r"))) {
            
                flags = 1;
                fclose (fp);
                
                includes++;
                preprocess_file (sname);
                
                includes--;
                
                flags = 2;
                goto end;
            
            }
        
        }
    
    }
    
    if (vec_include_paths.length > 0) {
    
        for (i = 0; i < vec_include_paths.length; i++) {
        
            inc_path = vec_include_paths.data[i];
            
            tmp = xmalloc (strlen (inc_path) + strlen (sname) + 1);
            sprintf (tmp, "%s%s", inc_path, sname);
            
            if ((fp = fopen (tmp, "r"))) {
            
                flags = 1;
                fclose (fp);
                
                includes++;
                preprocess_file (tmp);
                
                includes--;
                free (tmp);
                
                flags = 2;
                goto end;
            
            }
            
            free (tmp);
        
        }
    
    }
    
error:
    
    report_line_at (orig_fn, orig_ln, REPORT_ERROR, start, caret, "failed to open '%s' for reading", sname);
    
end:
    
    set_filename_and_line_number (orig_fn, orig_ln);
    
    if ((key = find_macro ("__FILE__"))) {
    
        if ((m = get_macro (key)) && m->type == MACRO_BUILTIN) {
        
            free (m->value);
            
            m->value = xmalloc (1 + strlen (orig_fn) + 2);
            sprintf (m->value, "\"%s\"", orig_fn);
        
        }
    
    }
    
    if ((key = find_macro ("__LINE__"))) {
    
        if ((m = get_macro (key)) && m->type == MACRO_BUILTIN) {
        
            free (m->value);
            
            m->value = xmalloc (16);
            sprintf (m->value, "%lu", orig_ln);
        
        }
    
    }
    
    free (sname);
    return 0;

}

static int handler_undef (char *start, char **pp) {

    remove_macro (start, pp, 1);
    return 1;

}

static int handler_warning (char *start, char **pp) {

    unsigned int len = strlen (*pp);
    char *temp, *type = "warning";
    
    if ((*pp)[len - 1] == '\n') {
        (*pp)[len - 1] = '\0';
    }
    
    temp = xmalloc (1 + strlen (type) + 1 + strlen (*pp) + 1);
    sprintf (temp, "#%s %s", type, *pp);
    
    report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, skip_whitespace (start + 1), "%s", temp);
    free (temp);
    
    return 1;

}

static struct pp_pseudo_op_entry pseudo_op_table[] = {

    {   "define",       &handler_define         },
    {   "error",        &handler_error          },
    {   "undef",        &handler_undef          },
    {   "warning",      &handler_warning        },
    
    {   0,              0                       }

};

static void install_pp_pseudo_op_table (struct pp_pseudo_op_entry *table) {

    struct pp_pseudo_op_entry *entry;
    struct hashtab_name *key;
    
    for (entry = table; entry->name; entry++) {
    
        if (hashtab_get_key (&hashtab_pseudo_ops, entry->name)) {
        
            report_at (program_name, 0, REPORT_ERROR, "duplicate entry '%s'", entry->name);
            continue;
        
        }
        
        if (!(key = hashtab_alloc_name (entry->name))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for '%s'", entry->name);
            continue;
        
        }
        
        hashtab_put (&hashtab_pseudo_ops, key, entry);
    
    }

}

struct pp_pseudo_op_entry *find_directive (char *name) {

    struct hashtab_name *key;
    struct pp_pseudo_op_entry *entry;
    
    if ((key = hashtab_get_key (&hashtab_pseudo_ops, name))) {
    
        if ((entry = hashtab_get (&hashtab_pseudo_ops, key))) {
            return entry;
        }
    
    }
    
    return 0;

}


static void init_builtin_macros (void) {

    static char *builtins[] = { "__FILE__", "__LINE__" };
    char *name;
    
    struct hashtab_name *key;
    struct macro *m;
    
    unsigned int cnt = (sizeof (builtins) / sizeof (*builtins));
    unsigned i;
    
    for (i = 0; i < cnt; i++) {
    
        name = xstrdup (builtins[i]);
        
        if ((key = hashtab_alloc_name (name))) {
        
            m = xmalloc (sizeof (*m));
            m->type = MACRO_BUILTIN;
            
            m->name = name;
            m->value = xstrdup ("");
            
            m->nargs = -1;
            push_macro (key, m);
        
        }
    
    }
    
    name = xstrdup ("__SCC__");
    
    if ((key = hashtab_alloc_name (name))) {
    
        m = xmalloc (sizeof (*m));
        m->type = MACRO_BUILTIN;
        
        m->name = name;
        m->value = "1";
        
        m->nargs = -1;
        push_macro (key, m);
    
    }
    
#if     defined (VERSION)

    name = xstrdup ("__SCC_VERSION__");
    
    if ((key = hashtab_alloc_name (name))) {
    
        m = xmalloc (sizeof (*m));
        m->type = MACRO_BUILTIN;
        
        m->name = name;
        
        m->value = xmalloc (10);
        sprintf (m->value, "%ldL", state->version);
        
        m->nargs = -1;
        push_macro (key, m);
    
    }
    
#endif
    
    if (state->std) {
    
        name = xstrdup ("__STDC__");
        
        if ((key = hashtab_alloc_name (name))) {
        
            m = xmalloc (sizeof (*m));
            m->type = MACRO_BUILTIN;
            
            m->name = name;
            m->value = "1";
            
            m->nargs = -1;
            push_macro (key, m);
        
        }
    
    }

}

static void skip_pp_quoted (char **pp, char *end) {

    char quote = **pp;
    (*pp)++;
    
    while (*pp < end) {
    
        if (**pp == '\\') {
        
            (*pp)++;
            
            if (*pp < end) {
                (*pp)++;
            }
            
            continue;
        
        }
        
        if (**pp == quote) {
        
            (*pp)++;
            break;
        
        }
        
        (*pp)++;
    
    }

}

static int line_has_unclosed_function_macro (char *line, char *line_end) {

    char *p = line, *q, *r, *sname;
    int depth;
    
    struct hashtab_name *key;
    struct macro *m;
    
    while (p < line_end) {
    
        if (*p == '"' || *p == '\'') {
        
            skip_pp_quoted (&p, line_end);
            continue;
        
        }
        
        if (!is_name_beginner ((int) *p)) {
        
            p++;
            continue;
        
        }
        
        q = p;
        sname = symname (&q);
        
        if ((key = find_macro (sname)) && (m = get_macro (key)) && m->nargs >= 0) {
        
            r = skip_whitespace (q);
            
            if (r < line_end && *r == '(') {
            
                depth = 0;
                
                while (r < line_end) {
                
                    if (*r == '"' || *r == '\'') {
                    
                        skip_pp_quoted (&r, line_end);
                        continue;
                    
                    }
                    
                    if (*r == '(') {
                        depth++;
                    } else if (*r == ')') {
                    
                        depth--;
                        
                        if (depth == 0) {
                            break;
                        }
                    
                    }
                    
                    r++;
                
                }
                
                free (sname);
                
                if (depth > 0) {
                    return 1;
                }
                
                p = r;
                continue;
            
            }
        
        }
        
        free (sname);
        p = q;
    
    }
    
    return 0;

}

static char *collect_macro_continuation (char *line, char **line_end_p, FILE *fp, void **load_line_internal_data_p, unsigned long *new_line_number_p) {

    char *buf, *next, *next_end;
    unsigned long len, next_len, cap, newlines;
    
    len = *line_end_p - line;
    cap = len + 1;
    
    buf = xmalloc (cap + 1);
    memcpy (buf, line, len);
    
    buf[len] = '\0';
    
    while (line_has_unclosed_function_macro (buf, buf + len)) {
    
        if (load_line (&next, &next_end, 0, 0, &newlines, fp, load_line_internal_data_p)) {
            break;
        }
        
        next_len = next_end - next;
        
        if (len + 1 + next_len + 1 > cap) {
        
            cap = len + 1 + next_len + 1 + 256;
            buf = xrealloc (buf, cap + 1);
        
        }
        
        buf[len++] = ' ';
        memcpy (buf + len, next, next_len);
        
        len += next_len;
        buf[len] = '\0';
        
        *new_line_number_p += newlines + 1;
    
    }
    
    *line_end_p = buf + len;
    return buf;

}

static void init_date_time_macros (void) {

    char *timep, *buf, *name;
    time_t now;
    
    struct hashtab_name *key;
    struct macro *m;
    
    time (&now);
    timep = ctime (&now);
    
    name = xstrdup ("__TIME__");
    
    if ((key = hashtab_alloc_name (name))) {
    
        buf = xmalloc (11);
        sprintf (buf, "\"%.8s\"", timep + 11);;
        
        m = xmalloc (sizeof (*m));
        m->type = MACRO_BUILTIN;
        
        m->name = name;
        m->value = buf;
        
        m->nargs = -1;
        push_macro (key, m);
    
    }
    
    name = xstrdup ("__DATE__");
    
    if ((key = hashtab_alloc_name (name))) {
    
        buf = xmalloc (14);
        sprintf (buf, "\"%.3s %.2s %.4s\"", timep + 4, timep + 8, timep + 20);
        
        m = xmalloc (sizeof (*m));
        m->type = MACRO_BUILTIN;
        
        m->name = name;
        m->value = buf;
        
        m->nargs = -1;
        push_macro (key, m);
    
    }

}

int preprocess_init (void) {

    struct list *item;
    char *opt, *nopt, *p;
    
    if (state->mode == CC_MODE_PREPROCESS) {
    
        hash_line (state->ifile, 0);
        hash_line ("<built-in>", 0);
    
    }
    
    init_builtin_macros ();
    init_date_time_macros ();
    
    if (state->mode == CC_MODE_PREPROCESS) {
        hash_line ("<command-line>", 0);
    }
    
    set_filename_and_line_number (xstrdup ("<command-line>"), 1);
    
    if (state->pplist) {
    
        item = state->pplist;
        
        do {
        
            item = item->next;
            
            if (!(opt = item->data)) {
                continue;
            }
            
            if (opt[0] != '-') {
            
                report_at (program_name, 0, REPORT_ERROR, "unrecognised option '%s'", opt);
                continue;
            
            }
            
            switch (opt[1]) {
            
                case 'D':
                
                    opt = nopt = xstrdup (opt + 2);
                    
                    if ((p = strrchr (nopt, '='))) {
                        *p++ = ' ';
                    }
                    
                    add_macro (nopt, &nopt, 0);
                    free (opt);
                    
                    break;
                
                case 'I':
                
                    vec_push (&vec_include_paths, xstrdup (opt + 2));
                    break;
                
                case 'U':
                
                    opt = nopt = xstrdup (opt + 2);
                    
                    remove_macro (nopt, &nopt, 0);
                    free (opt);
                    
                    break;
                
                default:
                
                    report_at (program_name, 0, REPORT_ERROR, "unrecognised option '%s'", opt);
                    break;
            
            }
        
        } while (item != state->pplist);
    
    }
    
    install_cond_pseudo_op_table (cond_pseudo_op_table);
    install_pp_pseudo_op_table (pseudo_op_table);
    
    return get_error_count () > 0;

}


static struct hashtab hashtab_seen_macros = { 0 };
static struct vector vector_tokens = { 0 };

struct vector *preprocess_line (char **line, char *real_start, char *real_caret, int in_macro) {

    char *caret = *line, *start;
    char *sname, ch;
    
    struct hashtab_name *key;
    struct macro *m;
    
    struct token *tok;
    
    while (!is_end_of_line[(int) *caret]) {
    
        start = caret;
        
        if (isspace ((int) *caret)) {
        
            while (isspace ((int) *caret)) {
            
                if (state->mode == CC_MODE_PREPROCESS && state->ofp) {
                    fprintf (state->ofp, " ");
                }
                
                caret++;
            
            }
            
            continue;
        
        }
        
        if (*caret == 'L' && (*(caret + 1) == '\'' || *(caret + 1) == '"' || *(caret + 1) == '\\')) {
        
            ch = *(caret + 1);
            caret += 2;
            
            while (!is_end_of_line[(int) *caret]) {
            
                if (*caret == '\\') {
                
                    caret++;
                    
                    if (!is_end_of_line[(int) *caret]) {
                        caret++;
                    }
                    
                    continue;
                
                }
                
                if (*caret == ch) { break; }
                caret++;
            
            }
            
            if (*caret != ch) {
            
                char *temp = xmalloc ((caret - start) + 2);
                sprintf (temp, "%.*s%c", (int) (caret - start), start, ch);
                
                report_line_at (get_filename (), get_line_number (), REPORT_WARNING, *line, start, "missing terminating %c character", ch);
                
                if (state->mode == CC_MODE_PREPROCESS) {
                
                    if (state->ofp) {
                        fprintf (state->ofp, "%s", temp);
                    }
                    
                    free (temp);
                    continue;
                
                }
                
                tok = xmalloc (sizeof (*tok));
                tok->kind = (ch == '"' ? TOK_LSTR : TOK_LCHAR);
                
                tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
                tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
                
                tok->ident = temp;
                tok->len = (caret - start);
                
                vec_push (&vector_tokens, tok);
                continue;
            
            }
            
            caret++;
            
            if (state->mode == CC_MODE_PREPROCESS) {
            
                if (state->ofp) {
                    fprintf (state->ofp, "%.*s", (int) (caret - start), start);
                }
                
                continue;
            
            }
            
            tok = xmalloc (sizeof (*tok));
            tok->kind = (ch == '"' ? TOK_LSTR : TOK_LCHAR);
            
            tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
            tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
            
            tok->ident = xstrndup (start, (int) (caret - start));
            tok->len = (caret - start);
            
            vec_push (&vector_tokens, tok);
            continue;
        
        }
        
        if (*caret == '"' || *caret == '\'') {
        
            ch = *caret++;
            
            while (!is_end_of_line[(int) *caret]) {
            
                if (*caret == '\\') {
                
                    caret++;
                    
                    if (!is_end_of_line[(int) *caret]) {
                        caret++;
                    }
                    
                    continue;
                
                }
                
                if (*caret == ch) { break; }
                caret++;
            
            }
            
            if (*caret != ch) {
            
                char *temp = xmalloc ((caret - start) + 2);
                sprintf (temp, "%.*s%c", (int) (caret - start), start, ch);
                
                report_line_at (get_filename (), get_line_number (), REPORT_WARNING, *line, start, "missing terminating %c character", ch);
                
                if (state->mode == CC_MODE_PREPROCESS) {
                
                    if (state->ofp) {
                        fprintf (state->ofp, "%s", temp);
                    }
                    
                    free (temp);
                    continue;
                
                }
                
                tok = xmalloc (sizeof (*tok));
                tok->kind = (ch == '"' ? TOK_PPSTR : TOK_CCHAR);
                
                tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
                tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
                
                tok->ident = temp;
                tok->len = (caret - start);
                
                vec_push (&vector_tokens, tok);
                continue;
            
            }
            
            caret++;
            
            if (state->mode == CC_MODE_PREPROCESS) {
            
                if (state->ofp) {
                    fprintf (state->ofp, "%.*s", (int) (caret - start), start);
                }
                
                continue;
            
            }
            
            tok = xmalloc (sizeof (*tok));
            tok->kind = (ch == '"' ? TOK_PPSTR : TOK_CCHAR);
            
            tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
            tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
            
            tok->ident = xstrndup (start, (int) (caret - start));
            tok->len = (caret - start);
            
            vec_push (&vector_tokens, tok);
            continue;
        
        }
        
        if (is_name_beginner ((int) *caret)) {
        
            sname = symname (&caret);
            
            if (!(key = hashtab_get_key (&hashtab_seen_macros, sname))) {
            
                char *pm;
                
                if ((key = find_macro (sname))) {
                
                    hashtab_put (&hashtab_seen_macros, key, sname);
                    
                    if ((m = get_macro (key))) {
                    
                        if (m->nargs >= 0) {
                            caret = skip_whitespace (caret);
                        }
                        
                        if ((pm = process_macro (start, &caret, m))) {
                        
                            if (m->nargs < 0 && !is_end_of_line[(int) *caret]) {
                            
                                char *p = skip_whitespace (pm);
                                char *q = p;
                                char *alias = 0;
                                char *tail = skip_whitespace (caret);
                                
                                struct hashtab_name *alias_key;
                                struct macro *alias_macro;
                                
                                if (is_name_beginner ((int) *p)) {
                                
                                    alias = symname (&q);
                                    q = skip_whitespace (q);
                                    
                                    if (is_end_of_line[(int) *q]
                                        && *tail == '('
                                        && (alias_key = find_macro (alias))
                                        && (alias_macro = get_macro (alias_key))
                                        && alias_macro->nargs >= 0) {
                                    
                                        char *combined;
                                        size_t pm_len = strlen (pm);
                                        size_t tail_len = strlen (caret);
                                        
                                        combined = xmalloc (pm_len + tail_len + 1);
                                        memcpy (combined, pm, pm_len);
                                        memcpy (combined + pm_len, caret, tail_len + 1);
                                        
                                        pm = combined;
                                        caret += tail_len;
                                    
                                    }
                                    
                                    free (alias);
                                
                                }
                            
                            }
                            
                            preprocess_line (&pm, real_start ? real_start : *line, real_caret ? real_caret : start, m->nargs >= 0 ? 2 : 1);
                        
                        }
                        
                        while (!is_end_of_line[(int) *caret]) {
                        
                            if (!isspace (*caret)) {
                                break;
                            }
                            
                            if (state->mode == CC_MODE_PREPROCESS) {
                                putc (*caret, state->ofp);
                            }
                            
                            caret++;
                        
                        }
                    
                    }
                    
                    hashtab_remove (&hashtab_seen_macros, key);
                    
                    /*while (!is_end_of_line[(int) *caret]) {
                    
                        if (isspace ((int) *caret)) {
                            break;
                        }
                        
                        if (state->mode == CC_MODE_PREPROCESS) {
                            fputc (*caret, state->ofp);
                        }
                        
                        caret++;
                    
                    }*/
                    
                    continue;
                
                }
            
            }
            
            free (sname);
            
            if (state->mode == CC_MODE_PREPROCESS) {
            
                if (state->ofp) {
                    fprintf (state->ofp, "%.*s", (int) (caret - start), start);
                }
                
                continue;
            
            }
            
            tok = xmalloc (sizeof (*tok));
            tok->kind = TOK_IDENT;
            
            tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
            tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
            
            tok->ident = xstrndup (start, (int) (caret - start));
            tok->len = (caret - start);
            
            vec_push (&vector_tokens, tok);
            continue;
        
        }
        
        if (caret[0] == '0' && tolower ((int) caret[1]) == 'x') {
        
            caret += 2;
            
            while (isxdigit ((int) *caret)) {
                caret++;
            }
            
            if (state->mode == CC_MODE_PREPROCESS) {
            
                if (state->ofp) {
                    fprintf (state->ofp, "%.*s", (int) (caret - start), start);
                }
                
                continue;
            
            }
            
            if (toupper ((int) *caret) == 'U') {
            
                caret++;
                
                if (toupper ((int) *caret) == 'L') {
                    caret++;
                }
                
                if (toupper ((int) *caret) == 'L') {
                    caret++;
                }
            
            } else if (toupper ((int) *caret) == 'L') {
            
                caret++;
                
                if (toupper ((int) *caret) == 'L') {
                    caret++;
                }
                
                if (toupper ((int) *caret) == 'U') {
                    caret++;
                }
            
            }
            
            tok = xmalloc (sizeof (*tok));
            tok->kind = TOK_PPNUM;
            
            tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
            tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
            
            tok->ident = xstrndup (start, (int) (caret - start));
            tok->len = (caret - start);
            
            vec_push (&vector_tokens, tok);
            
            if (is_name_part ((int) *caret)) {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, *line, caret, "unexpected '%c' character", *caret);
                
                while (is_name_part ((int) *caret)) {
                    caret++;
                }
            
            }
            
            continue;
        
        }
        
        if (isdigit ((int) *caret) || (caret[0] == '.' && isdigit ((int) caret[1]))) {
        
            int saw_dot = 0, saw_exp = 0;
            
            while (!is_end_of_line[(int) *caret]) {
            
                if (isdigit ((int) *caret)) {
                
                    caret++;
                    continue;
                
                }
                
                if (*caret == '.' && !saw_dot && !saw_exp) {
                
                    saw_dot = 1;
                    caret++;
                    
                    continue;
                
                }
                
                if ((*caret == 'e' || *caret == 'E' || *caret == 'p' || *caret == 'P') && !saw_exp) {
                
                    saw_exp = 1;
                    caret++;
                    
                    if (*caret == '+' || *caret == '-') {
                        caret++;
                    }
                    
                    continue;
                
                }
                
                break;
            
            }
            
            if (state->mode == CC_MODE_PREPROCESS) {
            
                if (state->ofp) {
                    fprintf (state->ofp, "%.*s", (int) (caret - start), start);
                }
                
                continue;
            
            }
            
            if (saw_dot || saw_exp) {
            
                if (tolower ((int) *caret) == 'f') {
                    caret++;
                } else if (toupper ((int) *caret) == 'L') {
                    caret++;
                }
            
            } else {
            
                if (toupper ((int) *caret) == 'U') {
                
                    caret++;
                    
                    if (toupper ((int) *caret) == 'L') {
                        caret++;
                    }
                    
                    if (toupper ((int) *caret) == 'L') {
                        caret++;
                    }
                
                } else if (toupper ((int) *caret) == 'L') {
                
                    caret++;
                    
                    if (toupper ((int) *caret) == 'L') {
                        caret++;
                    }
                    
                    if (toupper ((int) *caret) == 'U') {
                        caret++;
                    }
                
                }
            
            }
            
            tok = xmalloc (sizeof (*tok));
            tok->kind = TOK_PPNUM;
            
            tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
            tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
            
            tok->ident = xstrndup (start, (int) (caret - start));
            tok->len = (caret - start);
            
            vec_push (&vector_tokens, tok);
            
            if (is_name_part ((int) *caret)) {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, *line, caret, "unexpected '%c' character", *caret);
                
                while (is_name_part ((int) *caret)) {
                    caret++;
                }
            
            }
            
            continue;
        
        }
        
        if (ispunct ((int) *caret)) {
        
            struct punct *punct;
            
            if (state->mode == CC_MODE_PREPROCESS) {
            
                caret++;
                
                if (state->ofp) {
                    fprintf (state->ofp, "%c", *start);
                }
                
                continue;
            
            }
            
            tok = xmalloc (sizeof (*tok));
            
            if ((punct = find_punct (caret))) {
            
                tok->kind = punct->kind;
                
                tok->ident = xstrdup (punct->name);
                tok->len = strlen (punct->name);
                
                caret += tok->len;
            
            } else {
            
                tok->kind = *caret;
                
                tok->ident = xstrndup (caret, 1);;
                tok->len = 1;
                
                caret++;
            
            }
            
            tok->start = (in_macro == 2) ? *line : (real_start ? real_start : *line);
            tok->caret = (in_macro == 2) ? start : (real_caret ? real_caret : start);
            
            vec_push (&vector_tokens, tok);
            continue;
        
        }
        
        report_line_at (get_filename (), get_line_number (), REPORT_INTERNAL_ERROR, *line, caret, "Do we see this???");
        caret++;
    
    }
    
    *line = caret;
    
    if (!in_macro) {
    
        if (state->mode == CC_MODE_PREPROCESS) {
        
            if (state->ofp) {
                fputc ('\n', state->ofp);
            }
            
            return 0;
        
        }
        
        return &vector_tokens;
    
    }
    
    return 0;

}

static void hash_line (const char *filename, unsigned long line_number) {

    if (state->no_linemarkers) {
        return;
    }
    
    if (state->traditional_linemarkers) {
    
        if (line_number > 0) {
            fprintf (state->ofp, "#line %lu \"%s\"\n", line_number, filename);
        }
    
    } else {
    
        if (flags) {
        
            fprintf (state->ofp, "# %lu \"%s\" %d\n", line_number, filename, flags);
            flags = 0;
        
        } else {
            fprintf (state->ofp, "# %lu \"%s\"\n", line_number, filename);
        }
    
    }

}

void preprocess_file (const char *ifile) {

    char *line, *line_end;
    FILE *fp;
    
    char *start, *sname, *caret;
    int need_hash = 0;
    
    unsigned long newlines;
    unsigned long new_line_number;
    
    unsigned long consumed_extra;
    unsigned long old_new_line_number;
    
    struct pp_pseudo_op_entry *poe;
    void *load_line_internal_data = NULL;
    
    struct cond *cond;
    int cond_idx;
    
    struct hashtab_name *key;
    struct macro *m;
    
    unsigned long blanks = 1;
    
    if (!ifile || strcmp (ifile, "-") == 0) {
    
        set_filename (xstrdup ("<stdin>"));
        fp = stdin;
    
    } else {
    
        set_filename (xstrdup (ifile));
        
        if (!(fp = fopen (ifile, "r"))) {
        
            report_at (program_name, 0, REPORT_FATAL_ERROR, "failed to open '%s' for reading", ifile);
            return;
        
        }
    
    }
    
    hash_line (get_filename (), 1);
    
    set_line_number (0);
    new_line_number = 1;
    
    if ((key = find_macro ("__FILE__"))) {
    
        const char *filename = get_filename ();;
        
        if (filename && (m = get_macro (key)) && m->type == MACRO_BUILTIN) {
        
            free (m->value);
            
            m->value = xmalloc (1 + strlen (filename) + 2);
            sprintf (m->value, "\"%s\"", filename);
        
        }
    
    }
    
    load_line_internal_data = load_line_create_internal_data (&new_line_number);
    
    while (!load_line (&line, &line_end, 0, 0, &newlines, fp, &load_line_internal_data)) {
    
        set_line_number (new_line_number);
        new_line_number += newlines + 1;
        
        consumed_extra = newlines;
        
        if ((key = find_macro ("__LINE__"))) {
        
            if ((m = get_macro (key)) && m->type == MACRO_BUILTIN) {
            
                free (m->value);
                
                m->value = xmalloc (16);
                sprintf (m->value, "%lu", get_line_number ());
            
            }
        
        }
        
        start = line;
        caret = skip_whitespace (line);
        
        if (caret >= line_end) {
        
            if (!state->no_linemarkers) {
            
                blanks += (newlines + 1);
                need_hash = 1;
            
            }
            
            continue;
        
        }
        
        if (ignore_line && *caret != '#') {
        
            if (!state->no_linemarkers) {
            
                blanks += (newlines + 1);
                need_hash = 1;
            
            }
            
            continue;
        
        }
        
        while ((line = skip_whitespace (line)) < line_end) {
        
            caret = line;
            
            if (line[0] == '/' && line[1] == '/') {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "C++ style comments are not allowed in ISO C90");
                
                if (!state->no_linemarkers) {
                
                    blanks += (newlines + 1);
                    need_hash = 1;
                
                }
                
                break;
            
            }
            
            if (*caret == '#') {
            
                caret = (line = skip_whitespace (caret + 1));
                
                if (is_name_beginner ((int) *line)) {
                
                    while (is_name_part ((int) *line)) {
                        line++;
                    }
                    
                    sname = xstrndup (caret, line - caret);
                    line = skip_whitespace (line);
                    
                    if ((poe = find_cond_directive (sname))) {
                    
                        ignore_line = poe->handler (start, &line);
                        
                        if (!state->no_linemarkers) {
                        
                            blanks += (newlines + 1);
                            need_hash = 1;
                        
                        }
                        
                        free (sname);
                        continue;
                    
                    }
                    
                    if (!ignore_line) {
                        
                        if (strcmp (sname, "pragma") == 0) {
                        
                            caret = skip_whitespace (line);
                            free (sname);
                            
                            if (is_name_beginner ((int) *line)) {
                                
                                sname = symname (&line);
                                
                                if (strcmp (sname, "once") == 0) {
                                    
                                    free (sname);
                                    
                                    if (!state->no_linemarkers) {
                                    
                                        blanks += (newlines + 1);
                                        need_hash = 1;
                                    
                                    }
                                    
                                    if (includes) {
                                    
                                        if (is_pragma_igored (ifile)) {
                                            break;
                                        }
                                    
                                    } else {
                                        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, caret, "#pragma once in main file");
                                    }
                                    
                                    continue;
                                
                                }
                            
                            }
                            
                            caret = (line = skip_whitespace (start));
                            goto handle_line;
                        
                        }
                        
                        if (strcmp (sname, "include") == 0) {
                        
                            int ret = handler_include (start, &line);
                            free (sname);
                            
                            if (flags == 2) {
                            
                                if (!state->no_linemarkers) {
                                
                                    hash_line (get_filename (), new_line_number);
                                    
                                    need_hash = 0;
                                    blanks = 1;
                                
                                }
                            
                            } else {
                            
                                if (!state->no_linemarkers) {
                                
                                    blanks += (newlines + 1);
                                    need_hash = 1;
                                
                                }
                            
                            }
                            
                            if (ret) {
                                break;
                            }
                            
                            continue;
                        
                        }
                        
                        if ((poe = find_directive (sname))) {
                        
                            int ret = poe->handler (start, &line);
                            free (sname);
                            
                            if (!state->no_linemarkers) {
                            
                                blanks += (newlines + 1);
                                need_hash = 1;
                            
                            }
                            
                            if (ret) {
                                break;
                            }
                            
                            continue;
                        
                        }
                        
                        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "unknown preprocessor directive '#%s'", sname);
                    
                    }
                    
                    if (ignore_line && !state->no_linemarkers) {
                    
                        blanks += (newlines + 1);
                        need_hash = 1;
                    
                    }
                    
                    free (sname);
                    continue;
                
                }
                
                if (ignore_line && !state->no_linemarkers) {
                
                    blanks += (newlines + 1);
                    need_hash = 1;
                
                }
                
                if (!is_end_of_line[(int) *caret] && !ignore_line) {
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "unknown preprocessor directive '#%c'", *caret);
                }
                
                continue;
            
            }
            
        handle_line:
            
            if (!ignore_line) {
            
                if (!state->no_linemarkers && need_hash) {
                
                    if (blanks < 10) {
                    
                        while (blanks > 1) {
                        
                            putc ('\n', state->ofp);
                            blanks--;
                        
                        }
                    
                    } else {
                    
                        hash_line (get_filename (), get_line_number ());
                        blanks = 1;
                    
                    }
                    
                    need_hash = 0;
                
                }
                
                blanks = 1;
                
                if (line < line_end) {
                
                    if (line_has_unclosed_function_macro (start, line_end)) {
                    
                        old_new_line_number = new_line_number;
                        
                        start = collect_macro_continuation (start, &line_end, fp, &load_line_internal_data, &new_line_number);
                        consumed_extra += new_line_number - old_new_line_number;
                    
                    }
                    
                    preprocess_line (&start, 0, 0, 0);
                    
                    if (!state->no_linemarkers && consumed_extra) {
                    
                        blanks += consumed_extra;
                        need_hash = 1;
                    
                    }
                
                }
            
            }
            
            break;
        
        }
    
    }
    
/*end:*/
    
    load_line_destroy_internal_data (load_line_internal_data);
    
    if (!includes) {
    
        for (cond_idx = 0; cond_idx < vec_ifstack.length; cond_idx++) {
        
            cond = vec_ifstack.data[cond_idx];
            report_at (cond->filename, cond->line_number, REPORT_ERROR, "unterminated #%s statement", cond->directive);
            
            free (cond->filename);
            free (cond->directive);
            
            free (cond);
        
        }
    
    }
    
    if (fp != stdin) { fclose (fp); }

}
