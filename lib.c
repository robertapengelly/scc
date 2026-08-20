/******************************************************************************
 * @file            lib.c
 *****************************************************************************/
#include    <ctype.h>
#include    <stdarg.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "cc.h"
#include    "lex.h"
#include    "lib.h"
#include    "report.h"
#include    "vector.h"

#if     defined (_WIN32)
# define    PATHSEP                     ';'
#else
# define    PATHSEP                     ':'
#endif

struct cc_option {

    const char *name;
    int idx, flags;

};

#define     CC_OPTION_NO_ARG            0
#define     CC_OPTION_HAS_ARG           1
#define     CC_OPTION_EQUALS_ARG        3

#define     CC_OPTION_NONE                                  0
#define     CC_OPTION_BITS32                                1
#define     CC_OPTION_BITS64                                2
#define     CC_OPTION_COMPILE                               3
#define     CC_OPTION_DEFINE                                4
#define     CC_OPTION_HELP                                  5
#define     CC_OPTION_INCLUDE                               6
#define     CC_OPTION_LEADING_UNDERSCORE                    7
#define     CC_OPTION_MASM                                  8
#define     CC_OPTION_MAX_ERRORS                            9
#define     CC_OPTION_MLONG64                               10
#define     CC_OPTION_NO_LEADING_UNDERSCORE                 12
#define     CC_OPTION_NO_LINEMARKERS                        13
#define     CC_OPTION_OUTFILE                               14
#define     CC_OPTION_PEDANTIC                              15
#define     CC_OPTION_PREPOCESS                             16
#define     CC_OPTION_STD                                   17
#define     CC_OPTION_TRANDITIONAL_LINEMARKERS              18
#define     CC_OPTION_UNDEF                                 19
#define     CC_OPTION_WARN                                  20

static struct cc_option opts[] = {

    {   "-m32",                             CC_OPTION_BITS32,                       CC_OPTION_NO_ARG        },
    {   "-m64",                             CC_OPTION_BITS64,                       CC_OPTION_NO_ARG        },
    
    {   "-pedantic",                        CC_OPTION_PEDANTIC,                     CC_OPTION_NO_ARG        },
    {   "-std",                             CC_OPTION_STD,                          CC_OPTION_EQUALS_ARG    },
    
    {   "-D",                               CC_OPTION_DEFINE,                       CC_OPTION_HAS_ARG       },
    {   "-I",                               CC_OPTION_INCLUDE,                      CC_OPTION_HAS_ARG       },
    {   "-U",                               CC_OPTION_UNDEF,                        CC_OPTION_HAS_ARG       },
    
    {   "-P",                               CC_OPTION_NO_LINEMARKERS,               CC_OPTION_NO_ARG        },
    {   "-o",                               CC_OPTION_OUTFILE,                      CC_OPTION_HAS_ARG       },
    
    {   "-E",                               CC_OPTION_PREPOCESS,                    CC_OPTION_NO_ARG        },
    {   "-S",                               CC_OPTION_COMPILE,                      CC_OPTION_NO_ARG        },
    {   "-W",                               CC_OPTION_WARN,                         CC_OPTION_HAS_ARG       },
    
    {   "-fno-leading-underscore",          CC_OPTION_NO_LEADING_UNDERSCORE,        CC_OPTION_NO_ARG        },
    {   "-fleading-underscore",             CC_OPTION_LEADING_UNDERSCORE,           CC_OPTION_NO_ARG        },
    {   "-fmax-errors",                     CC_OPTION_MAX_ERRORS,                   CC_OPTION_EQUALS_ARG    },
    {   "-mlong64",                         CC_OPTION_MLONG64,                      CC_OPTION_NO_ARG        },
    {   "-masm",                            CC_OPTION_MASM,                         CC_OPTION_EQUALS_ARG    },
    
    {   "--traditional-linemarker-format",  CC_OPTION_TRANDITIONAL_LINEMARKERS,     CC_OPTION_NO_ARG        },
    {   "--help",                           CC_OPTION_HELP,                         CC_OPTION_NO_ARG        },
    
    {   0,                                  0,                                      0                       }

};


static int strstart (const char *val, const char **str) {

    const char *p = *str;
    const char *q = val;
    
    while (*q != '\0') {
    
        if (*p != *q) {
            return 0;
        }
        
        ++p;
        ++q;
    
    }
    
    *str = p;
    return 1;

}

static void print_usage (void) {

    if (program_name) {
    
        fprintf (stderr, "Usage: %s [options] file...\n\n", program_name);
        fprintf (stderr, "Options:\n\n");
        
        fprintf (stderr, "    -Dname[=value]                    Define 'name' with value 'value'.\n");
        fprintf (stderr, "    -I DIR                            Add DIR to include search path.\n");
        fprintf (stderr, "    -Uname                            Undefine 'name'.\n");
        
        fprintf (stderr, "\n");
        fprintf (stderr, "    -E                                Preprocess only; do not compile.\n");
        fprintf (stderr, "    -S                                Compile only.\n");
        
        fprintf (stderr, "\n");
        fprintf (stderr, "    -m32                              Generate code for Intel 386.\n");
        fprintf (stderr, "    -m64                              Generate code for AMD64.\n");
        
        fprintf (stderr, "\n");
        fprintf (stderr, "    -std=<standard>                   Assume that the input sources are for\n");
        fprintf (stderr, "                                      <standard>.\n");
        fprintf (stderr, "                                          Currently only C89 and C90 are\n");
        fprintf (stderr, "                                          supported.\n");
        fprintf (stderr, "    -masm=<syntax>                    Output <syntax> compatible syntax.\n");
        fprintf (stderr, "                                          Defaults to MASM but also supports\n");
        fprintf (stderr, "                                          GNU AT&T, GNU INTEL and NASM.\n");
        fprintf (stderr, "    -fmax-errors=<count>              Stop execution after <count> errros.\n");
        fprintf (stderr, "\n");
        fprintf (stderr, "    -fno-leading-underscore           Don't prefix symbols with an underscore.\n");
        fprintf (stderr, "    -fleading-underscore              Prefix symbols with an underscore.\n");
        fprintf (stderr, "    -mlong64                          Convert long to 64-bit.\n");
        fprintf (stderr, "\n");
        fprintf (stderr, "    -P                                Don't print linemarkers.\n");
        fprintf (stderr, "    -o OBJFILE                        Name the object-file output OBJFILE.\n");
        fprintf (stderr, "\n");
        fprintf (stderr, "    -Wall                            Enable most warning messages.\n");
        fprintf (stderr, "    -Werror                          Treat all warnings as errors.\n");
        fprintf (stderr, "    -Wextra                          Print extra (possibly unwanted) warnings.\n");
        
        fprintf (stderr, "\n");
        fprintf (stderr, "    --traditional-linemarker-format   Use #line instead of short form.\n");
        fprintf (stderr, "    --help                            Print this help information.\n");
        
        fprintf (stderr, "\n");
    
    }

}


char *skip_whitespace (char *__p) {

    while (*__p == ' ' || *__p == '\t') {
        __p++;
    }
    
    return __p;

}

char *symname (char **pp) {

    char *p = *pp;
    
    if (is_name_beginner ((int) **pp)) {
    
        while (is_name_part ((int) **pp)) {
            (*pp)++;
        }
        
        return xstrndup (p, *pp - p);
    
    }
    
    return 0;

}

char *xstrdup (const char *__p) {

    char *p = xmalloc (strlen (__p) + 1);
    
    strcpy (p, __p);
    return p;

}

char *xstrndup (const char *__p, unsigned long __len) {

    char *p = xmalloc (__len + 1);
    
    strncpy (p, __p, __len);
    return p;

}

void add_include_path (const char *__p) {

    char *in = xstrdup (__p);
    char *temp = in, *p;
    
    do {
    
        for (p = temp; *p != '\0' && *p != PATHSEP; p++) {
        
            if (*p == '\\') {
                *p = '/';
            }
        
        }
        
        if ((p - temp) > 0) {
        
            unsigned int len = (p - temp);
            char *path;
            
            if (*(p - 1) != '/') {
            
                path = xmalloc (2 + (p - temp) + 2);
                sprintf (path, "-I%.*s/", len, temp);
            
            } else {
            
                path = xmalloc (2 + (p - temp) + 1);
                sprintf (path, "-I%.*s", len, temp);
            
            }
            
            list_append (&state->pplist, path);
        
        }
        
        temp = (p + 1);
    
    } while (*p != '\0');
    
    free (in);

}

void dynarray_add (void *ptab, long *nb_ptr, void *data) {

    long nb, nb_alloc;
    void **pp;
    
    nb = *nb_ptr;
    pp = *(void ***) ptab;
    
    if ((nb & (nb - 1)) == 0) {
    
        if (!nb) {
            nb_alloc = 1;
        } else {
            nb_alloc = nb * 2;
        }
        
        pp = xrealloc (pp, nb_alloc * sizeof (void *));
        *(void ***) ptab = pp;
    
    }
    
    pp[nb++] = data;
    *nb_ptr = nb;

}

void parse_args (int argc, char **argv, int optind) {

    struct cc_option *popt;
    const char *optarg, *r;
    
    if (argc <= optind) {
    
        print_usage ();
        exit (EXIT_SUCCESS);
    
    }
    
    state->warn = WARN_DEFAULT;
    state->bits = 32;
    
    while (optind < argc) {
    
        r = argv[optind++];
        
        if (r[0] != '-' || r[1] == '\0') {
        
            if (state->ifile) {
            
                report_at (program_name, 0, REPORT_ERROR, "more than one file passed as input");
                exit (EXIT_FAILURE);
            
            }
            
            state->ifile = xstrdup (r);
            continue;
        
        }
        
        for (popt = opts; ; popt++) {
        
            const char *p1 = popt->name;
            const char *r1 = r;
            
            if (!p1) {
            
                report_at (program_name, 0, REPORT_ERROR, "invalid option -- '%s'", r);
                exit (EXIT_FAILURE);
            
            }
            
            if (!strstart (p1, &r1)) {
                continue;
            }
            
            optarg = r1;
            
            if (popt->flags & CC_OPTION_HAS_ARG) {
            
                if (!(popt->flags & ~CC_OPTION_HAS_ARG)) {
                
                    if (*r1 == '\0') {
                    
                        if (optind >= argc) {
                        
                            report_at (program_name, 0, REPORT_ERROR, "argument to '%s' is missing", r);
                            exit (EXIT_FAILURE);
                        
                        }
                        
                        optarg = argv[optind++];
                    
                    }
                
                } else {
                
                    if (*r1 != '=') {
                    
                        report_at (program_name, 0, REPORT_ERROR, "argument to '%s' is missing", r);
                        exit (EXIT_FAILURE);
                    
                    }
                    
                    optarg++;
                
                }
            
            } else if (*r1 != '\0') {
                continue;
            }
            
            break;
        
        }
        
        switch (popt->idx) {
        
            case CC_OPTION_BITS32: {
            
                state->bits = 32;
                break;
            
            }
            
            case CC_OPTION_BITS64: {
            
                state->no_leading_underscore = 1;
                state->bits = 64;
                
                break;
            
            }
            
            case CC_OPTION_COMPILE: {
            
                state->mode = CC_MODE_COMPILE;
                break;
            
            }
            
            case CC_OPTION_DEFINE: {
            
                char *arg;
                
                if (!strchr (optarg, '=')) {
                
                    arg = xmalloc (2 + strlen (optarg) + 3);
                    sprintf (arg, "-D%s=1", optarg);
                
                } else {
                
                    arg = xmalloc (2 + strlen (optarg) + 1);
                    sprintf (arg, "-D%s", optarg);
                
                }
                
                list_append (&state->pplist, arg);
                break;
            
            }
            
            case CC_OPTION_HELP: {
            
                print_usage ();
                exit (EXIT_SUCCESS);
            
            }
            
            case CC_OPTION_INCLUDE: {
            
                add_include_path (optarg);
                break;
            
            }
            
            case CC_OPTION_LEADING_UNDERSCORE: {
            
                state->no_leading_underscore = 0;
                break;
            
            }
            
            case CC_OPTION_MASM: {
            
                if (strcmp (optarg, "intel") == 0) {
                
                    state->syntax = ASM_SYNTAX_INTEL;
                    break;
                
                }
                
                if (strcmp (optarg, "att") == 0) {
                
                    state->syntax = ASM_SYNTAX_ATT;
                    break;
                
                }
                
                if (strcmp (optarg, "nasm") == 0) {
                
                    state->syntax = ASM_SYNTAX_INTEL | ASM_SYNTAX_NASM;
                    break;
                
                }
                
                report_at (program_name, 0, REPORT_ERROR, "unrecognised -masm= option");
                exit (EXIT_FAILURE);
            
            }
            
            case CC_OPTION_MAX_ERRORS: {
            
                if (!(state->max_errors = atol (optarg))) {
                
                    report_at (program_name, 0, REPORT_ERROR, "invalid number for -fmax-errors=");
                    exit (EXIT_FAILURE);    
                
                }
                
                break;
            
            }
            
            case CC_OPTION_MLONG64: {
            
                state->long64 = 1;
                break;
            
            }
            
            case CC_OPTION_NO_LEADING_UNDERSCORE: {
            
                state->no_leading_underscore = 1;
                break;
            
            }
            
            case CC_OPTION_NO_LINEMARKERS: {
            
                state->no_linemarkers = 1;
                break;
            
            }
            
            case CC_OPTION_OUTFILE: {
            
                if (state->ofile) {
                
                    report_at (program_name, 0, REPORT_ERROR, "multiple output files provided");
                    exit (EXIT_FAILURE);
                
                }
                
                state->ofile = xstrdup (optarg);
                break;
            
            }
            
            case CC_OPTION_PEDANTIC: {
            
                state->pedantic = 1;
                break;
            
            }
            
            case CC_OPTION_PREPOCESS: {
            
                state->mode = CC_MODE_PREPROCESS;
                break;
            
            }
            
            case CC_OPTION_STD: {
            
                if (strcmp (optarg, "c89") == 0 || strcmp (optarg, "c90") == 0) {
                
                    state->std = 90;
                    break;
                
                }
                
                report_at (program_name, 0, REPORT_ERROR, "unrecognised -std= version");
                exit (EXIT_FAILURE);
            
            }
            
            case CC_OPTION_TRANDITIONAL_LINEMARKERS: {
            
                state->traditional_linemarkers = 1;
                break;
            
            }
            
            case CC_OPTION_UNDEF: {
            
                char *arg = xmalloc (2 + strlen (optarg) + 1);
                sprintf (arg, "-U%s", optarg);
                
                list_append (&state->pplist, arg);
                break;
            
            }
            
            case CC_OPTION_WARN: {
            
                if (strcmp (optarg, "all") == 0) {
                
                    state->warn |= WARN_ALL;
                    break;
                
                }
                
                if (strcmp (optarg, "extra") == 0) {
                
                    state->warn |= WARN_EXTRA;
                    break;
                
                }
                
                if (strcmp (optarg, "error") == 0) {
                
                    state->warn |= WARN_ERROR;
                    break;
                
                }
                
                report_at (program_name, 0, REPORT_ERROR, "unrecognised -W argument");
                exit (EXIT_FAILURE);
            
            }
            
            default: {
            
                report_at (program_name, 0, REPORT_ERROR, "unsupported option '%s'", r);
                exit (EXIT_FAILURE);
            
            }
        
        }
    
    }
    
    if (!state->syntax) {
        state->syntax = ASM_SYNTAX_MASM | ASM_SYNTAX_INTEL;
    }
    
#if     defined (VERSION)
    state->version = VERSION;
#endif

}

void *xmalloc (unsigned long __size) {

    void *ptr = malloc (__size);
    
    if (!ptr && __size) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory");
        exit (EXIT_FAILURE);
    
    }
    
    memset (ptr, 0, __size);
    return ptr;

}

void *xrealloc (void *__ptr, unsigned long __size) {

    void *ptr = realloc (__ptr, __size);
    
    if (!ptr && __size) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to reallocate memory)");
        exit (EXIT_FAILURE);
    
    }
    
    return ptr;

}


static const char *filename = 0;
static unsigned long line_number = 0;

const char *get_filename (void) {
    return filename;
}

unsigned long get_line_number (void) {
    return line_number;
}

void get_filename_and_line_number (const char **__filename_p, unsigned long *__line_number_p) {

    *__filename_p = filename;
    *__line_number_p = line_number;

}

void set_filename (const char *__filename) {
    filename = __filename;
}

void set_line_number (unsigned long __line_number) {
    line_number = __line_number;
}

void set_filename_and_line_number (const char *__filename, unsigned long __line_number) {

    filename = __filename;
    line_number = __line_number;

}


static struct vector vec_pragmas = { 0 };

#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)
# include       <sys/stat.h>
# include       <unistd.h>

# include       "hashtab.h"

struct pragma_entry { unsigned long ino; };

char *get_current_directory (void) {

    static char cwd[4096] = { 0 };
    memset (cwd, 0, sizeof (cwd));
    
    if (getcwd (cwd, sizeof (cwd))) {
        return xstrdup (cwd);
    }
    
    return 0;

}

int is_pragma_igored (const char *filename) {

    struct stat st_buf;
    
    struct pragma_entry *entry;
    int i;
    
    if (stat (filename, &st_buf) >= 0) {
    
        for (i = 0; i < vec_pragmas.length; i++) {
        
            entry = vec_pragmas.data[i];
            
            if (entry->ino == st_buf.st_ino) {
                return 1;
            }
        
        }
        
        entry = xmalloc (sizeof (*entry));
        entry->ino = st_buf.st_ino;
        
        vec_push (&vec_pragmas, entry);
    
    }
    
    return 0;

}

struct temp_file {

    char *name, *path;
    FILE *fp;

};

static struct hashtab hashtab_tmpfiles = { 0 };

FILE *scc_tmpfile (void) {

    char *temp_path, *template, *name, *p;
    
    struct hashtab_name *key;
    struct temp_file *tmp;
    
    FILE *fp;
    int fd;
    
    if (!(temp_path = getenv ("TMPDIR"))) {
        temp_path = "/tmp";
    }
    
    template = xmalloc (strlen (temp_path) + 1 + 10 + 1);
    
again:
    
    sprintf (template, "%s/sccXXXXXX", temp_path);
    
    if ((fd = mkstemp (template)) < 0) {
        return 0;
    }
    
    if ((p = strrchr (template, '/'))) {
        name = xstrdup (p + 1);
    } else {
        name = xstrdup (template);
    }
    
    if ((key = hashtab_get_key (&hashtab_tmpfiles, name))) {
    
        close (fd);
        
        remove (template);
        free (name);
        
        goto again;
    
    }
    
    if (!((key = hashtab_alloc_name (name)))) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for key");
        exit (EXIT_FAILURE);
    
    }
    
    if ((fp = fdopen (fd, "w+b"))) {
    
        tmp = xmalloc (sizeof (*tmp));
        tmp->fp = fp;
        
        tmp->path = xstrdup (template);
        tmp->name = name;
        
        hashtab_put (&hashtab_tmpfiles, key, tmp);
        
        free (template);
        return fp;
    
    }
    
    free (name);
    free (key);
    
    close (fd);
    
    remove (template);
    free (template);
    
    return 0;

}

int scc_close (FILE *fp) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if (tmp->fp == fp) {
        
            if ((ret = fclose (fp))) {
                return ret;
            }
            
            hashtab_remove (&hashtab_tmpfiles, entry->key);
            remove (tmp->path);
            
            free (tmp->path);
            free (tmp->name);
            
            free (tmp);
            return 0;
        
        }
    
    }
    
    return 0;

}

void cleanup_tempfiles (void) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if ((ret = fclose (tmp->fp))) {
            continue;
        }
        
        remove (tmp->path);
    
    }

}
#elif       defined (_WIN32)
# include       <windows.h>

# ifndef    MAX_PATH
#  define       MAX_PATH                206
# endif

# include       "hashtab.h"

struct pragma_entry {

    DWORD    dwVolumeSerialNumber;
    
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;

};

char *get_current_directory (void) {

    static TCHAR tszBuffer[4096];
    size_t i;
    
    DWORD dwRet;
    
    if ((dwRet = GetCurrentDirectory (sizeof (tszBuffer), tszBuffer)) == 0) {
        return 0;
    }
    
    for (i = 0; i < strlen (tszBuffer); i++) {
    
        if (tszBuffer[i] == '\\') {
            tszBuffer[i] = '/';
        }
    
    }
    
    return xstrdup (tszBuffer);

}

int is_pragma_igored (const char *filename) {

    HANDLE handle;
    
    struct pragma_entry *entry;
    int i;
    
    BY_HANDLE_FILE_INFORMATION file_info;
    
    if ((handle = CreateFile (filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    if (GetFileInformationByHandle (handle, &file_info)) {
    
        for (i = 0; i < vec_pragmas.length; i++) {
        
            entry = vec_pragmas.data[i];
            
            if (entry->dwVolumeSerialNumber == file_info.dwVolumeSerialNumber) {
            
                if (entry->nFileSizeHigh == file_info.nFileSizeHigh && entry->nFileSizeLow == file_info.nFileSizeLow) {
                
                    CloseHandle (handle);
                    return 1;
                
                }
            
            }
        
        }
        
        entry = xmalloc (sizeof (*entry));
        
        entry->dwVolumeSerialNumber = file_info.dwVolumeSerialNumber;
        entry->nFileSizeHigh = file_info.nFileSizeHigh;
        entry->nFileSizeLow = file_info.nFileSizeLow;
        
        vec_push (&vec_pragmas, entry);
    
    }
    
    CloseHandle (handle);
    return 0;

}

struct temp_file {

    char *name, *path;
    FILE *fp;

};

static struct hashtab hashtab_tmpfiles = { 0 };

FILE *scc_tmpfile (void) {

    char temp_path[MAX_PATH + 1] = { 0 };
    char full_path[MAX_PATH + 1] = { 0 };
    
    struct hashtab_name *key;
    struct temp_file *tmp;
    
    char *name, *p;
    FILE *fp;
    
    if (!GetTempPath (MAX_PATH + 1, temp_path)) {
        return 0;
    }
    
again:
    
    if (!GetTempFileName (temp_path, "scc", 0, full_path)) {
        return 0;
    }
    
    if ((p = strrchr (full_path, '\\'))) {
        name = xstrdup (p + 1);
    } else {
        name = xstrdup (full_path);
    }
    
    if ((key = hashtab_get_key (&hashtab_tmpfiles, name))) {
    
        free (name);
        goto again;
    
    }
    
    if (!((key = hashtab_alloc_name (name)))) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for key");
        exit (EXIT_FAILURE);
    
    }
    
    if ((fp = fopen (full_path, "w+b"))) {
    
        tmp = xmalloc (sizeof (*tmp));
        tmp->fp = fp;
        
        tmp->path = xstrdup (full_path);
        tmp->name = name;
        
        hashtab_put (&hashtab_tmpfiles, key, tmp);
        return fp;
    
    }
    
    free (name);
    free (key);
    
    return 0;

}

int scc_close (FILE *fp) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if (tmp->fp == fp) {
        
            if ((ret = fclose (fp))) {
                return ret;
            }
            
            hashtab_remove (&hashtab_tmpfiles, entry->key);
            remove (tmp->path);
            
            free (tmp->path);
            free (tmp->name);
            
            free (tmp);
            return 0;
        
        }
    
    }
    
    return 0;

}

void cleanup_tempfiles (void) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if ((ret = fclose (tmp->fp))) {
            continue;
        }
        
        remove (tmp->path);
    
    }

}
#else
# include       "hashtab.h"

struct temp_file {

    char *name;
    FILE *fp;

};

static struct hashtab hashtab_tmpfiles = { 0 };

char *get_current_directory (void) {
    return xstrdup ("");
}

int is_pragma_igored (const char *filename) {

    const char *entry;
    int i;
    
    for (i = 0; i < vec_pragmas.length; i++) {
    
        entry = vec_pragmas.data[i];
        
        if (strcmp (filename, entry) == 0) {
            return 1;
        }
    
    }
    
    vec_push (&vec_pragmas, xstrdup (filename));
    return 0;

}

FILE *scc_tmpfile (void) {

    static char name[64] = { 0 };
    
    char *temp;
    unsigned long index, i, j;
    
    struct hashtab_name *key;
    struct temp_file *tmp;
    
    FILE *fp;
    
    for (i = 0; i < 456976; i++) {
    
        sprintf (name, "scc_AAAA.tmp");
        
        index = 7;
        j = i;
        
        do {
        
            name[index] = (j % 26) + 'A';
            index--;
        
        } while ((j /= 26) > 0);
        
        temp = xstrdup (name);
        
        if ((key = hashtab_get_key (&hashtab_tmpfiles, temp))) {
        
            free (temp);
            continue;
        
        }
        
        if ((fp = fopen (temp, "rb"))) {
        
            fclose (fp);
            
            free (temp);
            continue;
        
        }
        
        if (!((key = hashtab_alloc_name (temp)))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for key");
            exit (EXIT_FAILURE);
        
        }
        
        if ((fp = fopen (temp, "w+b"))) {
            
            tmp = xmalloc (sizeof (*tmp));
            
            tmp->name = temp;
            tmp->fp = fp;
        
            hashtab_put (&hashtab_tmpfiles, key, tmp);
            return fp;
        
        }
        
        free (temp);
        free (key);
    
    }
    
    return 0;

}

int scc_close (FILE *fp) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if (tmp->fp == fp) {
        
            if ((ret = fclose (fp))) {
                return ret;
            }
            
            hashtab_remove (&hashtab_tmpfiles, entry->key);
            remove (tmp->name);
            
            free (tmp->name);
            free (tmp);
            
            return 0;
        
        }
    
    }
    
    return 0;

}

void cleanup_tempfiles (void) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if ((ret = fclose (tmp->fp))) {
            continue;
        }
        
        remove (tmp->name);
    
    }

}
#endif


const char *real_filename = 0;
static unsigned long real_line_number = 0;

const char *get_real_filename (void) {
    return real_filename;
}

unsigned long get_real_line_number (void) {
    return real_line_number;
}

void set_real_filename (const char *__filename) {
    real_filename = __filename;
}

void set_real_line_number (unsigned long __line_number) {
    real_line_number = __line_number;
}


int has_include_stack (void) {
    return state->vec_hist.length > 0;
}

void print_include_stack (void) {

    struct hist_entry *entry;
    int i;
    
    printf ("in file included ");
    
    for (i = state->vec_hist.length - 1; i >= 0; i--) {
        
        entry = (struct hist_entry *) state->vec_hist.data[i];
        printf ("from %s:%lu", entry->filename, entry->line_number);
        
        if (i > 0) {
            
            printf (",\n");
            printf ("                 ");
        
        } else {
            printf ("\n");
        }
    
    }

}
