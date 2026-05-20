/******************************************************************************
 * @file            parse.h
 *****************************************************************************/
#ifndef     _PARSE_H
#define     _PARSE_H

void compile_translation_unit (void);

int token_is_sizeof_keyword (void);
int parse_sizeof_value (void);

int token_starts_type_name (void);
int parse_cast_type_name (int *out_size, int *out_is_unsigned, int *out_is_pointer);

int parse_constexpr_address_of_null_member (int64_s *out);
int resolve_enum_constant (const char *name, int64_s *out);

#endif      /* _PARSE_H */
