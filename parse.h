/******************************************************************************
 * @file            parse.h
 *****************************************************************************/
#ifndef     _PARSE_H
#define     _PARSE_H

#include    "int64.h"
#include    "token.h"
#include    "vector.h"

#define     DATA_NONE                   0
#define     DATA_VOID                   4

#define     DATA_PTR                    (state->bits == 64 ? 8 : 4)
extern int anonymous_aggregate_owner_id;

extern enum token_kind last_found_member_calling_convention;
extern enum token_kind postfix_member_calling_convention;
extern enum token_kind parsed_calling_convention;

struct dll_alias_entry {

    char *target, *name;
    int dllexport;

};

extern struct vector vec_dllaliases;
extern struct vector vec_dllexports;

extern int parsed_dllexport;
extern int parsed_dllimport;
extern int parsed_noreturn;

extern int parsed_type_size;
extern int parsed_type_is_inline;
extern int parsed_type_is_void;
extern int parsed_type_is_unsigned;
extern int parsed_type_is_floating;
extern int parsed_type_only_qualifiers;
extern int parsed_type_is_packed;

extern int parsed_type_is_aggregate;
extern int parsed_type_has_tag;

extern char parsed_type_tag_name[128];
extern char last_cast_type_tag_name[128];

extern int last_cast_type_object_size;
extern int last_cast_type_is_floating;
extern int floating_rhs_result_in_eax_bool;

extern int parsed_type_is_array_typedef;
extern int parsed_type_array_element_size;

extern long parsed_type_array_count;

extern int declarator_depth;
extern int declarator_has_function;
extern int declarator_function_is_pointer;
extern int declarator_function_param_count;
extern int declarator_function_has_prototype;
extern int declarator_function_is_variadic;
extern int declarator_array_unsized;

extern int declarator_is_pointer;
extern int declarator_pointer_depth;
extern int declarator_has_array;

extern char declarator_alias_target[512];
extern int declarator_dllexport;
extern int declarator_dllimport;
extern int declarator_noreturn;

extern enum token_kind declarator_calling_convention;
extern int global_initializer_accept_symbol_addresses;

extern long declarator_array_count;
extern long declarator_first_array_count;
extern long declarator_last_array_count;

extern int declarator_array_dimensions;

#define     STORAGE_NONE                0
#define     STORAGE_EXTERN              1
#define     STORAGE_STATIC              2
#define     STORAGE_TYPEDEF             3

extern int parsed_storage_class;

#define     MAX_GLOBAL_INIT_FIELDS      262144
#define     MAX_AGG_FIELDS              512

extern int parsed_field_sizes[MAX_AGG_FIELDS];
extern int parsed_field_count;

#define     MAX_STRING_INIT_BYTES       4096
int _accept (enum token_kind k);

void compile_translation_unit32 (void);
void compile_translation_unit64 (void);

int token_is_sizeof_keyword (void);
int parse_sizeof_value (void);

int is_type_start (enum token_kind k);
int token_is_ms_int_type_name (void);
int token_starts_type_name (void);

int parse_constexpr_address_of_null_member (int64_s *out);
int parse_cast_type_name (int *out_size, int *out_is_unsigned, int *out_is_pointer);

int resolve_enum_constant (const char *name, int64_s *out);
void record_dllalias (const char *name, const char *target, int dllexport);

#define     DATA_CHAR                   (1 | (1 << 5))
#define     DATA_SHORT                  (2 | (1 << 6))
#define     DATA_INT                    (4 | (1 << 7))
#define     DATA_LONG                   (4 | (1 << 8))
#define     DATA_LLONG                  (8 | (1 << 9))
#define     DATA_FLOAT                  (4 | (1 << 10))
#define     DATA_DOUBLE                 (8 | (1 << 11))

struct enum_const_entry {

    char *name;
    int64_s value;

};

#define     MAX_ENUM_CONSTANTS          4096

struct member_info_entry {

    char *name;
    int offset;
    int size;
    int elem_size;
    int pointer_depth;
    
    int is_array;
    int is_floating;
    int is_unsigned;
    int is_packed;
    
    char *tag_name;
    char *owner_tag_name;
    
    enum token_kind calling_convention;
    int owner_size;

};

#define     MAX_MEMBER_INFOS            4096

extern struct member_info_entry member_infos[MAX_MEMBER_INFOS];
extern int member_info_count;

struct local_symbol {

    char *name;
    
    int size;
    int align;
    
    int is_floating;
    long offset;
    
    char *static_label;
    
    int is_static, is_unsigned;
    int is_array;
    int array_element_size;
    int array_dimensions;
    
    int pointer_depth;
    int pointed_size;
    int pointed_is_floating;
    int pointed_is_unsigned;
    
    char *pointed_tag_name;
    char *tag_name;

};

extern const char *last_found_member_tag_name;
extern int last_found_member_is_unsigned;

struct aggregate_tag_entry {

    char *name;
    
    int is_union;
    int size;
    
    int field_count;
    int field_sizes[MAX_AGG_FIELDS];

};

#define     MAX_LOCAL_SYMBOLS           1024

extern struct local_symbol local_symbols[MAX_LOCAL_SYMBOLS];
extern int local_symbol_count;

struct local_symbol *find_local_symbol (const char *name);

#define     MAX_GLOBAL_SYMBOLS          4096
int find_global_symbol (const char *name);

#define     GLOBAL_SYMBOL_OBJECT        1
#define     GLOBAL_SYMBOL_FUNCTION      2

struct global_symbol_entry {

    char *name;
    
    int is_unsigned, is_extern;
    int is_dllimport;
    int kind, size;
    
    int array_element_size;
    int is_array;
    int array_dimensions;
    
    long array_count;
    
    int is_floating;
    int pointer_depth;
    int pointed_size;
    int pointed_is_floating;
    int pointed_is_unsigned;
    int returns_void;
    int is_noreturn;
    
    int param_count;
    int has_prototype;
    int is_variadic;
    
    int param_sizes[128];
    int param_unsigneds[128];
    int param_floatings[128];
    int param_pointer_depths[128];
    
    int import_call_stack_bytes;
    int is_implicit;
    int extern_emitted;
    
    enum token_kind calling_convention;
    char *tag_name;

};

extern struct global_symbol_entry global_symbols[MAX_GLOBAL_SYMBOLS];
extern int global_symbol_count;

int get_global_symbol_param_pointer_depth (const char *name, int param_index);
int get_global_symbol_array_element_size (const char *name);
int get_global_symbol_pointer_depth (const char *name);
int get_global_symbol_pointed_size (const char *name);
int get_global_symbol_array (const char *name);
int get_global_symbol_size (const char *name);

int find_member_info_ex (const char *name, int *offset, int *size, int *elem_size, int *pointer_depth, int *is_array, int *is_floating);

#define     MAX_AGG_TAGS                256

extern struct aggregate_tag_entry aggregate_tags[MAX_AGG_TAGS];
extern int aggregate_tag_count;

struct aggregate_tag_entry *find_aggregate_tag (const char *name, int is_union);
void parse_type_spec (void);

#define     MAX_TYPEDEF_NAMES            512

struct typedef_entry {

    char *name;
    int size;
    
    int is_aggregate;
    int is_unsigned;
    int is_void;
    
    int field_count;
    int field_sizes[MAX_AGG_FIELDS];
    
    int array_element_size;
    int is_array;
    
    char *tag_name;
    long array_count;
    
    enum token_kind calling_convention;

};

extern struct typedef_entry typedef_names[MAX_TYPEDEF_NAMES];
extern int typedef_name_count;

extern int capture_declarator_name_location;
extern int captured_declarator_name_location;

extern const char *captured_declarator_name_start;
extern const char *captured_declarator_name_caret;

extern unsigned long captured_declarator_name_line;

#define     MAX_PENDING_PARAMS          128

struct pending_param {

    char *name;
    int align, size;
    
    int is_unsigned;
    int is_floating;
    
    int pointer_depth;
    int pointed_size;
    int pointed_is_floating;
    int pointed_is_unsigned;
    
    char *pointed_tag_name;
    int knr_declared;

};

extern struct pending_param pending_params[MAX_PENDING_PARAMS];

extern int pending_param_count;
extern int preserve_pending_params;

extern const char *last_declarator_name_start;
extern const char *last_declarator_name_caret;

extern unsigned long last_declarator_name_line;
int declarator_object_size (int base_size);

void parse_declarator_inner (char **out_name);
void parse_declarator (char **out_name);

void parse_direct_declarator (char **out_name);
void append_parsed_field (int size);
void clear_parsed_fields (void);

int expect (enum token_kind k, const char *what);
int is_string_token (void);

extern struct enum_const_entry enum_constants[MAX_ENUM_CONSTANTS];
extern int enum_constant_count;

struct typedef_entry *find_typedef_name (const char *name);
int token_is_ident (void);

extern int parsed_string_initializer_elem_size;
void parse_string_initializer_values (int64_s *values, int max_values, int *count);

void append_global_init_value (int64_s *values, int max_values, int *count, unsigned int value, int elem_size);
void clear_pending_params (void);

void skip_balanced_until (enum token_kind stop1, enum token_kind stop2, enum token_kind stop3);
int parse_constexpr_null_member_address_after_lparen (int64_s *out);

int find_member_info_ex_bounded (const char *name, int max_size, const char *owner_tag_name, int *offset, int *size, int *elem_size, int *pointer_depth, int *is_array, int *is_floating);
int type_alignment (int size);

char *take_ident (void);
int find_pending_param_from (const char *name, int start);

void add_pending_param (const char *name, int size, int align, int is_unsigned, int is_floating, int pointer_depth, int pointed_size, int pointed_is_unsigned);
long align_up_long (long value, int align);

#endif      /* _PARSE_H */
