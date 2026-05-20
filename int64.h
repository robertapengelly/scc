/******************************************************************************
 * @file            int64.h
 *****************************************************************************/
#ifndef     _INT64_H
#define     _INT64_H

typedef struct {

    unsigned long low;
    unsigned long high;

} int64_s;

void add64 (int64_s *a, int64_s b);
void div64 (int64_s *a, int64_s b);
void mod64 (int64_s *a, int64_s b);
void mul64 (int64_s *a, int64_s b);
void shl64 (int64_s *a, int n);
void shr64 (int64_s *a, int n);
void sub64 (int64_s *a, int64_s b);

void and64 (int64_s *a, int64_s b);
void or64 (int64_s *a, int64_s b);
void xor64 (int64_s *a, int64_s b);

void mask64 (int64_s *a, int bits);
void trunc64 (int64_s *a);

void inc64 (int64_s *a);
void dec64 (int64_s *a);

void not64 (int64_s *a);
void neg64 (int64_s *a);

void zext64 (int64_s *dest, unsigned long input);
void sext64 (int64_s *dest, long input);

int compare64 (int64_s a, int64_s b);
int is_zero64 (int64_s a);

int eq64 (int64_s a, int64_s b);
int neq64 (int64_s a, int64_s b);

int lt64_unsigned (int64_s a, int64_s b);
int lte64_unsigned (int64_s a, int64_s b);

int lt64_signed (int64_s a, int64_s b);
int lte64_signed (int64_s a, int64_s b);

int gte64_unsigned (int64_s a, int64_s b);
int gte64_signed (int64_s a, int64_s b);

int gt64_unsigned (int64_s a, int64_s b);
int gt64_signed (int64_s a, int64_s b);

void ld_to_u64 (int64_s *out, long double x);
void ld_to_i64 (int64_s *out, long double x);

int int64_is_negative (int64_s v);
int int64_fits_uint (int64_s v);
int int64_fits_int (int64_s v);

void sign_extend_masked (int64_s *v, unsigned long mask);
void parse_string_to_i64 (int64_s *val, const char *str);

#define     U32_MASK                    ((((unsigned long) 0xFFFFU) << 16) | 0xFFFFU)

long double u64_to_ld (int64_s v);
long double i64_to_ld (int64_s v);

#endif      /* _INT64_H */
