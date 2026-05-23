/******************************************************************************
 * @file            eval.c
 *****************************************************************************/
#include    <assert.h>
#include    <ctype.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "cstr.h"
#include    "eval.h"
#include    "lex.h"
#include    "lib.h"
#include    "macro.h"
#include    "report.h"
#include    "stdint.h"

static uint64_t eval_expr (uint64_t lhs, char *start, char **pp, int outer_prec, int brackets);

static int hex_value (int ch) {

    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    
    ch = tolower ((int) ch);
    
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    
    return -1;

}

static uint64_t eval_char_constant (char *start, char **pp) {

    char *caret = *pp;
    uint64_t value = 0, ch = 0;
    int count = 0;
    
    (*pp)++;
    
    while (!is_end_of_line[(int) **pp] && **pp != '\'') {
    
        if (**pp == '\\') {
        
            (*pp)++;
            
            switch (**pp) {
            
                case 'a':
                
                    ch = '\a';
                    
                    (*pp)++;
                    break;
                
                case 'b':
                
                    ch = '\b';
                    
                    (*pp)++;
                    break;
                
                case 'f':
                
                    ch = '\f';
                    
                    (*pp)++;
                    break;
                
                case 'n':
                
                    ch = '\n';
                    
                    (*pp)++;
                    break;
                
                case 'r':
                
                    ch = '\r';
                    
                    (*pp)++;
                    break;
                
                case 't':
                
                    ch = '\t';
                    
                    (*pp)++;
                    break;
                
                case 'v':
                
                    ch = '\v';
                    
                    (*pp)++;
                    break;
                
                case '\\':
                
                    ch = '\\';
                    
                    (*pp)++;
                    break;
                
                case '\'':
                
                    ch = '\'';
                    
                    (*pp)++;
                    break;
                
                case '"':
                
                    ch = '"';
                    
                    (*pp)++;
                    break;
                
                case '0':   case '1':
                case '2':   case '3':
                case '4':   case '5':
                case '6':   case '7': {
                
                    int i = 0;
                    ch = 0;
                    
                    while (i < 3 && **pp >= '0' && **pp <= '7') {
                    
                        ch = (ch * 8) + (*((*pp)++) - '0');
                        i++;
                    
                    }
                    
                    break;
                
                }
                
                case 'x': {
                
                    int h, any = 0;
                    ch = 0;
                    
                    (*pp)++;
                    
                    while ((h = hex_value ((int) **pp)) >= 0) {
                    
                        ch = (ch * 16) + h;
                        
                        (*pp)++;
                        any = 1;
                    
                    }
                    
                    if (!any) {
                        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, *pp - 1, "\\x used with no following hex digits");
                    }
                    
                    break;
                
                
                }
                
                case '\0':
                
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, *pp, "missing terminating ' character");
                    return value;
                
                default:
                
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, *pp, "unknown escape sequence");
                    
                    ch = (int) **pp;
                    (*pp)++;
                    
                    break;
            
            }
        
        } else {
            ch = (int) *((*pp)++);
        }
        
        value = (value << 8) | (ch & 0xff);
        count++;
    
    }
    
    if (**pp != '\'') {
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "missing terminating ' character");
    } else {
        (*pp)++;
    }
    
    if (count > 1) {
        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start, caret, "multi-character character constant");
    }
    
    return value;

}

static uint64_t eval_unary (uint64_t lhs, char *start, char **pp) {

    char *caret;
    *pp = skip_whitespace (*pp);
    
    if (**pp == '\'') {
        return eval_char_constant (start, pp);
    }
    
    if (isdigit ((int) **pp)) {
    
        uint64_t temp, temp2;
        int ch, i;
        
        if ((*pp)[0] == '0' && tolower ((int) (*pp)[1]) == 'x') {
        
            unsigned int base = 16;
            *pp += 2;
            
            while (isxdigit ((int) **pp)) {
            
                temp = lhs * base;
                ch = *((*pp)++);
                
                if (ch >= '0' && ch <= '9') {
                    temp2 = ch - '0';
                } else {
                    temp2 = (ch & 0xdf) - ('A' - 10);
                }
                
                lhs = temp + temp2;
            
            }
        
        } else if ((*pp)[0] == '0') {
        
            unsigned int base = 8;
            
            while (isdigit ((int) **pp)) {
            
                temp = lhs * base;
                lhs = (temp + (*((*pp)++) - '0'));
            
            }
        
        } else {
        
            unsigned int base = 10;
            
            while (isdigit ((int) **pp)) {
            
                temp = lhs * base;
                lhs = (temp + (*((*pp)++) - '0'));
            
            }
        
        }
        
        caret = *pp;
        
        if (**pp == 'L') {
        
            (*pp)++;
            
            if (**pp == 'L') {
                (*pp)++;
            }
            
            if (**pp == 'U') {
                (*pp)++;
            }
        
        } else if (**pp == 'U') {
        
            (*pp)++;
            
            for (i = 0; i < 2; i++) {
                
                if (**pp == 'L') {
                    (*pp)++;
                }
                
            }
        
        }
        
        if (is_name_beginner ((int) **pp)) {
        
            while (is_name_part ((int) **pp)) {
                (*pp)++;
            }
            
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "invalid suffix \"%.*s\" on integer constant", (int) (*pp - caret), caret);
        
        }
        
        return lhs;
    
    }
    
    if (is_name_beginner ((int) **pp)) {
    
        char *sname, *temp;
        
        struct hashtab_name *key;
        struct macro *m;
        
        caret = *pp;
        
        while (is_name_part ((int) *caret)) {
            caret++;
        }
        
        if ((caret - *pp) == 7 && memcmp (*pp, "defined", 7) == 0) {
        
            caret = skip_whitespace (caret);
            *pp = caret;
            
            if (*caret == '(') {
            
                caret = skip_whitespace (caret + 1);
                *pp = caret;
                
                while (!is_end_of_line[(int) *caret]) {
                
                    if (isspace ((int) *caret) || *caret == ')') { break; }
                    caret++;
                
                }
                
                sname = xstrndup (*pp, caret - *pp);
                caret = skip_whitespace (caret);
                
                if (*caret != ')') {
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "missing ')' after \"defined\"");
                }
            
            } else {
                sname = xstrndup (*pp, caret - *pp);
            }
            
            if (*caret == ')') { caret++; }
            *pp = skip_whitespace (caret);
            
            lhs = !(find_macro (sname) == 0);
            return lhs;
        
        }
        
        sname = xstrndup (*pp, caret - *pp);
        *pp = skip_whitespace (caret);
        
        if ((key = find_macro (sname))) {
        
            free (sname);
            
            if ((m = get_macro (key))) {
            
                char *line = (temp = process_macro (start, pp, m));
                
                lhs = eval_unary (lhs, line, &temp);
                free (line);
                
                return lhs;
            
            }
        
        }
        
        free (sname);
        return lhs;
    
    }
    
    if (**pp == '!') {
    
        uint64_t temp = 0;
        
        *pp = skip_whitespace (*pp + 1);
        temp = eval_unary (temp, start, pp);
        
        return (temp == 0);
    
    }
    
    if (**pp == '~') {
    
        int flip_bits = 0;
        
        while (**pp == '~') {
        
            flip_bits = !flip_bits;
            *pp = skip_whitespace (*pp + 1);
        
        }
        
        lhs = eval_unary (lhs, start, pp);
        
        if (flip_bits) {
            lhs = ~lhs;
        }
        
        return lhs;
    
    }
    
    if (**pp == '-') {
    
        int sign = 1;
        
        while (**pp == '-' || **pp == '+') {
        
            if (**pp == '-') { sign = !sign; }
            *pp = skip_whitespace (*pp + 1);
        
        }
        
        lhs = eval_unary (lhs, start, pp);
        
        if (!sign) {
            lhs = -lhs;
        }
        
        return lhs;
    
    }
    
    if (**pp == '(') {
    
        char *new_line, *temp;
        int depth = 0;
        
        CString cstr;
        cstr_new (&cstr);
        
        caret = (*pp)++;
        cstr_cat (&cstr, start, *pp - start);
        
        while (!is_end_of_line[(int) **pp]) {
        
            if (**pp == '(') {
            
                cstr_ccat (&cstr, *((*pp)++));
                
                depth++;
                continue;
            
            }
            
            if (**pp == ')') {
            
                if (depth > 0) {
                
                    cstr_ccat (&cstr, *((*pp)++));
                    
                    depth--;
                    continue;
                
                }
                
                break;
            
            }
            
            cstr_ccat (&cstr, *((*pp)++));
        
        }
        
        if (**pp == ')') {
            cstr_ccat (&cstr, *((*pp)++));
        }
        
        cstr_cat (&cstr, *pp, strlen (*pp));
        cstr_ccat (&cstr, '\0');
        
        new_line = temp = xstrdup (cstr.data);
        temp += (caret - start) + 1;
        
        lhs = eval_unary (lhs, new_line, &temp);
        lhs = eval_expr (lhs, new_line, &temp, 14, 1);
        
        if (*(temp = skip_whitespace (temp)) != ')') {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "missing ')' in expression");
        }
        
        free (new_line);
        
        /*caret = (*pp)++;
        
        lhs = eval_unary (lhs, start, pp);
        lhs = eval_expr (lhs, start, pp, 15, 1);
        
        if (*(*pp = skip_whitespace (*pp)) != ')') {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "missing ')' in expression");
        }*/
        
        return lhs;
    
    }
    
    return -1;

}

#define     OP_MUL                      0
#define     OP_DIV                      1
#define     OP_MOD                      2
#define     OP_PLUS                     3
#define     OP_MINUS                    4
#define     OP_LT                       5
#define     OP_GT                       6
#define     OP_LTEQ                     7
#define     OP_GTEQ                     8
#define     OP_EQEQ                     9
#define     OP_NOTEQ                    11
#define     OP_AND                      12
#define     OP_XOR                      13
#define     OP_OR                       14
#define     OP_ANDAND                   15
#define     OP_OROR                     16
#define     OP_LSHIFT                   17
#define     OP_RSHIFT                   18
#define     OP_QUEST                    19
#define     OP_MAX                      20

struct op {

    char *word;
    int kind;

};

static struct op *get_op (char **pp) {

    static struct op kws[] = {
    
        {   "<=",   OP_LTEQ     },
        {   ">=",   OP_GTEQ     },
        {   "==",   OP_EQEQ     },
        {   "!=",   OP_NOTEQ    },
        {   "&&",   OP_ANDAND   },
        {   "||",   OP_OROR     },
        {   "<<",   OP_LSHIFT   },
        {   ">>",   OP_RSHIFT   },
        
        {   "*",    OP_MUL      },
        {   "/",    OP_DIV      },
        {   "%",    OP_MOD      },
        {   "+",    OP_PLUS     },
        {   "-",    OP_MINUS    },
        {   "<",    OP_LT       },
        {   ">",    OP_GT       },
        {   "&",    OP_AND      },
        {   "^",    OP_XOR      },
        {   "|",    OP_OR       },
        {   "?",    OP_QUEST    }
    
    };
    
    struct op *kw;
    unsigned int i;
    
    for (i = 0; i < (sizeof (kws) / sizeof (*kws)); i++) {
    
        kw = &kws[i];
        
        if (strncmp (*pp, kw->word, strlen (kw->word)) == 0) {
        
            *pp += strlen (kw->word);
            return kw;
        
        }
    
    }
    
    return 0;

}

static int get_prec (int kind) {

    switch (kind) {
    
        case OP_MUL:    case OP_DIV:    case OP_MOD:
        
            return 3;
        
        case OP_PLUS:   case OP_MINUS:
        
            return 4;
        
        case OP_LSHIFT:                 case OP_RSHIFT:
        
            return 5;
        
        case OP_LT:     case OP_GT:     case OP_LTEQ:       case OP_GTEQ:
        
            return 6;
        
        case OP_EQEQ:   case OP_NOTEQ:
        
            return 7;
        
        case OP_AND:
        
            return 8;
        
        case OP_XOR:
        
            return 9;
        
        case OP_OR:
        
            return 10;
        
        case OP_ANDAND:
        
            return 11;
        
        case OP_OROR:
        
            return 12;
        
        case OP_QUEST:
        
            return 13;
        
        default:
        
            break;
    
    } 
    
    return 100;

}

static uint64_t eval_expr (uint64_t lhs, char *start, char **pp, int outer_prec, int brackets) {

    struct op *op = 0, *op2 = 0;
    int prec, look_ahead;
    
    char *caret;
    uint64_t rhs;
    
    for (;;) {
    
        *pp = skip_whitespace (*pp);
        
        if (brackets && **pp == ')') {
            return lhs;
        }
        
        if (is_end_of_line[(int) **pp]) {
            break;
        }
        
        if (!(op = get_op (pp)) || (prec = get_prec (op->kind)) >= outer_prec) {
        
            if (op) { *pp -= strlen (op->word); }
            
            if (outer_prec == 15) {
                break;
            }
            
            return lhs;
        
        }
        
        *pp = skip_whitespace (*pp);
        
        if (is_end_of_line[(int) **pp]) {
            break;
        }
        
        rhs = 0;
        
        if (op->kind == OP_QUEST) {
        
            uint64_t left = 0, right = 0;
            
            left = eval_unary (left, start, pp);
            left = eval_expr (left, start, pp, 14, brackets);
            
            *pp = skip_whitespace (*pp);
            assert (**pp == ':');
            *pp = skip_whitespace (*pp + 1);
            
            right = eval_unary (right, start, pp);
            right = eval_expr (right, start, pp, 14, brackets);
            
            if (lhs != 0) {
                lhs = left;
            } else {
                lhs = right;
            }
            
            continue;
        
        }
        
        caret = (*pp = skip_whitespace (*pp));
        
        if (is_end_of_line[(int) **pp]) {
            break;
        }
        
        rhs = eval_unary (rhs, start, pp);
        
        if (*pp == caret) {
            break;
        }
        
        for (;;) {
        
            *pp = skip_whitespace (*pp);
            
            if (is_end_of_line[(int) **pp] || (brackets && **pp == ')')) {
                goto _calc;
            }
            
            if (!(op2 = get_op (pp)) || (look_ahead = get_prec (op2->kind)) >= prec) {
            
                if (op2) { *pp -= strlen (op2->word); }
                break;
            
            }
            
            caret = (*pp -= strlen (op2->word));
            
            if (is_end_of_line[(int) **pp]) {
                break;
            }
            
            rhs = eval_expr (rhs, start, pp, prec, brackets);
            
            if (*pp == caret) {
                break;
            }
        
        }
        
        if (!op2) { break; }
        
    _calc:
        
        switch (op->kind) {
        
            case OP_MUL:
            
                lhs *= rhs;
                break;
            
            case OP_DIV:
            
                lhs /= rhs;
                break;
            
            case OP_MOD:
            
                lhs %= rhs;
                break;
            
            case OP_PLUS:
            
                lhs += rhs;
                break;
            
            case OP_MINUS:
            
                lhs -= rhs;
                break;
            
            case OP_LT:
            
                lhs = (lhs < rhs);
                break;
            
            case OP_GT:
            
                lhs = (lhs > rhs);
                break;
            
            case OP_LTEQ:
            
                lhs = (lhs <= rhs);
                break;
            
            case OP_GTEQ:
            
                lhs = (lhs >= rhs);
                break;
            
            case OP_EQEQ:
            
                lhs = (lhs == rhs);
                break;
            
            case OP_NOTEQ:
            
                lhs = (lhs != rhs);
                break;
            
            case OP_AND:
            
                lhs &= rhs;
                break;
            
            case OP_XOR:
            
                lhs ^= rhs;
                break;
            
            case OP_OR:
            
                lhs |= rhs;
                break;
            
            case OP_ANDAND:
            
                lhs = ((lhs != 0) && (rhs != 0));
                break;
            
            case OP_OROR:
            
                lhs = ((lhs != 0) || (rhs != 0));
                break;
            
            case OP_LSHIFT:
            
                lhs <<= rhs;
                break;
            
            case OP_RSHIFT:
            
                lhs >>= rhs;
                break;
            
            default:
            
                report_at (__FILE__, __LINE__, REPORT_INTERNAL_ERROR, "unimplemented");
                break;
        
        }
    
    }
    
    caret = (*pp = skip_whitespace (*pp));
    
    if (!is_end_of_line[(int) **pp]) {
    
        if (is_name_beginner ((int) **pp) || isdigit ((int) **pp)) {
        
            while (is_name_part ((int) **pp)) {
                (*pp)++;
            }
            
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "missing binary operator before token \"%.*s\"", (int) (*pp - caret), caret);
            
            while (!is_end_of_line[(int) **pp]) {
            
                if ((brackets && **pp == ')')) {
                    break;
                }
                
                (*pp)++;
            
            }
            
        } else if (op && ((op2 = get_op (pp)) || (brackets && **pp == ')'))) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "operator '%s' has no right operand", op->word);
            
            if (op2) {
            
                while (!is_end_of_line[(int) **pp]) {
                
                    if ((brackets && **pp == ')')) {
                        break;
                    }
                    
                    (*pp)++;
                
                }
            
            }
        
        } else if (**pp == ')') {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "missing '(' in expression");
            
            while (!is_end_of_line[(int) **pp]) {
            
                if ((brackets && **pp == ')')) {
                    break;
                }
                
                (*pp)++;
            
            }
        
        } else {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, *pp, "token \"%c\" is not valid in preprocessor expressions", **pp);
            
            while (!is_end_of_line[(int) **pp]) {
            
                /*if (is_name_beginner ((int) **pp)) {
                    break;
                }*/
                
                if (/*isspace ((int) **pp) || */(brackets && **pp == ')')) {
                    break;
                }
                
                (*pp)++;
            
            }
        
        }
        
        *pp = skip_whitespace (*pp);
        return 0;
    
    }
    
    return lhs;

}

int eval (char *start, char **pp) {

    struct op *op = 0;
    uint64_t lhs = 0;
    
    char *caret = (*pp = skip_whitespace (*pp));
    
    if (!is_end_of_line[(int) **pp]) {
    
        lhs = eval_unary (lhs, start, pp);
        
        if (*pp == caret && (op = get_op (pp))) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "operator '%s' has no left operand", op->word);
            
            while (!is_end_of_line[(int) **pp]) {
                (*pp)++;
            }
            
            return 0;
        
        }
        
        lhs = eval_expr (lhs, start, pp, 15, 0);
        
        if (!is_end_of_line[(int) **pp]) {
            report_at (__FILE__, __LINE__, REPORT_INTERNAL_ERROR, "#if not empty");
        }
        
        return (lhs != 0);
    
    }
    
    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "#if with no expression");
    return 0;

}
