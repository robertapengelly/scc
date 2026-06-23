/******************************************************************************
 * @file            token.c
 *****************************************************************************/
#include    <ctype.h>
#include    <errno.h>
#include    <math.h>
#include    <stdlib.h>
#include    <string.h>

#include    "cc.h"
#include    "cstr.h"
#include    "hashtab.h"
#include    "int64.h"
#include    "lex.h"
#include    "lib.h"
#include    "ll.h"
#include    "macro.h"
#include    "pp.h"
#include    "report.h"
#include    "token.h"
#include    "vector.h"

static unsigned long new_line_number = 1, newlines = 0;
static char *start = 0, *line = 0;

static char *line_end = 0;
static void *load_line_internal_data = 0;

static struct vector vec_history = { 0 };
static unsigned long real_new_line_number = 1;

struct history {

    FILE *ifp;
    
    unsigned long new_line_number;
    unsigned long newlines;
    
    const char *filename;
    unsigned long line_number;
    
    char *start;
    char *line;
    
    char *line_end;
    void *load_line_internal_data;
    
    struct vector *vec_tokens;
    unsigned long real_new_line_number;

};

static struct vector *vec_tokens = 0;

static int handler_include (char *start, char **pp) {

    char *caret, *sname, ch;
    int i;
    
    char *inc_path, *tmp;
    FILE *fp;
    
    char *current_directory,*p;
    int len;
    
    struct history *history;
    
    if (**pp != '"' && **pp != '<') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, *pp, "#include expects \"FILENAME\" or <FILENAME>");
        return 1;
    
    }
    
    ch = (**pp == '"' ? '"' : '>');
    caret = (*pp)++;
    
    while (!is_end_of_line[(int) **pp]) {
    
        if (**pp == ch) { break; }
        (*pp)++;
    
    }
    
    if (**pp != ch) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "#include expects \"FILENAME\" or <FILENAME>");
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
            
                fclose (fp);
                
                history = xmalloc (sizeof (*history));
                history->ifp = state->ifp;
                
                history->new_line_number = new_line_number;
                history->newlines = newlines;
                
                history->filename = get_filename ();
                history->line_number = get_line_number ();
                
                history->start = start;
                history->line = line;
                
                history->line_end = line_end;
                history->load_line_internal_data = load_line_internal_data;
                
                history->vec_tokens = vec_tokens;
                history->real_new_line_number = real_new_line_number;
                
                if (init_tokenizer (sname)) {
                
                    free (history);
                    return 1;
                
                }
                
                vec_tokens = 0;
                
                vec_push (&vec_history, history);
                return 0;
            
            }
            
            goto error;
        
        } else if ((p = strrchr (get_filename (), '/')) || (p = strrchr (get_filename (), '\\'))) {
        
            len = p - get_filename ();
            
            tmp = xmalloc (len + 1 + strlen (sname) + 1);
            sprintf (tmp, "%.*s/%s", len, get_filename (), sname);
            
            if ((fp = fopen (tmp, "r"))) {
            
                fclose (fp);
                
                history = xmalloc (sizeof (*history));
                history->ifp = state->ifp;
                
                history->new_line_number = new_line_number;
                history->newlines = newlines;
                
                history->filename = get_filename ();
                history->line_number = get_line_number ();
                
                history->start = start;
                history->line = line;
                
                history->line_end = line_end;
                history->load_line_internal_data = load_line_internal_data;
                
                history->vec_tokens = vec_tokens;
                history->real_new_line_number = real_new_line_number;
                
                if (init_tokenizer (tmp)) {
                
                    free (history);
                    return 1;
                
                }
                
                vec_tokens = 0;
                
                vec_push (&vec_history, history);
                return 0;
            
            }
            
            free (tmp);
        
        }
        
#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)
        if (get_filename ()[0] == '/') {
#else
        if ((isalpha ((int) get_filename ()[0]) && get_filename ()[1] == ':') || get_filename ()[0] == '/' || get_filename ()[0] == '\\') {
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
                
                    fclose (fp);
                    
                    history = xmalloc (sizeof (*history));
                    history->ifp = state->ifp;
                    
                    history->new_line_number = new_line_number;
                    history->newlines = newlines;
                    
                    history->filename = get_filename ();
                    history->line_number = get_line_number ();
                    
                    history->start = start;
                    history->line = line;
                    
                    history->line_end = line_end;
                    history->load_line_internal_data = load_line_internal_data;
                    
                    history->vec_tokens = vec_tokens;
                    history->real_new_line_number = real_new_line_number;
                    
                    if (init_tokenizer (tmp)) {
                    
                        free (history);
                        return 1;
                    
                    }
                    
                    vec_tokens = 0;
                    
                    vec_push (&vec_history, history);
                    return 0;
                
                }
                
                free (tmp);
            
            }
        
        } else {
        
            if ((fp = fopen (sname, "r"))) {
            
                history = xmalloc (sizeof (*history));
                history->ifp = state->ifp;
                
                history->new_line_number = new_line_number;
                history->newlines = newlines;
                
                history->filename = get_filename ();
                history->line_number = get_line_number ();
                
                history->start = start;
                history->line = line;
                
                history->line_end = line_end;
                history->load_line_internal_data = load_line_internal_data;
                
                history->vec_tokens = vec_tokens;
                history->real_new_line_number = real_new_line_number;
                
                if (init_tokenizer (sname)) {
                
                    free (history);
                    return 1;
                
                }
                
                vec_tokens = 0;
                
                vec_push (&vec_history, history);
                return 0;
            
            }
        
        }
    
    }
    
    if (vec_include_paths.length > 0) {
    
        for (i = 0; i < vec_include_paths.length; i++) {
        
            inc_path = vec_include_paths.data[i];
            
            tmp = xmalloc (strlen (inc_path) + strlen (sname) + 1);
            sprintf (tmp, "%s%s", inc_path, sname);
            
            if ((fp = fopen (tmp, "r"))) {
            
                fclose (fp);
                
                history = xmalloc (sizeof (*history));
                history->ifp = state->ifp;
                
                history->new_line_number = new_line_number;
                history->newlines = newlines;
                
                history->filename = get_filename ();
                history->line_number = get_line_number ();
                
                history->start = start;
                history->line = line;
                
                history->line_end = line_end;
                history->load_line_internal_data = load_line_internal_data;
                
                history->vec_tokens = vec_tokens;
                history->real_new_line_number = real_new_line_number;
                
                if (init_tokenizer (tmp)) {
                
                    free (history);
                    return 1;
                
                }
                
                vec_tokens = 0;
                
                vec_push (&vec_history, history);
                return 0;
            
            }
            
            free (tmp);
        
        }
    
    }
    
error:
    
    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "failed to open '%s' for reading", sname);
    return 0;

}

static int get_line (void) {

    const char *filename;
    unsigned long line_number = 0, flags, ln;
    
    char *saved_line, *caret, *temp, *name;
    int ch, ret;
    
    struct pp_pseudo_op_entry *poe;
    struct hashtab_name *key;
    struct macro *m;
    
    if (!load_line_internal_data || !state->ifp) {
        return 0;
    }
    
    while (!load_line (&line, &line_end, 0, 0, &newlines, state->ifp, &load_line_internal_data)) {
    
        set_real_line_number (real_new_line_number);
        set_line_number (new_line_number);
        
        if ((key = find_macro ("__LINE__"))) {
        
            if ((m = get_macro (key)) && m->type == MACRO_BUILTIN) {
            
                free (m->value);
                
                m->value = xmalloc (16);
                sprintf (m->value, "%lu", new_line_number);
            
            }
        
        }
        
        real_new_line_number += newlines + 1;
        new_line_number += newlines + 1;
        
        start = saved_line = line;
        
        while (line < line_end) {
        
            caret = (line = skip_whitespace (line));
            
            if (line[0] == '/' && line[1] == '/') {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "C++ style comments are not allowed in ISO C90");
                break;
            
            }
            
            if (*line == '#') {
            
                caret = (line = skip_whitespace (line + 1));
                
                if (is_name_beginner ((int) *line)) {
                
                    name = symname (&line);
                    
                    if (strcmp (name, "line") == 0) {
                    
                        caret = (line = skip_whitespace (line));
                        free (name);
                        
                        if (!isdigit ((int) *line)) {
                        
                            report_line_at (get_real_filename (), get_real_line_number (), REPORT_ERROR, start, caret, "expected line number");
                            continue;
                        
                        }
                        
                        line_number = 0;
                        
                        while (isdigit ((int) *line)) {
                        
                            line_number = line_number * 10 + (*line - '0');
                            line++;
                        
                        }
                        
                        caret = (line = skip_whitespace (line));
                        
                        if (*line != '"') {
                        
                            report_line_at (get_real_filename (), get_real_line_number (), REPORT_ERROR, start, caret, "expected filename");
                            continue;
                        
                        }
                        
                        ch = *line++;
                        
                        while (!is_end_of_line[(int) *line]) {
                        
                            if (*line == '\\') {
                            
                                line++;
                                
                                if (*line == ch) {
                                    (line)++;
                                }
                                
                                continue;
                            
                            }
                            
                            if (*line == ch) { break; }
                            (line)++;
                        
                        }
                        
                        if (*line != ch) {
                        
                            report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, caret, "missing terminating %c character", ch);
                            
                            temp = xmalloc ((line - caret + 1));
                            sprintf (temp, "%.*s", (int) (line - caret), caret + 1);
                            
                            new_line_number = line_number;
                            set_filename (temp);
                        
                        } else {
                        
                            new_line_number = line_number;
                            set_filename (xstrndup (caret + 1, line - caret - 1));
                            
                            line++;
                        
                        }
                        
                        caret = skip_whitespace (line);
                        
                        if (!is_end_of_line[(int) *caret]) {
                            report_line_at (get_real_filename (), get_real_line_number (), REPORT_WARNING, start, caret, "extra tokens at end of #line directive");
                        }
                        
                        continue;
                    
                    }
                    
                    line = skip_whitespace (line);
                    
                    if ((poe = find_cond_directive (name))) {
                    
                        ignore_line = poe->handler (start, &line);
                        
                        free (name);
                        break;
                    
                    }
                    
                    if (!ignore_line) {
                        
                        if (strcmp (name, "pragma") == 0) {
                        
                            caret = skip_whitespace (line);
                            free (name);
                            
                            if (is_name_beginner ((int) *line)) {
                                
                                name = symname (&line);
                                
                                if (strcmp (name, "once") == 0) {
                                    
                                    free (name);
                                    
                                    if (vec_history.length > 0) {
                                    
                                        if (is_pragma_igored (get_real_filename ())) {
                                            goto cleanup;
                                        }
                                    
                                    } else {
                                        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, caret, "#pragma once in main file");
                                    }
                                    
                                    continue;
                                
                                }
                            
                            }
                            
                            caret = (line = skip_whitespace (start));
                            break;
                        
                        }
                        
                        if (strcmp (name, "include") == 0) {
                        
                            ret = handler_include (start, &line);
                            free (name);
                            
                            if (ret) {
                                break;
                            }
                            
                            continue;
                        
                        }
                        
                        if ((poe = find_directive (name))) {
                        
                            ret = poe->handler (start, &line);
                            free (name);
                            
                            if (ret) {
                                break;
                            }
                            
                            continue;
                        
                        }
                        
                        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "unknown preprocessor directive '#%s'", name);
                    
                    }
                    
                    free (name);
                    
                    while (!is_end_of_line[(int) *line]) {
                        line++;
                    }
                    
                    continue;
                
                }
                
                if (!isdigit ((int) *line)) {
                
                    report_line_at (get_real_filename (), get_real_line_number (), REPORT_ERROR, start, caret, "expected line number");
                    continue;
                
                }
                
                line_number = 0;
                
                while (isdigit ((int) *line)) {
                
                    line_number = line_number * 10 + (*line - '0');
                    line++;
                
                }
                
                caret = (line = skip_whitespace (line));
                
                if (*line != '"') {
                
                    report_line_at (get_real_filename (), get_real_line_number (), REPORT_ERROR, start, caret, "expected filename");
                    continue;
                
                }
                
                ch = *line++;
                
                while (!is_end_of_line[(int) *line]) {
                
                    if (*line == '\\') {
                    
                        line++;
                        
                        if (*line == ch) {
                            (line)++;
                        }
                        
                        continue;
                    
                    }
                    
                    if (*line == ch) { break; }
                    (line)++;
                
                }
                
                filename = get_filename ();
                ln = get_line_number ();
                
                if (*line != ch) {
                
                    report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, caret, "missing terminating %c character", ch);
                    
                    temp = xmalloc ((line - caret) + 1);
                    sprintf (temp, "%.*s", (int) (line - caret), caret + 1);
                    
                    new_line_number = line_number;
                    set_filename (temp);
                
                } else {
                
                    new_line_number = line_number;
                    set_filename (xstrndup (caret + 1, line - caret - 1));
                    
                    line++;
                
                }
                
                caret = (line = skip_whitespace (line));
                
                if (!is_end_of_line[(int) *line]) {
                
                    if (!isdigit ((int) *line)) {
                    
                        report_line_at (get_real_filename (), get_real_line_number (), REPORT_ERROR, start, caret, "expected flags");
                        continue;
                    
                    }
                    
                    flags = 0;
                    
                    while (isdigit ((int) *line)) {
                    
                        flags = flags * 10 + (*line - '0');
                        line++;
                    
                    }
                    
                    switch (flags) {
                    
                        case 1: {
                        
                            struct hist_entry *entry = xmalloc (sizeof (*entry));
                            
                            entry->filename = filename;
                            entry->line_number = ln;
                            
                            vec_push (&state->vec_hist, entry);
                            break;
                        
                        }
                        
                        case 2: {
                        
                            struct hist_entry *entry;
                            
                            if (state->vec_hist.length == 0) {
                                report_line_at (get_real_filename (), get_real_line_number (), REPORT_ERROR, start, caret, "invalid line marker flag '%d': cannot pop empty include stack", flags);
                            } else {
                            
                                entry = vec_pop (&state->vec_hist);
                                
                                if (strcmp (entry->filename, get_filename ())) {
                                
                                    report_at (get_real_filename (), get_real_line_number (), REPORT_WARNING, "file \"%s\" linemaker ignored due to incorrect nesting", get_filename ());
                                    
                                    entry->line_number += newlines;
                                    entry->line_number++;
                                    
                                    new_line_number = entry->line_number;
                                    set_filename (entry->filename);
                                    
                                    /*state->corrupted = 1;*/
                                
                                }
                                
                                free (entry);
                            
                            }
                            
                            break;
                        
                        }
                        
                        default:
                        
                            break;
                    
                    }
                
                }
                
                continue;
            
            }
            
            if (!ignore_line) {
            
                line = saved_line;
                return 1;
            
            }
            
            break;
        
        }
    
    }
    
cleanup:
    
    load_line_destroy_internal_data (load_line_internal_data);
    load_line_internal_data = 0;
    
    if (vec_history.length == 0) {
    
        struct cond *cond;
        long i;
        
        for (i = 0; i < vec_ifstack.length; i++) {
        
            cond = vec_ifstack.data[i];
            report_at (cond->filename, cond->line_number, REPORT_ERROR, "unterminated #%s statement", cond->directive);
            
            free (cond->filename);
            free (cond->directive);
            
            free (cond);
        
        }
    
    }
    
    return 0;

}

static int chrpos (char *s, int ch) {

    char *p = strchr (s, ch);
    return p ? p - s : -1;

}

#include    "stdint.h"

static int hex_char (uint64_t *out, struct token *tok, int idx) {

    const char *s = tok->ident + idx + 1;
    
    int f = 0, i = 0, h;
    uint64_t n = 0;
    
    while (*s && isxdigit ((int) *s)) {
    
        h = chrpos ("0123456789abcdef", tolower ((int) *s));
        
        f = 1;
        i++;
        
        n = n * 16 + h;
        s++;
    
    }
    
    if (!f) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok->start, tok->caret + idx - 1, "\\x used with no following hex digits");
        return i;
    
    }
    
    if (n > ((1U << 8) - 1)) {
    
        uint64_t val = n % ((1U << 8) - 1);
        
        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, tok->start, tok->caret + idx - 1, "hex escape sequence out of range");
        *out = *out * (1U << 8) + val;
    
    } else {
        *out = *out * (1U << 8) + n;
    }
    
    return i;

}

static void scan_ch (struct token *tok, int is_long) {

    const char *s = tok->ident;
    int has_warned = 0, i;
    
    int64_s i64;
    uint64_t n = 0;
    
    if (is_long) {
        s++;
    }
    
    s++;
    
    while (*s) {
    
        if (*s == '\\') {
        
            switch (*(++s)) {
            
                case 'a':
                
                    n = '\a';
                    break;
                
                case 'b':
                
                    n = '\b';
                    break;
                
                case 'f':
                
                    n = '\f';
                    break;
                
                case 'n':
                
                    n = '\n';
                    break;
                
                case 'r':
                
                    n = '\r';
                    break;
                
                case 't':
                
                    n = '\t';
                    break;
                
                case 'v':
                
                    n = '\v';
                    break;
                
                case '\\':
                
                    n = '\\';
                    break;
                
                case '"':
                
                    n = '"' | (1U << 8);
                    break;
                
                case '\'':
                
                    n = '\'';
                    break;
                
                case '0':   case '1':
                case '2':   case '3':
                case '4':   case '5':
                case '6':   case '7':
                
                    for (i = 0; isdigit ((int) *s) && *s < '8'; s++) {
                    
                        if (++i > 3) {
                            break;
                        }
                        
                        n = n * 8 + (*s - '0');
                    
                    }
                    
                    s--;
                    break;
                
                case 'x':
                
                    s += hex_char (&n, tok, s - tok->ident);
                    break;
                
                default:
                
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok->start, tok->caret + (s - tok->ident), "unknown escape sequence");
                    
                    n = ' ';
                    break;
            
            }
        
        } else {
            n = *s;
        }
        
        zext64 (&i64, 1U << (is_long ? 16 : 8));
        mul64 (&tok->val.i, i64);
        
        zext64 (&i64, n & ((1U << (is_long ? 16 : 8)) - 1));
        add64 (&tok->val.i, i64);
        
        if (*(++s) == '\'') {
            break;
        }
        
        if (!has_warned) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_WARNING, tok->start, tok->caret, "multi-character character constant");
            has_warned = 1;
        
        }
    
    }

}

/*static void scan_int (struct token *tok) {

    const char *s = tok->ident;
    int radix = 10, k;
    
    if (*s == '0') {
    
        radix = 8;
        s++;
        
        if (*s == 'x') {
        
            radix = 16;
            s++;
        
        }
    
    }
    
    while (*s != '\0' && (k = chrpos ("0123456789abcdef", tolower ((int) *s))) >= 0) {
    
        if (k >= radix) {
        
            int idx = s - tok->ident;
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok->start, tok->caret + idx, "invalid digit in integer literal");
        
        }
        
        tok->val.d = tok->val.d * radix + k;
        s++;
    
    }

}*/


int tok_ident = TOK_IDENT;

struct keyword {

    char *name;
    
    int std;
    long verison;
    
    enum token_kind kind;

};

struct punct *find_punct (char *p) {

    static struct punct kws[] = {
    
        {   "<<=",  TOK_LSHEQ       },
        {   ">>=",  TOK_RSHEQ       },
        {   "...",  TOK_ELLIPSIS    },
        
        {   "!=",   TOK_NOTEQ       },
        {   "%=",   TOK_MODEQ       },
        {   "&&",   TOK_LOGAND      },
        {   "&=",   TOK_ANDEQ       },
        {   "*=",   TOK_STAREQ      },
        {   "++",   TOK_INCR        },
        {   "+=",   TOK_PLUSEQ      },
        {   "--",   TOK_DECR        },
        {   "-=",   TOK_MINUSEQ     },
        {   "->",   TOK_ARROW       },
        {   "/=",   TOK_SLASHEQ     },
        {   "<<",   TOK_LSH         },
        {   "<=",   TOK_LTEQ        },
        {   "==",   TOK_EQEQ        },
        {   ">>",   TOK_RSH         },
        {   ">=",   TOK_GTEQ        },
        {   "^=",   TOK_XOREQ       },
        {   "||",   TOK_LOGOR       },
        {   "|=",   TOK_OREQ        },
        
        {   "!",    TOK_XMARK       },
        {   "%",    TOK_MOD         },
        {   "&",    TOK_AMPER       },
        {   "(",    TOK_LPAREN      },
        {   ")",    TOK_RPAREN      },
        {   "*",    TOK_STAR        },
        {   "+",    TOK_PLUS        },
        {   ",",    TOK_COMMA       },
        {   "-",    TOK_MINUS       },
        {   ".",    TOK_DOT         },
        {   "/",    TOK_BSLASH      },
        {   ":",    TOK_COLON       },
        {   ";",    TOK_SEMI        },
        {   "<",    TOK_LESS        },
        {   "=",    TOK_ASSIGN      },
        {   ">",    TOK_GREATER     },
        {   "?",    TOK_QMARK       },
        {   "[",    TOK_LBRACK      },
        {   "]",    TOK_RBRACK      },
        {   "^",    TOK_CARET       },
        {   "{",    TOK_LBRACE      },
        {   "|",    TOK_PIPE        },
        {   "}",    TOK_RBRACE      },
        {   "~",    TOK_TILDE       }
    
    };
    
    struct punct *kw;
    unsigned long i;
    
    for (i = 0; i < (sizeof (kws) / sizeof (*kws)); i++) {
    
        kw = &kws[i];
        
        if (strncmp (p, kw->name, strlen (kw->name)) == 0) {
            return kw;
        }
    
    }
    
    return 0;

}

#ifndef     VERSION
# define        VERSION                 0L
#endif

static int find_kind (const char *start, const char *caret, const char *p) {

    static struct keyword kws[] = {
    
        /* Compiler Specific keywords */
        {   "__scc_builtin_va_arg",     -1,     VERSION,    TOK_SCC_BUILTIN_VA_ARG  },
        
        {   "__dllexport",              -1,     VERSION,    TOK_DLLEXPORT           },
        {   "__dllimport",              -1,     VERSION,    TOK_DLLIMPORT           },
        
        {   "__stdcall",                -1,     VERSION,    TOK_STDCALL             },
        {   "__packed",                 -1,     VERSION,    TOK_PACKED              },
        
        {   "__asm__",                  0,      VERSION,    TOK_ASM                 },
        {   "__inline__",               0,      VERSION,    TOK_INLINE              },
        {   "__restrict__",             0,      VERSION,    TOK_RESTRICT            },
        
        /* C90 keywords */
        {   "auto",                     90,     VERSION,    TOK_AUTO                },
        {   "break",                    90,     VERSION,    TOK_BREAK               },
        {   "case",                     90,     VERSION,    TOK_CASE                },
        {   "char",                     90,     VERSION,    TOK_CHAR                },
        {   "const",                    90,     VERSION,    TOK_CONST               },
        {   "continue",                 90,     VERSION,    TOK_CONTINUE            },
        {   "default",                  90,     VERSION,    TOK_DEFAULT             },
        {   "do",                       90,     VERSION,    TOK_DO                  },
        {   "double",                   90,     VERSION,    TOK_DOUBLE              },
        {   "else",                     90,     VERSION,    TOK_ELSE                },
        {   "enum",                     90,     VERSION,    TOK_ENUM                },
        {   "extern",                   90,     VERSION,    TOK_EXTERN              },
        {   "float",                    90,     VERSION,    TOK_FLOAT               },
        {   "for",                      90,     VERSION,    TOK_FOR                 },
        {   "goto",                     90,     VERSION,    TOK_GOTO                },
        {   "if",                       90,     VERSION,    TOK_IF                  },
        {   "int",                      90,     VERSION,    TOK_INT                 },
        {   "long",                     90,     VERSION,    TOK_LONG                },
        {   "register",                 90,     VERSION,    TOK_REGISTER            },
        {   "return",                   90,     VERSION,    TOK_RETURN              },
        {   "short",                    90,     VERSION,    TOK_SHORT               },
        {   "signed",                   90,     VERSION,    TOK_SIGNED              },
        {   "sizeof",                   90,     VERSION,    TOK_SIZEOF              },
        {   "static",                   90,     VERSION,    TOK_STATIC              },
        {   "struct",                   90,     VERSION,    TOK_STRUCT              },
        {   "switch",                   90,     VERSION,    TOK_SWITCH              },
        {   "typedef",                  90,     VERSION,    TOK_TYPEDEF             },
        {   "union",                    90,     VERSION,    TOK_UNION               },
        {   "unsigned",                 90,     VERSION,    TOK_UNSIGNED            },
        {   "void",                     90,     VERSION,    TOK_VOID                },
        {   "volatile",                 90,     VERSION,    TOK_VOLATILE            },
        {   "while",                    90,     VERSION,    TOK_WHILE               },
        
        /* C99 keywords */
        {   "inline",                   99,     VERSION,    TOK_INLINE              },
        {   "restrict",                 99,     VERSION,    TOK_RESTRICT            }
    
    };
    
    struct keyword *kw;
    unsigned long i;
    
    for (i = 0; i < (sizeof (kws) / sizeof (*kws)); i++) {
    
        kw = &kws[i];
        
        if (strcmp (p, kw->name) == 0 && kw->verison <= state->version) {
        
            if (state->std) {
            
                if (state->pedantic) {
                
                    if (!kw->std || kw->std > state->std) {
                        break;
                    }
                
                }
                
                if (kw->std) {
                
                    if (kw->std > state->std) {
                        break;
                    }
                    
                    return kw->kind;
                
                }
                
                report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, caret, "'%s' is a compiler-specific keyword so compatiblity isn't guarenteed", kw->name);
            
            }
            
            return kw->kind;
        
        }
    
    }
    
    return TOK_IDENT;

}

#define     BN_SIZE                     4

/* bn = (bn << shift) | or_val */
static int bn_lshift (unsigned int *bn, int shift, int or_val) {

    int i;
    unsigned int v;
    
    if (bn[BN_SIZE - 1] >> (32 - shift)) {
        return shift;
    }
    
    for (i = 0; i < BN_SIZE; i++) {
    
        v = bn[i];
        
        bn[i] = (v << shift) | or_val;
        or_val = v >> (32 - shift);
    
    }
    
    return 0;

}

static float scc_strtof (const char *nptr, char **endptr) {

    const char *s = nptr;
    
    float res = 0;
    float frac = 0;
    float div = 1;
    float pow10;
    
    int sign = 1;
    int exp = 0;
    int exp_sign = 1;
    int has_digits = 0;
    int i;
    
    while (isspace ((int) *s)) {
        s++;
    }
    
    if (*s == '-') {
    
        sign = -1;
        s++;
    
    } else if (*s == '+') {
        s++;
    }
    
    /* Integer part */
    while (isdigit ((int) *s)) {
    
        res = (res * 10) + (*s - '0');
        s++;
        
        has_digits = 1;
    
    }
    
    /* Fractional part */
    if (*s == '.') {
    
        s++;
        
        while (isdigit ((int) *s)) {
        
            frac = (frac * 10) + (*s - '0');
            
            div *= 10;
            s++;
            
            has_digits = 1;
        
        }
    
    }
    
    if (!has_digits) {
    
        if (endptr) {
            *endptr = (char *) nptr;
        }
        
        return 0;
    
    }
    
    res += frac / div;
    
    /* Exponent part */
    if (*s == 'e' || *s == 'E') {
    
        const char *exp_start = s;
        int exp_val = 0;
        
        s++;
        
        if (*s == '-') {
        
            exp_sign = -1;
            s++;
        
        } else if (*s == '+') {
            s++;
        }
        
        if (isdigit ((int) *s)) {
        
            while (isdigit ((int) *s)) {
            
                exp_val = (exp_val * 10) + (*s - '0');
                s++;
            
            }
            
            exp = exp_val * exp_sign;
        
        } else {
            s = exp_start;
        }
    
    }
    
    pow10 = 1;
    
    if (exp > 0) {
    
        for (i = 0; i < exp; i++) {
            pow10 *= 10;
        }
        
        res *= pow10;
    
    } else if (exp < 0) {
    
        for (i = 0; i < -exp; i++) {
            pow10 *= 10;
        }
        
        res /= pow10;
    
    }
    
    if (sign < 0) {
        res = -res;
    }
    
    if (endptr) {
        *endptr = (char *) s;
    }
    
    return res;

}

static double scc_strtod (const char *nptr, char **endptr) {

    const char *s = nptr;
    
    double x = 0;
    double frac = 0;
    double div = 1;
    double pow10;

    int neg = 0;
    int exp_neg = 0;
    int exp_val = 0;
    int saw_digit = 0;
    int i;
    
    while (isspace ((int) *s)) {
        s++;
    }
    
    if (*s == '-') {
    
        neg = 1;
        s++;
    
    } else if (*s == '+') {
        s++;
    }
    
    while (isdigit ((int) *s)) {
    
        x = (x * 10) + (*s - '0');
        s++;
        
        saw_digit = 1;
    
    }
    
    if (*s == '.') {
    
        s++;
        
        while (isdigit ((int) *s)) {
        
            frac = (frac * 10) + (*s - '0');
            
            div *= 10;
            s++;
            
            saw_digit = 1;
        
        }

    }

    if (!saw_digit) {
    
        if (endptr) {
            *endptr = (char *) nptr;
        }
        
        return 0;
    
    }
    
    x += frac / div;
    
    if (*s == 'e' || *s == 'E') {
    
        const char *exp_start = s++;
        
        if (*s == '-') {
        
            exp_neg = 1;
            s++;
        
        } else if (*s == '+') {
            s++;
        }
        
        if (isdigit ((int) *s)) {
        
            while (isdigit ((int) *s)) {
            
                if (exp_val < 308) {
                    exp_val = (exp_val * 10) + (*s - '0');
                }
                
                s++;
            
            }
        
        } else {
            s = exp_start;
        }
    
    }
    
    pow10 = 1;
    
    for (i = 0; i < exp_val; i++) {
        pow10 *= 10;
    }
    
    if (exp_val != 0) {
    
        if (exp_neg) {
            x /= pow10;
        } else {
            x *= pow10;
        }
    
    }
    
    if (neg) {
        x = -x;
    }
    
    if (endptr) {
        *endptr = (char *) s;
    }
    
    return x;

}

static void parse_number (struct token *tok) {

    const char *p = tok->ident, *q;
    
    unsigned int bn[BN_SIZE] = { 0 };
    double d;
    
    CString cstr;
    int b, t, shift, frac_bits, s, exp_val, ch;
    
    cstr_new (&cstr);
    
    t = (ch = *p++);
    ch = *p++;
    
    cstr_ccat (&cstr, t);
    b = 10;
    
    if (t == '0') {
    
        if (tolower ((int) ch) == 'x') {
        
            cstr.size--;
            
            ch = *p++;
            b = 16;
        
        }
    
    }
    
    for (;;) {
    
        if (ch >= 'a' && ch <= 'f') {
            t = ch - 'a' + 10;
        } else if (ch >= 'A' && ch <= 'F') {
            t = ch - 'A' + 10;
        } else if (isdigit ((int) ch)) {
            t = ch - '0';
        } else {
            break;
        }
        
        if (t >= b) {
            break;
        }
        
        cstr_ccat (&cstr, ch);
        ch = *p++;
    
    }
    
    if (ch == '.' || ((ch == 'e' || ch == 'E') && b == 10) || ((ch == 'p' || ch == 'P') && (b == 16 || b == 2))) {
    
        if (b != 10) {
        
            frac_bits = 0;
            cstr_ccat (&cstr, '\0');
            
            if (b == 16) {
                shift = 4;
            } else {
                shift = 1;
            }
            
            q = cstr.data;
            
            for (;;) {
            
                t = *q++;
                
                if (t == '\0') {
                    break;
                } else if (t >= 'a') {
                    t = t - 'a' + 10;
                } else if (t >= 'A') {
                    t = t - 'A' + 10;
                } else {
                    t = t - '0';
                }
                
                frac_bits -= bn_lshift (bn, shift, t);
            
            }
            
            if (ch == '.') {
            
                ch = *p++;
                
                for (;;) {
                
                    t = ch;
                    
                    if (t >= 'a' && t <= 'f') {
                        t = t - 'a' + 10;
                    } else if (t >= 'A' && t <= 'F') {
                        t = t - 'A' + 10;
                    } else if (t >= '0' && t <= '9') {
                        t = t - '0';
                    } else {
                        break;
                    }
                    
                    if (t >= b) {
                        report_at (get_filename (), get_line_number (), REPORT_ERROR, "invalid digit");
                    }
                    
                    frac_bits -= bn_lshift (bn, shift, t);
                    frac_bits += shift;
                    
                    ch = *p++;
                
                }
            
            }
            
            if (ch != 'p' && ch != 'P') {
                report_at (get_filename (), get_line_number (), REPORT_ERROR, "expected exponent");
            }
            
            ch = *p++;
            
            s = 1;
            exp_val = 0;
            
            if (ch == '+') {
                ch = *p++;
            } else if (ch == '-') {
            
                s = -1;
                ch = *p++;
            
            }
            
            if (ch < '0' || ch > '9') {
                report_at (get_filename (), get_line_number (), REPORT_ERROR, "expected exponent digits");
            }
            
            while (ch >= '0' && ch <= '9') {
            
                if (exp_val < 100000000) {
                    exp_val = exp_val * 10 + ch - '0';
                }
                
                ch = *p++;
            
            }
            
            exp_val = exp_val * s;
            d = bn[3];
            
            d = ldexp (d, 32);
            d += bn[2];
            
            d = ldexp (d, 32);
            d += bn[1];
            
            d = ldexp (d, 32);
            d += bn[0];
            
            d = ldexp (d, exp_val - frac_bits);
            t = toupper ((int) ch);
            
            if (t == 'F') {
            
                ch = *p++;
                
                tok->kind = TOK_CFLOAT;
                tok->val.f = d;
            
            } else if (t == 'L') {
            
                ch = *p++;
                
                tok->kind = TOK_CLDOUBLE;
                tok->val.ld = d;
            
            } else {
            
                tok->kind = TOK_CDOUBLE;
                tok->val.d = d;
            
            }
        
        } else {
        
            if (ch == '.') {
            
                cstr_ccat (&cstr, ch);
                ch = *p++;
                
                while (ch >= '0' && ch <= '9') {
                
                    cstr_ccat (&cstr, ch);
                    ch = *p++;
                
                }
            
            }
            
            if (ch == 'e' || ch == 'E') {
            
                cstr_ccat (&cstr, ch);
                ch = *p++;
                
                if (ch == '-' || ch == '+') {
                
                    cstr_ccat (&cstr, ch);
                    ch = *p++;
                
                }
                
                if (ch < '0' || ch > '9') {
                    report_at (get_filename (), get_line_number (), REPORT_ERROR, "expected exponent digits");
                }
                
                while (ch >= '0' && ch <= '9') {
                
                    cstr_ccat (&cstr, ch);
                    ch = *p++;
                
                }
            
            }
            
            cstr_ccat (&cstr, '\0');
            
            t = toupper ((int) ch);
            errno = 0;
            
            if (t == 'F') {
            
                ch = *p++;
                
                tok->kind = TOK_CFLOAT;
                tok->val.f = scc_strtof (cstr.data, 0);
            
            } else if (t == 'L') {
            
                ch = *p++;
                
                tok->kind = TOK_CLDOUBLE;
                tok->val.ld = scc_strtod (cstr.data, 0);
            
            } else {
            
                tok->kind = TOK_CDOUBLE;
                tok->val.d = scc_strtod (cstr.data, 0);
            
            }
        
        }
    
    } else {
    
        int64_s n = { 0 }, n1 = { 0 }, n2, n3, n4, n5;
        
        const char *p1;
        int ov = 0, lcount, ucount;
        
        cstr_ccat (&cstr, '\0');
        q = cstr.data;
        
        if (b == 10 && *q == '0') {
        
            b = 8;
            q++;
        
        }
        
        for (;;) {
        
            t = *q++;
            
            if (t == '\0') {
                break;
            }
            
            if (t >= 'a') {
                t = t - 'a' + 10;
            } else if (t >= 'A') {
                t = t - 'A' + 10;
            } else {
                t = t - '0';
            }
            
            if (t >= b) {
                report_at (get_filename (), get_line_number (), REPORT_ERROR, "invalid digit");
            }
            
            zext64 (&n2, b);
            zext64 (&n3, t);
            
            n1 = n;
            
            mul64 (&n, n2);
            add64 (&n, n3);
            
            parse_string_to_i64 (&n4, "0x1000000000000000");
            n5 = n;
            
            if (gte64_unsigned (n1, n4)) {
            
                div64 (&n5, n2);
                
                if (neq64 (n5, n1)) {
                    ov = 1;
                }
            
            }
        
        }
        
        lcount = ucount = 0;
        p1 = p;
        
        for (;;) {
        
            t = toupper ((int) ch);
            
            if (t == 'L') {
            
                if (lcount >= 2) {
                    report_at (get_filename (), get_line_number (), REPORT_ERROR, "three 'L's in integer constant");
                }
                
                if (lcount && *(p - 1) != ch) {
                    report_at (get_filename (), get_line_number (), REPORT_ERROR, "incorrect integer suffix: %s", p1);
                }
                
                lcount++;
                ch = *p++;
            
            } else if (t == 'U') {
            
                if (ucount >= 1) {
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok->start, tok->caret, "two 'U's in integer constant");
                }
                
                ucount++;
                ch = *p++;
            
            } else {
                break;
            }
        
        }
        
        /*if (pp_expr) {
            lcount = 2;
        }*/
        
        if (ucount == 0 && b == 10) {
        
            parse_string_to_i64 (&n4, "0x80000000");
            
            if (gte64_unsigned (n, n4)) {
                lcount = 1 + 1;
            }
            
            parse_string_to_i64 (&n4, " 0x8000000000000000");
            
            if (gte64_unsigned (n, n4)) {
                ov = 1, ucount = 1;
            }
        
        } else {
        
            parse_string_to_i64 (&n4, "0x100000000");
            parse_string_to_i64 (&n5, "0x80000000");
            
            if (gte64_unsigned (n, n4)) {
                lcount = 1 + 1;
            } else if (gte64_unsigned (n, n5)) {
                ucount = 1;
            }
            
            parse_string_to_i64 (&n4, " 0x8000000000000000");
            
            if (gte64_unsigned (n, n4)) {
                ucount = 1;
            }
        
        }
        
        if (ov) {
            report_at (get_filename (), get_line_number (), REPORT_WARNING, "integer constant overflow");
        }
        
        tok->kind = TOK_CINT;
        
        if (lcount) {
        
            tok->kind = (state->long64 ? TOK_CLLONG : TOK_CLONG);
            
            if (lcount == 2) {
            
                if (state->std == 90) {
                
                    const char *caret = tok->caret;
                    caret += (p1 - tok->ident);
                    
                    report_line_at (get_filename (), get_line_number (), state->pedantic ? REPORT_ERROR : REPORT_WARNING, tok->start, caret - 1, "use of C99 long long integer constant");
                
                }
                
                tok->kind = TOK_CLLONG;
            
            }
        
        }
        
        if (ucount) {
            tok->kind++;
        }
        
        tok->val.i = n;
        
    }

}

static int tok_index = 0;
struct token tok = { 0 };

void unget_token (struct token *saved_tok) {

    if (tok_index > 0) {
        tok_index--;
    }
    
    tok.val = saved_tok->val;
    tok.kind = saved_tok->kind;
    
    tok.start = saved_tok->start;
    tok.caret = saved_tok->caret;
    
    tok.ident = saved_tok->ident;
    tok.len = saved_tok->len;
    
    tok.is_unget = 1;
    free (saved_tok);

}

static void token_skip_pp_quoted (char **pp, char *end) {

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

static int token_line_has_unclosed_function_macro (char *line_start, char *end) {

    char *p = line_start, *q, *r, *sname;
    int depth;
    
    struct hashtab_name *key;
    struct macro *m;
    
    while (p < end) {
    
        if (*p == '"' || *p == '\'') {
        
            token_skip_pp_quoted (&p, end);
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
            
            if (r < end && *r == '(') {
            
                depth = 0;
                
                while (r < end) {
                
                    if (*r == '"' || *r == '\'') {
                    
                        token_skip_pp_quoted (&r, end);
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

static char *token_collect_macro_continuation (char *line_start, char **line_end_p) {

    char *buf, *next, *next_end;
    unsigned long len, next_len, cap, extra_newlines;
    
    len = *line_end_p - line_start;
    cap = len + 1;
    
    buf = xmalloc (cap + 1);
    memcpy (buf, line_start, len);
    buf[len] = '\0';
    
    while (token_line_has_unclosed_function_macro (buf, buf + len)) {
    
        if (load_line (&next, &next_end, 0, 0, &extra_newlines, state->ifp, &load_line_internal_data)) {
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
        new_line_number += extra_newlines + 1;
    
    }
    
    *line_end_p = buf + len;
    return buf;

}

void get_token (void) {

    struct history *history;
    
    struct hashtab_name *key;
    struct macro *m;
    
    if (tok.is_unget) {
    
        if (tok.start) {
            free ((char *) tok.start);
        }
    
    }
    
    if (tok.ident) { free (tok.ident); }
    memset (&tok, 0, sizeof (tok));
    
    for (;;) {
        
        if (!line || skip_whitespace (line) >= line_end) {
        
            if (vec_tokens) {
            
                struct token *tok;
                
                if (tok_index < vec_tokens->length) {
                    break;
                }
                
                while ((tok = vec_pop (vec_tokens))) {
                    free (tok);
                }
            
            }
            
            if (!get_line ()) {
            
                free ((char *) get_filename ());
                
                if ((history = vec_pop (&vec_history))) {
                
                    state->ifp = history->ifp;
                    
                    new_line_number = history->new_line_number;
                    newlines = history->newlines;
                    
                    start = history->start;
                    line = history->line;
                    
                    line_end = history->line_end;
                    load_line_internal_data = history->load_line_internal_data;
                    
                    vec_tokens = history->vec_tokens;
                    real_new_line_number = history->real_new_line_number;
                    
                    set_filename_and_line_number (history->filename, history->line_number);
                    
                    if ((key = find_macro ("__FILE__"))) {
                    
                        if ((m = get_macro (key)) && m->type == MACRO_BUILTIN) {
                        
                            free (m->value);
                            
                            m->value = xmalloc (1 + strlen (history->filename) + 2);
                            sprintf (m->value, "\"%s\"", history->filename);
                        
                        }
                    
                    }
                    
                    if ((key = find_macro ("__LINE__"))) {
                    
                        if ((m = get_macro (key)) && m->type == MACRO_BUILTIN) {
                        
                            free (m->value);
                            
                            m->value = xmalloc (16);
                            sprintf (m->value, "%lu", history->line_number);
                        
                        }
                    
                    }
                    
                    free (history);
                    continue;
                
                }
                
                return;
            
            }
            
            continue;
        
        }
        
        if (token_line_has_unclosed_function_macro (start, line_end)) {
        
            start = token_collect_macro_continuation (start, &line_end);
            line = start;
        
        }
        
        vec_tokens = preprocess_line (&line, 0, 0, 0);
        
        tok_index = 0;
        break;
    
    }
    
    if (tok_index < vec_tokens->length) {
    
        memcpy (&tok, vec_tokens->data[tok_index++], sizeof (tok));
        
        if (tok.kind == TOK_LCHAR || tok.kind == TOK_CCHAR) {
        
            scan_ch (&tok, tok.kind == TOK_LCHAR);
            return;
        
        }
        
        if (tok.kind == TOK_PPNUM) {
        
            parse_number (&tok);;
            return;
        
        }
        
        if (tok.kind == TOK_IDENT) {
        
            tok.kind = find_kind (tok.start, tok.caret, tok.ident);;
            return;
        
        }
    
    }

}

int init_tokenizer (const char *ifile) {

    const char *tmp;
    
    if (!ifile || strcmp (ifile, "-") == 0) {
    
        tmp = xstrdup ("<stdin>");
        
        set_real_filename (tmp);
        set_filename (tmp);
        
        state->ifp = stdin;
    
    } else {
    
        tmp = xstrdup (ifile);
        
        set_real_filename (tmp);
        set_filename (tmp);
        
        if (!(state->ifp = fopen (ifile, "r"))) {
        
            report_at (program_name, 0, REPORT_FATAL_ERROR, "failed to open '%s' for reading", ifile);
            return 1;
        
        }
    
    }
    
    set_real_line_number (0);
    set_line_number (0);
    
    new_line_number = 1, newlines = 0;
    start = 0, line = 0;

    line_end = 0;
    real_new_line_number = 1;
    
    load_line_internal_data = load_line_create_internal_data (&new_line_number);
    return 0;

}
