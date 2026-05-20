/******************************************************************************
 * @file            int64.c
 *****************************************************************************/
#include    <ctype.h>
#include    <string.h>

#include    "int64.h"
#include    "report.h"

extern const char *get_filename (void);
extern unsigned long get_line_number (void);

void add64 (int64_s *a, int64_s b) {

    unsigned long raw_sum, carry;
    
    /* 1. Calculate the raw result */
    raw_sum = a->low + b.low;

    /* 2. Manually force 32-bit wrap around using a mask */
    /* This ensures that any "1" that would be in the 33rd bit is stripped */
    a->low = raw_sum & 0xFFFFFFFFUL;

    /* 3. Extract the carry: if raw_sum > 0xFFFFFFFF, we had a carry */
    carry = (raw_sum > 0xFFFFFFFFUL) ? 1 : 0;

    /* 4. Update the high part */
    a->high = (a->high + b.high + carry) & 0xFFFFFFFFUL;

}

void mul64 (int64_s *a, int64_s b) {

    /* Extract 16-bit halves of the low words */
    unsigned long al = a->low & 0xFFFF;
    unsigned long ah = (a->low >> 16) & 0xFFFF;
    unsigned long bl = b.low & 0xFFFF;
    unsigned long bh = (b.low >> 16) & 0xFFFF;
    
    /* 1. Calculate the 32x32 -> 64 bit product of (a->low * b.low) */
    unsigned long p0 = al * bl;
    unsigned long p1 = al * bh;
    unsigned long p2 = ah * bl;
    unsigned long p3 = ah * bh;
    
    /* Handle middle-word carries */
    unsigned long mid = (p0 >> 16) + (p1 & 0xFFFF) + (p2 & 0xFFFF);
    
    /* Store the new low 32 bits */
    unsigned long new_low = ((mid << 16) | (p0 & 0xFFFF));
    
    /* 2. Calculate the high word */
    /* Start with the carries/high-bits from the 32x32 multiply */
    unsigned long new_high = p3 + (p1 >> 16) + (p2 >> 16) + (mid >> 16);
    
    /* 3. Add the "Cross Multiplication" of the original high word */
    /* Mathematically: (a->high * b.low) + (b.high * a->low) */
    /* Note: We ignore (a->high * b.high) as it overflows 64 bits */
    new_high += (a->high * b.low);
    new_high += (b.high * a->low);
    
    /* Final Assignment */
    a->low = new_low;
    a->high = new_high;

}

void shl64 (int64_s *a, int n) {

    /* Handle shifts >= 32 separately */
    if (n >= 32) {
    
        a->high = (a->low << (n - 32)) & 0xFFFFFFFFUL;
        a->low = 0;
    
    } else if (n > 0) {
    
        /* Bits pushed out of low go into high */
        a->high = ((a->high << n) | (a->low >> (32 - n))) & 0xFFFFFFFFUL;
        a->low = (a->low << n) & 0xFFFFFFFFUL;
    
    }

}

void shr64 (int64_s *a, int n) {

    /* Handle shifts >= 32 separately */
    if (n >= 32) {
    
        a->low = (a->high >> (n - 32));
        a->high = 0;
    
    } else if (n > 0) {
    
        /* Bits pushed out of high go into low */
        a->low = ((a->low >> n) | (a->high << (32 - n))) & 0xFFFFFFFFUL;
        a->high = (a->high >> n) & 0xFFFFFFFFUL;
    
    }

}

void sub64 (int64_s *a, int64_s b) {

    unsigned long old_low = a->low, borrow;

    /* 1. Perform subtraction and force 32-bit wrapping */
    a->low = (a->low - b.low) & 0xFFFFFFFFUL;

    /* 2. Determine if a borrow occurred:
     * In unsigned arithmetic, if (a->low > old_low), we wrapped.
     * Because we masked the result, this logic is now 100% stable.
     */
    borrow = (old_low < b.low) ? 1 : 0;

    /* 3. Subtract high parts and the borrow, forcing 32-bit wrap */
    /* If high - b.high - borrow is negative, the & 0xFFFFFFFFUL 
       will wrap it correctly to the 32-bit representation */
    a->high = (a->high - b.high - borrow) & 0xFFFFFFFFUL;

}

void and64 (int64_s *a, int64_s b) {

    a->low &= b.low;
    a->high &= b.high;

}

void or64 (int64_s *a, int64_s b) {

    a->low |= b.low;
    a->high |= b.high;

}

void xor64 (int64_s *a, int64_s b) {

    a->low ^= b.low;
    a->high ^= b.high;

}

/* Returns 1 if a > b, -1 if a < b, and 0 if a == b */
int compare64 (int64_s a, int64_s b) {

    /* Compare the high bits first (the most significant part) */
    if (a.high > b.high) return 1;
    if (a.high < b.high) return -1;
    
    /* If high bits are equal, compare the low bits */
    if (a.low > b.low) return 1;
    if (a.low < b.low) return -1;
    
    /* If both are identical */
    return 0;

}

void div64 (int64_s *a, int64_s b) {

    int64_s quotient = { 0, 0 };
    int64_s remainder = { 0, 0 };
    
    int i;
    
    /* If divisor is zero, you should handle this as a fatal compiler error */
    if (b.low == 0 && b.high == 0) {
        return;
    }
    
    /* Iterate through all 64 bits of the dividend */
    for (i = 63; i >= 0; i--) {
    
        /* 1. Shift the working remainder left to make room for the next bit */
        shl64 (&remainder, 1);
        
        /* 2. Bring in the i-th bit from the dividend (a) */
        /* Use a mask to check the bit and pull it into remainder.low */
        if (i >= 32) {
        
            if ((a->high >> (i - 32)) & 1UL) {
                remainder.low |= 1;
            }
        
        } else {
        
            if ((a->low >> i) & 1UL) {
                remainder.low |= 1;
            }
        
        }

        /* 3. If remainder >= divisor, we can subtract and set quotient bit */
        if (compare64 (remainder, b) >= 0) {
        
            sub64 (&remainder, b);
            
            /* Set the i-th bit in the quotient */
            if (i >= 32) {
                quotient.high |= (1UL << (i - 32));
            } else {
                quotient.low |= (1UL << i);
            }
        
        }
    
    }
    
    /* Overwrite dividend with the resulting quotient */
    a->low = quotient.low;
    a->high = quotient.high;

}

void mod64 (int64_s *a, int64_s b) {

    int64_s remainder = {0, 0};
    int i;
    
    /* 1. Safety check for division by zero */
    if (b.low == 0 && b.high == 0) {
        return;
    }
    
    /* 2. Iterate through all 64 bits */
    for (i = 63; i >= 0; i--) {
    
        /* Shift remainder left by 1 */
        shl64 (&remainder, 1);
        
        /* Bring in the i-th bit from 'a' */
        if (i >= 32) {
        
            if ((a->high >> (i - 32)) & 1UL) {
                remainder.low |= 1;
            }
        
        } else {
        
            if ((a->low >> i) & 1UL) {
                remainder.low |= 1;
            }
        
        }

        /* 3. Compare and Subtract (The core logic) */
        if (compare64 (remainder, b) >= 0) {
            sub64 (&remainder, b);
        }
    
    }
    
    /* 4. Update 'a' with the final remainder */
    a->low = remainder.low;
    a->high = remainder.high;

}

void zext64 (int64_s *dest, unsigned long input) {

    dest->low = input;
    dest->high = 0;                     /* Clear the upper bits */

}

void sext64 (int64_s *dest, long input) {

    dest->low = (unsigned long)input;
    
    /* If the 31st bit (MSB) is 1, the number is negative */
    if (input & 0x80000000UL) {
        dest->high = 0xFFFFFFFFUL;
    } else {
        dest->high = 0x00000000UL;
    }

}

void mask64 (int64_s *a, int bits) {

    unsigned long low_mask;
    unsigned long high_mask;
    unsigned long new_low, new_high;
    
    if (bits >= 64) {
        return;
    }
    
    if (bits <= 0) {
    
        if (a->high != 0 || a->low != 0) {
            report_at (get_filename(), get_line_number(), REPORT_WARNING, "implicit truncation of 0x%08lX%08lX to 0x00000000", a->high, a->low);
        }
        
        a->high = 0;
        a->low = 0;
        
        return;
    
    }
    
    if (bits < 32) {
    
        low_mask = (1UL << bits) - 1;
        
        new_low = a->low & low_mask;
        new_high = 0;
        
        if (a->high != 0 || (a->low & ~low_mask) != 0) {
            report_at (get_filename(), get_line_number(), REPORT_WARNING, "implicit truncation of 0x%08lX%08lX to 0x%08lX", a->high, a->low, new_low);
        }
    
    
    } else if (bits == 32) {
    
        new_low = a->low;
        new_high = 0;
        
        if (a->high != 0) {
            report_at (get_filename(), get_line_number(), REPORT_WARNING, "implicit truncation of 0x%08lX%08lX to 0x%08lX", a->high, a->low, new_low);
        }
    
    } else { /* 33..63 */
    
        high_mask = (1UL << (bits - 32)) - 1;
        
        new_low = a->low;
        new_high = a->high & high_mask;
        
        if ((a->high & ~high_mask) != 0) {
            report_at (get_filename(), get_line_number(), REPORT_WARNING, "implicit truncation of 0x%08lX%08lX to 0x%08lX%08lX", a->high, a->low, new_high, new_low);
        }
    
    }
    
    a->high = new_high;
    a->low = new_low;
    
    report_at (__FILE__, __LINE__, REPORT_INTERNAL_ERROR, "???");

}

/* The specialized wrapper for standard 32-bit int truncation */
void trunc64 (int64_s *a) {
    mask64 (a, 32);
}

void inc64 (int64_s *a) {

    a->low++;
    
    if (a->low == 0) {                  /* Carry occurred */
        a->high++;
    }

}

void dec64 (int64_s *a) {

    if (a->low == 0) {                  /* Borrow needed */
        a->high--;
    }
    
    a->low--;

}

void not64 (int64_s *a) {

    /* Flip bits and then force them back into 32-bit bounds */
    a->low = (~a->low) & 0xFFFFFFFFUL;
    a->high = (~a->high) & 0xFFFFFFFFUL;

}

void neg64 (int64_s *a) {

    not64 (a);
    inc64 (a);

}

int is_zero64 (int64_s a) {
    return (a.low == 0 && a.high == 0);
}

int eq64 (int64_s a, int64_s b) {
    return (a.low == b.low && a.high == b.high);
}

int neq64 (int64_s a, int64_s b) {
    return !eq64 (a, b);
}

int lt64_unsigned (int64_s a, int64_s b) {

    if (a.high != b.high) {
        return a.high < b.high;
    }
    
    return a.low < b.low;

}

int lte64_unsigned (int64_s a, int64_s b) {

    if (a.high != b.high) {
        return a.high < b.high;
    }
    
    return a.low <= b.low;

}

int lt64_signed (int64_s a, int64_s b) {

    unsigned long a_sign = (a.high >> 31) & 1;
    unsigned long b_sign = (b.high >> 31) & 1;
    
    /* If signs differ, negative is less than positive */
    if (a_sign != b_sign) {
        return a_sign > b_sign;
    }
    
    /* If signs are the same, treat as unsigned */
    return lt64_unsigned (a, b);

}

/* Less than or equal to (Signed) */
int lte64_signed (int64_s a, int64_s b) {

    /* A <= B is true if A < B or A == B */
    return lt64_signed (a, b) || eq64 (a, b);

}

/* Greater than or equal to (unsigned) */
int gte64_unsigned (int64_s a, int64_s b) {

    /* If it's NOT less than, it is greater than or equal to */
    return !lt64_unsigned (a, b);

}

/* Greater than or equal to (signed) */
int gte64_signed (int64_s a, int64_s b) {

    /* If it's NOT less than, it is greater than or equal to */
    return !lt64_signed (a, b);

}

/* Greater than (Unsigned) */
int gt64_unsigned (int64_s a, int64_s b) {

    /* If it's not less than or equal to, it must be greater than */
    return !lte64_unsigned (a, b);

}

/* Greater than (Signed) */
int gt64_signed (int64_s a, int64_s b) {

    /* If it's not less than or equal to, it must be greater than */
    return !lte64_signed (a, b);

}

void ld_to_u64 (int64_s *out, long double x) {

    long double base = 4294967296.0L;                       /* 2^32 */
    long double hi, lo;
    
    if (x <= 0.0L) {
    
        out->low = 0;
        out->high = 0;
        
        return;
    
    }
    
    /* optional clamp */
    if (x >= 18446744073709551615.0L) {                     /* 2^64 - 1 */
    
        out->low = 0xFFFFFFFFUL;
        out->high = 0xFFFFFFFFUL;
        
        return;
    
    }
    
    hi = x / base;
    out->high = (unsigned long) hi;
    
    lo = x - ((long double) out->high * base);
    out->low = (unsigned long) lo;

}

void ld_to_i64 (int64_s *out, long double x) {

    long double limit = 9223372036854775808.0L; /* 2^63 */
    
    if (x >= limit) {
    
        /* INT64_MAX */
        out->low = 0xFFFFFFFFUL;
        out->high = 0x7FFFFFFFUL;
        
        return;
    
    }
    
    if (x < -limit) {
    
        /* INT64_MIN */
        out->low = 0x00000000UL;
        out->high = 0x80000000UL;
        
        return;
    
    }
    
    if (x >= 0.0L) {
    
        ld_to_u64 (out, x);
        return;
    
    }
    
    /* negative case */
    {
    
        int64_s tmp;
        ld_to_u64 (&tmp, -x);
        
        *out = tmp;
        neg64 (out);
    
    }

}

void sign_extend_masked (int64_s *v, unsigned long mask) {

    unsigned long sign;
    
    mask &= 0xFFFFFFFFUL;
    sign = ((mask >> 1) + 1) & 0xFFFFFFFFUL;
    
    v->low &= 0xFFFFFFFFUL;
    
    if (v->low & sign) {
    
        v->low = (v->low | (~mask)) & 0xFFFFFFFFUL;
        v->high = 0xFFFFFFFFUL;
    
    } else {
    
        v->low &= mask;
        v->high = 0;
    
    }
    
    v->low &= 0xFFFFFFFFUL;
    v->high &= 0xFFFFFFFFUL;

}

static void norm64 (int64_s *v) {

    v->low &= U32_MASK;
    v->high &= U32_MASK;

}

long double u64_to_ld (int64_s v) {

    norm64 (&v);
    return (long double) v.high * 4294967296.0L + (long double) v.low;

}

long double i64_to_ld (int64_s v) {

    norm64 (&v);

    if (!(v.high & 0x80000000UL)) {
        return (long double) v.high * 4294967296.0L + (long double) v.low;
    }

    neg64 (&v);
    return -((long double) v.high * 4294967296.0L + (long double) v.low);

}

int int64_is_negative (int64_s v) {
    return (v.high & 0x80000000LU) != 0;
}

int int64_fits_uint (int64_s v) {

    /* assumes unsigned int is 32-bit */
    return v.high == 0;

}

int int64_fits_int (int64_s v) {

    /* range: -2147483648 .. 2147483647 */
    if (v.high == 0) {
        return v.low <= 0x7fffffffLU;
    }
    
    if ((long) v.high == -1) {
        return v.low >= 0x80000000LU;
    }
    
    return 0;

}

void parse_string_to_i64 (int64_s *val, const char *str) {

    unsigned long l_low, l_high;
    unsigned long res_low, res_mid;
    unsigned long carry1, carry2;
    
    unsigned long old_low;
    int base = 10, digit;
    
    char ch;
    
    val->low = 0;
    val->high = 0;
    
    /* 1. Skip leading whitespace */
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    /* 2. Detect Base */
    if (*str == '0') {
    
        base = 8;
        str++;
        
        if (*str == 'x' || *str == 'X') {
        
            base = 16;
            str++;
        
        }
    
    }
    
    /* 3. Process Digits */
    for (;;) {
    
        ch = *str;
        
        /* Determine digit value */
        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        } else if (base == 16 && ch >= 'a' && ch <= 'f') {
            digit = ch - 'a' + 10;
        } else if (base == 16 && ch >= 'A' && ch <= 'F') {
            digit = ch - 'A' + 10;
        } else {
            /* Not a valid digit for the current base */
            break; 
        }
        
        if (digit >= base) {
            break;
        }
        
        /* --- Math: val = (val * base) + digit --- */
        
        /* Multiply existing val->low by base */
        l_low = val->low & 0xFFFF;
        l_high = (val->low >> 16) & 0xFFFF;
        
        res_low = l_low * base;
        carry1 = res_low >> 16;
        
        res_mid = (l_high * base) + carry1;
        carry2 = res_mid >> 16;
        
        /* Update high part: (val->high * base) + carry from low part */
        val->high = (val->high * base) + carry2;
        
        /* Finalize low part */
        val->low = ((res_mid & 0xFFFF) << 16) | (res_low & 0xFFFF);
        
        /* Add the new digit */
        old_low = val->low;
        val->low += (unsigned long) digit;
        
        /* Handle carry from addition into the high word */
        if (val->low < old_low) {
            val->high++;
        }
        
        str++;
    
    }

}
