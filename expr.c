/******************************************************************************
 * @file            expr.c
 *****************************************************************************/
#include    <assert.h>
#include    <stdlib.h>
#include    <string.h>

#include    "expr.h"
#include    "int64.h"
#include    "lib.h"
#include    "parse.h"
#include    "report.h"
#include    "token.h"

static int64_s *eval_expr (int64_s *lhs, int outer_prec, int brackets);
static enum token_kind *killat = 0;

static void cast_integer_constant (int64_s *v, int size, int is_unsigned) {

    unsigned long sign;
    
    if (!v) {
        return;
    }
    
    if (size <= 1) {
    
        v->low &= 0xffUL;
        v->high = 0;
        
        if (!is_unsigned && (v->low & 0x80UL)) {
        
            v->low |= 0xffffff00UL;
            v->high = 0xffffffffUL;
        
        }
        
        return;
    
    }
    
    if (size == 2) {
    
        v->low &= 0xffffUL;
        v->high = 0;
        
        if (!is_unsigned && (v->low & 0x8000UL)) {
            v->low |= 0xffff0000UL;
            v->high = 0xffffffffUL;
        }
        
        return;
    
    }
    
    if (size == 4) {
    
        sign = v->low & 0x80000000UL;
        
        v->high = (!is_unsigned && sign) ? 0xffffffffUL : 0;
        return;
    
    }

}

static int should_terminate (void) {

    int i;
    
    if (killat) {
    
        for (i = 0; ; i++) {
        
            if (killat[i]) {
            
                if (tok.kind == killat[i]) {
                    return 1;
                }
                
                continue;
            
            }
            
            break;
        
        }
    
    }
    
    return 0;

}

static int64_s *eval_unary (int64_s *lhs) {

    if (token_is_sizeof_keyword ()) {
    
        int size;
        get_token ();
        
        size = parse_sizeof_value ();
        zext64 (lhs, (unsigned long) size);
        
        return lhs;
    
    }
    
    if (tok.kind == TOK_IDENT && resolve_enum_constant (tok.ident, lhs)) {
    
        get_token ();
        return lhs;
    
    }
    
    switch (tok.kind) {
    
        case TOK_CCHAR:     case TOK_CINT:      case TOK_CUINT:     case TOK_CULONG:
        case TOK_CLONG:     case TOK_CLLONG:    case TOK_CULLONG:   case TOK_LCHAR:
        
            lhs->high = tok.val.i.high;
            lhs->low = tok.val.i.low;
            
            get_token ();
            return lhs;
        
        default:
        
            break;
    
    }
    
    if (tok.kind == TOK_XMARK) {
    
        int64_s tmp = { 0 }, *tmp_p;
        
        get_token ();
        tmp_p = eval_unary (&tmp);
        
        if (!tmp_p) {
            return 0;
        }
        
        tmp = *tmp_p;
        zext64 (lhs, is_zero64 (tmp));
        
        return lhs;
    
    }
    
    if (tok.kind == TOK_TILDE) {
    
        int flip_bits = 0;
        
        while (tok.kind == TOK_TILDE) {
        
            flip_bits = !flip_bits;
            get_token ();
        
        }
        
        lhs = eval_unary (lhs);
        
        if (!lhs) {
            return 0;
        }
        
        if (flip_bits) {
            not64 (lhs);
        }
        
        return lhs;
    
    }
    
    if (tok.kind == TOK_MINUS || tok.kind == TOK_PLUS) {
    
        int sign = 1;
        
        while (tok.kind == TOK_MINUS || tok.kind == TOK_PLUS) {
        
            if (tok.kind == TOK_MINUS) { sign = !sign; }
            get_token ();
        
        }
        
        lhs = eval_unary (lhs);
        
        if (!lhs) {
            return 0;
        }
        
        if (!sign) {
            neg64 (lhs);
        }
        
        return lhs;
    
    }
    
    if (tok.kind == TOK_AMPER) {
    
        if (parse_constexpr_address_of_null_member (lhs)) {
            return lhs;
        }
        
        return 0;
    
    }
    
    if (tok.kind == TOK_LPAREN) {
    
        char *start = xstrdup (tok.start);
        unsigned long caret = tok.caret - tok.start;
        
        get_token ();
        
        if (token_starts_type_name ()) {
        
            int cast_size = 0;
            int cast_is_unsigned = 0;
            int cast_is_pointer = 0;
            
            if (parse_cast_type_name (&cast_size, &cast_is_unsigned, &cast_is_pointer)) {
            
                lhs = eval_unary (lhs);
                
                if (!lhs) {
                
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "integer constant expression expected");
                    lhs = 0;
                
                } else {
                
                    if (cast_is_pointer) {
                    
                        cast_size = 4;
                        cast_is_unsigned = 1;
                    
                    }
                    
                    cast_integer_constant (lhs, cast_size, cast_is_unsigned);
                
                }
                
                free (start);
                return lhs;
            
            }
        
        }
        
        lhs = eval_unary (lhs);
        lhs = eval_expr (lhs, 14, 1);
        
        if (tok.kind != TOK_RPAREN) {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, start + caret, "missing ')' in expression");
        } else {
            get_token ();
        }
        
        free (start);
        return lhs;
    
    }
    
    lhs->high = 0;
    lhs->low = 0;
    
    return 0;

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

static struct op *get_op (char *pp) {

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
        
        if (strncmp (pp, kw->word, strlen (kw->word)) == 0) {
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

static void recover_bad_const_operand (void) {

    int depth = 0;
    
    if (tok.kind == TOK_EOF || should_terminate ()) {
        return;
    }
    
    do {
    
        if (tok.kind == TOK_LPAREN) {
        
            depth++;
            
            get_token ();
            continue;
        
        }
        
        if (tok.kind == TOK_RPAREN) {
        
            if (depth > 0) {
            
                depth--;
                
                get_token ();
                continue;
            
            }
            
            if (should_terminate ()) {
            
                get_token ();
                continue;
            
            }
            
            break;
        
        }
        
        get_token ();
    
    } while (tok.kind != TOK_EOF && (depth > 0 || !should_terminate ()));

}

static int64_s *eval_expr (int64_s *lhs, int outer_prec, int brackets) {

    struct op *op = 0, *op2 = 0;
    
    int64_s rhs = { 0 }, *tmp;
    int prec, look_ahead;
    
    unsigned long index, lineno;
    
    for (;;) {
    
        if (brackets && tok.kind == TOK_RPAREN) {
            return lhs;
        }
        
        if (tok.kind == TOK_EOF || should_terminate ()) {
            break;
        }
        
        if (!(op = get_op (tok.ident)) || (prec = get_prec (op->kind)) >= outer_prec) {
        
            if (outer_prec == 15) {
                break;
            }
            
            return lhs;
        
        }
        
        get_token ();
        
        if (tok.kind == TOK_EOF || should_terminate ()) {
            break;
        }
        
        rhs.high = 0;
        rhs.low = 0;
        
        if (op->kind == OP_QUEST) {
        
            int64_s left = { 0 }, right = { 0 }, *tmp;
            
            if (!(tmp = eval_unary (&left))) {
                break;
            }
            
            left = *eval_expr (tmp, 14, brackets);
            
            assert (tok.kind == TOK_COLON);
            get_token ();
            
            if (!(tmp = eval_unary (&right))) {
                break;
            }
            
            right = *eval_expr (&right, 14, brackets);
            
            if (!is_zero64 (*lhs)) {
            
                lhs->high = left.high;
                lhs->low = left.low;
            
            } else {
            
                lhs->high = right.high;
                lhs->low = right.low;
            
            }
            
            continue;
        
        }
        
        if (!(tmp = eval_unary (&rhs))) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "integer constant expression expected");
            
            recover_bad_const_operand ();
            break;
        
        }
        
        for (;;) {
        
            rhs.high = tmp->high;
            rhs.low = tmp->low;
            
            if (tok.kind == TOK_EOF || should_terminate () || (brackets && tok.kind == TOK_RPAREN)) {
                goto _calc;
            }
            
            if (!(op2 = get_op (tok.ident)) || (look_ahead = get_prec (op2->kind)) >= prec) {
                break;
            }
            
            /* Get the current index and line number. */
            index = tok.caret - tok.start;
            lineno = get_line_number ();
            
            /* Try evaluation. */
            tmp = eval_expr (&rhs, prec, brackets);
            
            /* If the index and line number are the same them we;re done. */
            if (index == (unsigned long) (tok.caret - tok.start) && lineno == get_line_number ()) {
                break;
            }
        
        }
        
        if (!op2) { break; }
        
    _calc:
        
        switch (op->kind) {
        
            case OP_MUL:
            
                mul64 (lhs, rhs);
                break;
            
            case OP_DIV:
            
                div64 (lhs, rhs);
                break;
            
            case OP_MOD:
            
                mod64 (lhs, rhs);
                break;
            
            case OP_PLUS:
            
                add64 (lhs, rhs);
                break;
            
            case OP_MINUS:
            
                sub64 (lhs, rhs);
                break;
            
            case OP_LT:
            
                zext64 (lhs, lt64_unsigned (*lhs, rhs));
                break;
            
            case OP_GT:
            
                zext64 (lhs, gt64_unsigned (*lhs, rhs));
                break;
            
            case OP_LTEQ:
            
                zext64 (lhs, lte64_unsigned (*lhs, rhs));
                break;
            
            case OP_GTEQ:
            
                zext64 (lhs, gte64_unsigned (*lhs, rhs));
                break;
            
            case OP_EQEQ:
            
                zext64 (lhs, eq64 (*lhs, rhs));
                break;
            
            case OP_NOTEQ:
            
                zext64 (lhs, !eq64 (*lhs, rhs));
                break;
            
            case OP_AND:
            
                and64 (lhs, rhs);
                break;
            
            case OP_XOR:
            
                xor64 (lhs, rhs);
                break;
            
            case OP_OR:
            
                or64 (lhs, rhs);
                break;
            
            case OP_ANDAND:
            
                zext64 (lhs, !is_zero64 (*lhs) && !is_zero64 (rhs));
                break;
            
            case OP_OROR:
            
                zext64 (lhs, !is_zero64 (*lhs) || !is_zero64 (rhs));
                break;
            
            case OP_LSHIFT:
            
                shl64 (lhs, rhs.low);
                break;
            
            case OP_RSHIFT:
            
                shr64 (lhs, rhs.low);
                break;
            
            default:
            
                report_at (__FILE__, __LINE__, REPORT_INTERNAL_ERROR, "unimplemented");
                break;
        
        }
    
    }
    
    if (!(tok.kind == TOK_EOF || should_terminate ())) {
    
        if (tok.kind >= TOK_LCHAR && tok.kind <= TOK_CULONG) {
            
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "missing binary operator before token \"%s\"", tok.ident);
            
            while (!(tok.kind == TOK_EOF || should_terminate ())) {
            
                if (brackets && tok.kind == TOK_RPAREN) {
                    break;
                }
                
                get_token ();
            
            }
            
        } else if (op && ((op2 = get_op (tok.ident)) || (brackets && tok.kind == TOK_RPAREN))) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "operator '%s' has no right operand", op->word);
            
            if (op2) {
            
                get_token ();
                
                while (!(tok.kind == TOK_EOF || should_terminate ())) {
                
                    if (brackets && tok.kind == TOK_RPAREN) {
                        break;
                    }
                    
                    get_token ();
                
                }
            
            }
        
        } else if (brackets && tok.kind == TOK_RPAREN) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "missing '(' in expression");
            
            while (!(tok.kind == TOK_EOF || should_terminate ())) {
            
                if (brackets && tok.kind == TOK_RPAREN) {
                    break;
                }
                
                get_token ();
            
            }
        
        } else {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "integer constant expression expected");
            
            while (!(tok.kind == TOK_EOF || should_terminate ())) {
            
                if (brackets && tok.kind == TOK_RPAREN) {
                    break;
                }
                
                get_token ();
            
            }
        
        }
        
        lhs->high = 0;
        lhs->low = 0;
    
    }
    
    return lhs;

}

int expr_const (enum token_kind *k) {

    char *start = xstrdup (tok.start);
    unsigned long caret = tok.caret - tok.start;
    
    int64_s wc = expr_const64 (k);
    unsigned long sign = wc.low & 0x80000000UL;
    
    int c = (int) wc.low;
    
    if (!((wc.high == 0) || (sign && wc.high == 0xFFFFFFFFUL))) {
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, start + caret, "constant exceeds 32 bit");
    }
    
    free (start);
    return c;

}

int64_s expr_const64 (enum token_kind *k) {

    struct op *op = 0;
    int64_s lhs = { 0 }, *tmp;
    
    char *start = xstrdup (tok.start);
    unsigned long caret = tok.caret - tok.start;
    
    killat = k;
    
    if (!(tmp = eval_unary (&lhs))) {
    
        if ((op = get_op (tok.ident))) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, start + caret, "operator '%s' has no left operand", op->word);
            
            while (!(tok.kind == TOK_EOF || should_terminate ())) {
                get_token ();
            }
            
            free (start);
            
            lhs.high = 0;
            lhs.low = 0;
            
            return lhs;
        
        }
        
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "integer constant expression expected");
        
        recover_bad_const_operand ();
        tmp = &lhs;
    
    }
    
    lhs = *eval_expr (tmp, 15, 0);
    
    free (start);
    return lhs;

}
