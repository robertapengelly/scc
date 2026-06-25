/******************************************************************************
 * @file            parse.c
 *****************************************************************************/
#include    <stdlib.h>
#include    <string.h>

#include    "cc.h"
#include    "expr.h"
#include    "token.h"
#include    "lib.h"
#include    "parse.h"
#include    "int64.h"
#include    "report.h"
#include    "vector.h"

struct typedef_entry typedef_names[MAX_TYPEDEF_NAMES];
int typedef_name_count = 0;
int anonymous_aggregate_owner_id = 1;

enum token_kind last_found_member_calling_convention = TOK_EOF;
enum token_kind postfix_member_calling_convention = TOK_EOF;
enum token_kind parsed_calling_convention = TOK_EOF;

struct vector vec_dllexports = { 0 };
int parsed_dllexport = 0;
int parsed_dllimport = 0;

int parsed_type_size = DATA_NONE;
int parsed_type_is_inline = 0;
int parsed_type_is_void = 0;
int parsed_type_is_unsigned = 0;
int parsed_type_is_floating = 0;
int parsed_type_only_qualifiers = 0;
int parsed_type_is_packed = 0;

int parsed_type_is_aggregate = 0;
int parsed_type_has_tag = 0;

char parsed_type_tag_name[128];
char last_cast_type_tag_name[128];

int last_cast_type_object_size = 0;
int last_cast_type_is_floating = 0;
int floating_rhs_result_in_eax_bool = 0;

int parsed_type_is_array_typedef = 0;
int parsed_type_array_element_size = DATA_NONE;

long parsed_type_array_count = 1;

int declarator_depth = 0;
int declarator_has_function = 0;
int declarator_function_is_pointer = 0;
int declarator_function_param_count = 0;
int declarator_function_has_prototype = 0;
int declarator_function_is_variadic = 0;
int declarator_array_unsized = 0;

int declarator_is_pointer = 0;
int declarator_pointer_depth = 0;
int declarator_has_array = 0;

int declarator_dllexport = 0;
int declarator_dllimport = 0;

enum token_kind declarator_calling_convention = TOK_EOF;
int global_initializer_accept_symbol_addresses = 0;

long declarator_array_count = 1;
long declarator_first_array_count = 1;
long declarator_last_array_count = 1;

int declarator_array_dimensions = 0;
int parsed_storage_class = STORAGE_NONE;

int parsed_field_sizes[MAX_AGG_FIELDS];
int parsed_field_count = 0;

struct member_info_entry member_infos[MAX_MEMBER_INFOS];
int member_info_count = 0;

const char *last_found_member_tag_name = 0;
int last_found_member_is_unsigned = 0;

struct pending_param pending_params[MAX_PENDING_PARAMS];

int pending_param_count;
int preserve_pending_params;

const char *last_declarator_name_start = 0;
const char *last_declarator_name_caret = 0;

unsigned long last_declarator_name_line = 0;


int capture_declarator_name_location = 0;
int captured_declarator_name_location = 0;

const char *captured_declarator_name_start = 0;
const char *captured_declarator_name_caret = 0;

unsigned long captured_declarator_name_line = 0;

void clear_parsed_fields (void) {
    parsed_field_count = 0;
}

void append_parsed_field (int size) {

    if (parsed_field_count < MAX_AGG_FIELDS) {
        parsed_field_sizes[parsed_field_count++] = size;
    }

}

int is_string_token (void) {
    return tok.kind == TOK_PPSTR || tok.kind == TOK_LSTR;
}

int expect (enum token_kind k, const char *what) {

    if (tok.kind == k) {
    
        get_token ();
        return 1;
    
    }
    
    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "expected %s", what);
    return 0;

}

int declarator_object_size (int base_size) {

    if (declarator_has_array) {
    
        int elem_size = declarator_is_pointer ? DATA_PTR : base_size;
        
        if (declarator_array_count <= 0) {
            return elem_size;
        }
        
        return elem_size * declarator_array_count;
    
    }
    
    if (declarator_is_pointer) {
        return DATA_PTR;
    }
    
    return base_size;

}

struct enum_const_entry enum_constants[MAX_ENUM_CONSTANTS];
int enum_constant_count = 0;

struct typedef_entry *find_typedef_name (const char *name) {

    int i;
    
    if (!name) {
        return 0;
    }

    for (i = 0; i < typedef_name_count; i++) {
    
        if (strcmp (typedef_names[i].name, name) == 0) {
            return &typedef_names[i];
        }
    
    }
    
    return 0;

}

static void update_typedef_name_from_aggregate_tag (const char *name, int size, const int *field_sizes, int field_count) {

    struct typedef_entry *entry;
    int i;
    
    if (!name) {
        return;
    }
    
    entry = find_typedef_name (name);
    
    if (!entry) {
        return;
    }
    
    entry->size = size;
    entry->is_aggregate = 1;
    entry->is_void = 0;
    entry->is_array = 0;
    entry->array_count = 1;
    entry->array_element_size = DATA_NONE;
    entry->calling_convention = TOK_EOF;
    entry->field_count = 0;
    
    if (entry->tag_name) {
    
        free (entry->tag_name);
        entry->tag_name = 0;
    
    }
    
    if (name && *name) {
        entry->tag_name = xstrdup (name);
    }
    
    for (i = 0; i < field_count && i < MAX_AGG_FIELDS; i++) {
        entry->field_sizes[entry->field_count++] = field_sizes[i];
    }

}

static int is_current_typedef_name (void) {

    if (tok.kind != TOK_IDENT || !tok.ident) {
        return 0;
    }
    
    if (find_local_symbol (tok.ident)) {
        return 0;
    }
    
    return find_typedef_name (tok.ident) != 0;

}

static void load_aggregate_tag_fields (struct aggregate_tag_entry *entry) {

    int i;
    clear_parsed_fields ();
    
    if (!entry) {
        return;
    }
    
    parsed_type_size = entry->size;
    parsed_type_is_aggregate = 1;
    parsed_type_is_void = 0;
    parsed_type_has_tag = 1;
    
    for (i = 0; i < entry->field_count; i++) {
        append_parsed_field (entry->field_sizes[i]);
    }

}

static void save_aggregate_tag (const char *name, int is_union, int size, const int *field_sizes, int field_count) {

    struct aggregate_tag_entry *entry;
    int i;
    
    if (!name) {
        return;
    }
    
    entry = find_aggregate_tag (name, is_union);
    
    if (!entry) {
    
        if (aggregate_tag_count >= MAX_AGG_TAGS) {
            return;
        }
        
        entry = &aggregate_tags[aggregate_tag_count++];
        entry->name = xstrdup (name);
        entry->is_union = is_union;
    
    }
    
    entry->size = size;
    entry->field_count = 0;
    
    for (i = 0; i < field_count && i < MAX_AGG_FIELDS; i++) {
        entry->field_sizes[entry->field_count++] = field_sizes[i];
    }

}

static struct enum_const_entry *find_enum_constant (const char *name) {

    int i;
    
    if (!name) {
        return 0;
    }
    
    for (i = 0; i < enum_constant_count; i++) {
    
        if (strcmp (enum_constants[i].name, name) == 0) {
            return &enum_constants[i];
        }
    
    }
    
    return 0;

}

static void save_enum_constant (const char *name, int64_s value, const char *start, const char *caret) {

    struct enum_const_entry *entry;
    
    if (!name || !*name) {
        return;
    }
    
    entry = find_enum_constant (name);
    
    if (entry) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "duplicate enum constant '%s'", name);
        return;
    
    }
    
    if (enum_constant_count >= MAX_ENUM_CONSTANTS) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start, caret, "too many enum constants");
        return;
    
    }
    
    entry = &enum_constants[enum_constant_count++];
    entry->name = xstrdup (name);
    entry->value = value;

}

int token_is_ident (void) {
    return tok.kind == TOK_IDENT;
}

static void parse_enum_body (void) {

    int64_s value;
    zext64 (&value, 0);
    
    expect (TOK_LBRACE, "{");
    
    while (tok.kind != TOK_EOF && tok.kind != TOK_RBRACE) {
    
        if (!token_is_ident ()) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "expected enum constant");
            
            get_token ();
            continue;
        
        } else {
        
            char *name;
            
            const char *name_start;
            const char *name_caret;
            
            name = xstrdup (tok.ident);
            
            name_start = tok.start;
            name_caret = tok.caret;
            
            get_token ();
            
            if (_accept (TOK_ASSIGN)) {
            
                enum token_kind kill[4];
                
                kill[0] = TOK_COMMA;
                kill[1] = TOK_RBRACE;
                kill[2] = TOK_EOF;
                kill[3] = 0;
                
                value = expr_const64 (kill);
            
            }
            
            save_enum_constant (name, value, name_start, name_caret);
            free (name);
            
            inc64 (&value);
        
        }
        
        if (!_accept (TOK_COMMA)) {
            break;
        }
    
    }
    
    expect (TOK_RBRACE, "}");

}

static void parse_decl_modifier (void) {

    if (tok.kind == TOK_STDCALL) {
    
        if (parsed_calling_convention == TOK_EOF && declarator_calling_convention == TOK_EOF) {
            parsed_calling_convention = TOK_STDCALL;
        }
    
    } else if (tok.kind == TOK_DLLEXPORT) {
    
        if (parsed_dllexport) {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate '__dllexport'");
        } else if (parsed_dllimport) {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "'__dllexport' and '__dllimport' cannot both be specified");
        } else {
        
            declarator_dllexport = 1;
            parsed_dllexport = 1;
        
        }
    
    } else if (tok.kind == TOK_DLLIMPORT) {
    
        if (parsed_dllimport) {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate '__dllimport'");
        } else if (parsed_dllexport) {
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "'__dllexport' and '__dllimport' cannot both be specified");
        } else {
        
            declarator_dllimport = 1;
            parsed_dllimport = 1;
        
        }
    
    }
    
    get_token ();

}

void parse_declarator_inner (char **out_name) {

    while (tok.kind == TOK_STDCALL || tok.kind == TOK_DLLEXPORT || tok.kind == TOK_DLLIMPORT) {
    
        if (tok.kind == TOK_STDCALL) {
        
            if (declarator_calling_convention == TOK_EOF && parsed_calling_convention == TOK_EOF) {
                declarator_calling_convention = tok.kind;
            }
            
            get_token ();
        
        } else {
            parse_decl_modifier ();
        }
    
    }
    
    while (tok.kind == TOK_STAR) {
    
        declarator_is_pointer = 1;
        declarator_pointer_depth++;
        
        get_token ();
        
        while (tok.kind == TOK_CONST || tok.kind == TOK_VOLATILE || tok.kind == TOK_RESTRICT || tok.kind == TOK_STDCALL || tok.kind == TOK_DLLEXPORT || tok.kind == TOK_DLLIMPORT) {
        
            if (tok.kind == TOK_STDCALL) {
            
                if (declarator_calling_convention == TOK_EOF && parsed_calling_convention == TOK_EOF) {
                    declarator_calling_convention = tok.kind;
                }
                
                get_token ();
            
            } else if (tok.kind == TOK_DLLEXPORT || tok.kind == TOK_DLLIMPORT) {
                parse_decl_modifier ();
            } else {
                get_token ();
            }
        
        }
    
    }
    
    parse_direct_declarator (out_name);

}

void clear_pending_params (void) {

    int i;
    
    for (i = 0; i < pending_param_count; i++) {
    
        if (pending_params[i].name) {
        
            free (pending_params[i].name);
            pending_params[i].name = 0;
        
        }
        
        pending_params[i].size = 0;
        pending_params[i].align = 0;
        pending_params[i].is_unsigned = 0;
        pending_params[i].is_floating = 0;
        pending_params[i].pointer_depth = 0;
        pending_params[i].pointed_size = 0;
        pending_params[i].pointed_is_floating = 0;
        pending_params[i].pointed_is_unsigned = 0;
        pending_params[i].knr_declared = 0;
        
        if (pending_params[i].pointed_tag_name) {
        
            free (pending_params[i].pointed_tag_name);
            pending_params[i].pointed_tag_name = 0;
        
        }
    
    }
    
    pending_param_count = 0;

}

void parse_declarator (char **out_name) {

    int top_level = (declarator_depth == 0);
    
    int saved_capture_declarator_name_location = capture_declarator_name_location;
    int saved_captured_declarator_name_location = captured_declarator_name_location;

    const char *saved_captured_declarator_name_start = captured_declarator_name_start;
    const char *saved_captured_declarator_name_caret = captured_declarator_name_caret;

    enum token_kind saved_declarator_calling_convention = declarator_calling_convention;
    
    int saved_declarator_dllexport = declarator_dllexport;
    int saved_declarator_dllimport = declarator_dllimport;
    
    unsigned long saved_captured_declarator_name_line = captured_declarator_name_line;
    
    if (top_level) {
    
        if (!preserve_pending_params) {
            clear_pending_params ();
        }
        
        declarator_calling_convention = TOK_EOF;
        capture_declarator_name_location = 1;
        
        declarator_dllexport = 0;
        declarator_dllimport = 0;
        
        captured_declarator_name_location = 0;
        captured_declarator_name_start = 0;
        captured_declarator_name_caret = 0;
        captured_declarator_name_line = 0;
    
    }
    
    declarator_depth++;
    
    declarator_is_pointer = 0;
    declarator_pointer_depth = 0;
    declarator_has_array = 0;
    declarator_has_function = 0;
    declarator_function_is_pointer = 0;
    declarator_function_param_count = 0;
    declarator_function_has_prototype = 0;
    declarator_function_is_variadic = 0;
    declarator_array_unsized = 0;
    declarator_array_count = 1;
    declarator_first_array_count = 1;
    declarator_last_array_count = 1;
    declarator_array_dimensions = 0;
    
    parse_declarator_inner (out_name);
    
    if (top_level && parsed_type_is_array_typedef && !declarator_is_pointer && !declarator_has_array && !declarator_has_function) {
    
        declarator_has_array = 1;
        
        declarator_array_count = parsed_type_array_count > 0 ? parsed_type_array_count : 1;
        declarator_first_array_count = declarator_array_count;
        declarator_last_array_count = declarator_array_count;
        declarator_array_dimensions = 1;
        
        if (parsed_type_array_element_size > 0) {
            parsed_type_size = parsed_type_array_element_size;
        }
    
    }
    
    declarator_depth--;
    
    if (top_level) {
    
        if (captured_declarator_name_location) {
        
            last_declarator_name_line = captured_declarator_name_line;
            last_declarator_name_start = captured_declarator_name_start;
            last_declarator_name_caret = captured_declarator_name_caret;
        
        }
        
        capture_declarator_name_location = saved_capture_declarator_name_location;
        captured_declarator_name_location = saved_captured_declarator_name_location;
        captured_declarator_name_start = saved_captured_declarator_name_start;
        captured_declarator_name_caret = saved_captured_declarator_name_caret;
        captured_declarator_name_line = saved_captured_declarator_name_line;
    
    } else {
    
        declarator_calling_convention = saved_declarator_calling_convention;
        declarator_dllexport = saved_declarator_dllexport;
        declarator_dllimport = saved_declarator_dllimport;
    
    }

}


int _accept (enum token_kind k) {

    if (tok.kind == k) {
    
        get_token ();
        return 1;
    
    }
    
    return 0;

}

static const char *find_member_tag_name (const char *name) {

    int i;
    
    if (!name) {
        return 0;
    }
    
    for (i = member_info_count - 1; i >= 0; i--) {
    
        if (member_infos[i].name && strcmp (member_infos[i].name, name) == 0) {
            return member_infos[i].tag_name;
        }
    
    }
    
    return 0;

}

static int parse_sizeof_member_expr_size (int leading_stars, int *out_size) {

    int size = DATA_INT & 0x1f;
    int pointer_depth = 0;
    int pointed_size = 0;
    int is_array = 0;
    int array_element_size = 0;
    int final_pointer_depth = 0;
    int final_pointed_size = 0;
    int final_is_array = 0;
    int final_array_element_size = 0;
    
    struct local_symbol *local;
    
    if (tok.kind != TOK_IDENT) {
        return 0;
    }
    
    local = find_local_symbol (tok.ident);
    
    if (local) {
    
        size = local->size;
        
        pointer_depth = local->pointer_depth;
        pointed_size = local->pointed_size;
        
        is_array = local->is_array;
        array_element_size = local->array_element_size;
    
    } else if (find_global_symbol (tok.ident) >= 0) {
    
        size = get_global_symbol_size (tok.ident);
        
        pointer_depth = get_global_symbol_pointer_depth (tok.ident);
        pointed_size = get_global_symbol_pointed_size (tok.ident);
        
        is_array = get_global_symbol_array (tok.ident);
        array_element_size = get_global_symbol_array_element_size (tok.ident);
    
    } else {
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "unknown symbol '%s'", tok.ident ? tok.ident : "");
    }
    
    final_pointer_depth = pointer_depth;
    final_pointed_size = pointed_size;
    final_is_array = is_array;
    final_array_element_size = array_element_size;
    
    get_token ();
    
    while (tok.kind == TOK_ARROW || tok.kind == TOK_DOT) {
    
        int member_pointer_depth = 0;
        int member_pointed_size = 0;
        int member_offset = 0;
        int member_size = 0;
        int member_is_array = 0;
        
        const char *member_tag_name = 0;
        enum token_kind member_op = tok.kind;
        
        get_token ();
        
        if (tok.kind != TOK_IDENT) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret,
                            "expected member name after %s", member_op == TOK_ARROW ? "->" : ".");
            break;
        
        }
        
        if (find_member_info_ex (tok.ident, &member_offset, &member_size,
                                 &member_pointed_size, &member_pointer_depth,
                                 &member_is_array, 0)) {
        
            member_tag_name = last_found_member_tag_name ? last_found_member_tag_name : find_member_tag_name (tok.ident);
            size = member_size;
            
            final_pointer_depth = member_pointer_depth;
            final_pointed_size = member_pointed_size;
            
            final_is_array = member_is_array;
            final_array_element_size = member_is_array ? member_size : 0;
        
            if (final_pointer_depth > 0 && member_tag_name) {
            
                struct aggregate_tag_entry *entry = find_aggregate_tag (member_tag_name, 0);
                
                if (entry) {
                    final_pointed_size = entry->size;
                }
            
            }
        
        } else {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret,
                            "unknown member '%s'", tok.ident ? tok.ident : "");
        
        }
        
        get_token ();
    
    }
    
    while (tok.kind == TOK_LBRACK) {
    
        int depth = 1;
        get_token ();
        
        while (tok.kind != TOK_EOF && depth > 0) {
        
            if (tok.kind == TOK_LBRACK) {
                depth++;
            } else if (tok.kind == TOK_RBRACK) {
            
                depth--;
                
                if (depth == 0) {
                    break;
                }
            
            }
            
            get_token ();
        
        }
        
        if (tok.kind == TOK_RBRACK) {
            get_token ();
        }
        
        if (final_is_array && leading_stars > 0) {
        
            int remaining_stars = leading_stars - 1;
            int elem_size = final_array_element_size > 0 ? final_array_element_size :
                (final_pointer_depth > 0 ? DATA_PTR : final_pointed_size);
            
            /*
             * In expressions, an array object decays to a pointer to its first
             * element before the leading '*' operators are applied.  For an
             * array of pointers, sizeof(*array) is therefore pointer-sized,
             * not the size of the pointed-to base type.
             */
            size = elem_size > 0 ? elem_size : size;
            
            if (remaining_stars > 0 && final_pointer_depth >= remaining_stars) {
            
                int remaining_depth = final_pointer_depth - remaining_stars;
                size = remaining_depth > 0 ? DATA_PTR : final_pointed_size;
            
            }
            
            final_is_array = 0;
            final_pointer_depth = 0;
        
        } else if (final_pointer_depth > 0) {
         
            size = final_pointer_depth > 1 ? DATA_PTR : final_pointed_size;
            final_pointer_depth--;
        
        } else if (final_is_array && final_array_element_size > 0) {
            size = final_array_element_size;
        } else if (final_is_array && final_pointed_size > 0) {
            size = final_pointed_size;
        }
        
        final_is_array = 0;
    
    }
    
    if (leading_stars > 0 && final_is_array) {
    
        int remaining_stars = leading_stars - 1;
        int elem_size = final_array_element_size > 0 ? final_array_element_size :
            (final_pointer_depth > 0 ? DATA_PTR : final_pointed_size);
        
        /*
         * In an expression, an array object decays to a pointer to its first
         * element before unary '*' is applied.  This is needed for plain
         * sizeof(*array) as well as for forms that later include subscripts.
         * For example, with char *builtins[], sizeof(*builtins) is the size
         * of one array element, i.e. pointer-sized, not sizeof(char).
         */
        size = elem_size > 0 ? elem_size : size;
        
        if (remaining_stars > 0 && final_pointer_depth >= remaining_stars) {
        
            int remaining_depth = final_pointer_depth - remaining_stars;
            size = remaining_depth > 0 ? DATA_PTR : final_pointed_size;
        
        }
        
        final_is_array = 0;
        final_pointer_depth = 0;
    
    } else if (leading_stars > 0 && final_pointer_depth >= leading_stars) {
    
        int remaining_depth = final_pointer_depth - leading_stars;
        size = remaining_depth > 0 ? DATA_PTR : final_pointed_size;
    
    }
    
    if (size < 1) {
        size = DATA_INT & 0x1f;
    }
    
    *out_size = size;
    return 1;

}

static int parse_octal_escape_value (const char **ps) {

    const char *s = *ps;
    int value = 0, i;
    
    for (i = 0; i < 3; i++) {
    
        if (*s < '0' || *s > '7') {
            break;
        }
        
        value = value * 8 + (*s - '0');
        s++;
    
    }
    
    *ps = s;
    return value;

}

static int parse_hex_escape_value (const char **ps) {

    const char *s = *ps;
    
    int value = 0;
    int any = 0;
    
    while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')) {
    
        int digit;
        
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'f') {
            digit = *s - 'a' + 10;
        } else {
            digit = *s - 'A' + 10;
        }
        
        value = (value << 4) + digit;
        any = 1;
        
        s++;
    
    }
    
    if (!any) {
        value = 'x';
    }
    
    *ps = s;
    return value;

}

void append_global_init_value (int64_s *values, int max_values, int *count, unsigned int value, int elem_size) {

    unsigned int mask = 0xffU;
    
    if (elem_size == (DATA_SHORT & 0x1f)) {
        mask = 0xffffU;
    }
    
    if (*count < max_values) {
        zext64 (&values[*count], value & mask);
    }
    
    (*count)++;

}

void skip_balanced_until (enum token_kind stop1, enum token_kind stop2, enum token_kind stop3) {

    int paren = 0, brace = 0, brack = 0;
    
    while (tok.kind != TOK_EOF) {
    
        if (paren == 0 && brace == 0 && brack == 0) {
        
            if (tok.kind == stop1 || tok.kind == stop2 || tok.kind == stop3) {
                return;
            }
        
        }
        
        if (tok.kind == TOK_LPAREN) {
            paren++;
        } else if (tok.kind == TOK_RPAREN) {
        
            if (paren > 0) {
                paren--;
            } else {
            
                if (stop1 == TOK_RPAREN || stop2 == TOK_RPAREN || stop3 == TOK_RPAREN) {
                    return;
                }
            
            }
        
        } else if (tok.kind == TOK_LBRACE) {
            brace++;
        } else if (tok.kind == TOK_RBRACE) {
        
            if (brace > 0) {
                brace--;
            } else {
            
                if (stop1 == TOK_RPAREN || stop2 == TOK_RPAREN || stop3 == TOK_RPAREN) {
                    return;
                }
            
            }
        
        } else if (tok.kind == TOK_LBRACK) {
            brack++;
        } else if (tok.kind == TOK_RBRACK) {
        
            if (brack > 0) {
                brack--;
            } else {
            
                if (stop1 == TOK_RPAREN || stop2 == TOK_RPAREN || stop3 == TOK_RPAREN) {
                    return;
                }
            
            }
        
        }
        
        get_token ();
    
    }

}

int find_member_info_ex_bounded (const char *name, int max_size, const char *owner_tag_name, int *offset, int *size, int *elem_size, int *pointer_depth, int *is_array, int *is_floating) {

    int best;
    int i;
    
    last_found_member_tag_name = 0;
    last_found_member_is_unsigned = 0;
    last_found_member_calling_convention = TOK_EOF;
    
    if (!name) {
        return 0;
    }
    
    if (max_size <= 0 && (!owner_tag_name || !owner_tag_name[0])) {
        return find_member_info_ex (name, offset, size, elem_size, pointer_depth, is_array, is_floating);
    }
    
    best = -1;
    
    if (owner_tag_name && owner_tag_name[0]) {
    
        for (i = member_info_count - 1; i >= 0; i--) {
        
            if (member_infos[i].name && strcmp (member_infos[i].name, name) == 0
                && member_infos[i].owner_tag_name
                && strcmp (member_infos[i].owner_tag_name, owner_tag_name) == 0) {
            
                best = i;
                break;
            
            }
        
        }
    
    }
    
    if (best < 0) {
    
        for (i = member_info_count - 1; i >= 0; i--) {
        
            if (member_infos[i].name && strcmp (member_infos[i].name, name) == 0
                && (member_infos[i].owner_size == max_size || member_infos[i].owner_size == -max_size)) {
            
                if (best < 0 || member_infos[i].offset < member_infos[best].offset) {
                    best = i;
                }
            
            }
        
        }
    
    }
    
    if (best < 0) {
    
        for (i = member_info_count - 1; i >= 0; i--) {
        
            if (member_infos[i].name && strcmp (member_infos[i].name, name) == 0
                && member_infos[i].offset + member_infos[i].size <= max_size) {
            
                if (best < 0 || member_infos[i].offset < member_infos[best].offset) {
                    best = i;
                }
            
            }
        
        }
    
    }
    
    if (best >= 0) {
    
        last_found_member_tag_name = member_infos[best].tag_name;
        last_found_member_is_unsigned = member_infos[best].is_unsigned ? 1 : 0;
        last_found_member_calling_convention = member_infos[best].calling_convention;
        
        if (offset) {
            *offset = member_infos[best].offset;
        }
        
        if (size) {
            *size = member_infos[best].size;
        }
        
        if (elem_size) {
            *elem_size = member_infos[best].elem_size;
        }
        
        if (pointer_depth) {
            *pointer_depth = member_infos[best].pointer_depth;
        }
        
        if (is_array) {
            *is_array = member_infos[best].is_array;
        }
        
        if (is_floating) {
            *is_floating = member_infos[best].is_floating;
        }
        
        return 1;
    
    }
    
    return find_member_info_ex (name, offset, size, elem_size, pointer_depth, is_array, is_floating);

}

int parse_constexpr_null_member_address_after_lparen (int64_s *out) {

    char current_tag_name[128];
    
    int current_object_size;
    int need_closing_paren = 0;
    
    unsigned long total_offset = 0;
    
    if (tok.kind != TOK_LPAREN) {
        return 0;
    }
    
    get_token ();
    
    /*
     * Accept both common offsetof spellings:
     *
     *     &((T *)0)->member
     *     &(((T *)0)->member)
     *
     * The caller has already consumed the first '(' after '&'.  Older code
     * always expected another wrapper before the cast, so it rejected the
     * normal form and expr_const64() reported "integer constant expression
     * expected" at the '&'.
     */
    if (!is_type_start (tok.kind)) {
    
        if (tok.kind != TOK_LPAREN) {
            return 0;
        }
        
        get_token ();
        need_closing_paren = 1;
    
    }
    
    if (!parse_cast_type_name (0, 0, 0)) {
        return 0;
    }
    
    current_tag_name[0] = '\0';
    
    if (last_cast_type_tag_name[0]) {
    
        strncpy (current_tag_name, last_cast_type_tag_name, sizeof (current_tag_name) - 1);
        current_tag_name[sizeof (current_tag_name) - 1] = '\0';
    
    }
    
    current_object_size = last_cast_type_object_size;
    
    if (!((tok.kind == TOK_CINT || tok.kind == TOK_CUINT || tok.kind == TOK_CLONG ||
           tok.kind == TOK_CULONG || tok.kind == TOK_CLLONG || tok.kind == TOK_CULLONG) &&
          tok.val.i.low == 0 && tok.val.i.high == 0)) {
        return 0;
    }
    
    get_token ();
    expect (TOK_RPAREN, ")");
    
    while (tok.kind == TOK_ARROW || tok.kind == TOK_DOT) {
    
        enum token_kind member_op = tok.kind;
        char *member;
        
        const char *member_start;
        const char *member_caret;
        
        unsigned long member_line;
        
        int offset = 0;
        int size = DATA_INT & 0x1f;
        int elem_size = DATA_INT & 0x1f;
        int pointer_depth = 0;
        int is_array = 0;
        
        get_token ();
        
        member_start = tok.start;
        member_caret = tok.caret;
        member_line = get_line_number ();
        
        if (tok.kind != TOK_IDENT) {
        
            report_line_at (get_filename (), member_line, REPORT_ERROR, member_start, member_caret,
                            "expected member name after %s", member_op == TOK_ARROW ? "->" : ".");
            return 1;
        
        }
        
        member = xstrdup (tok.ident);
        get_token ();
        
        if (!find_member_info_ex_bounded (member, current_object_size,
                                          current_tag_name[0] ? current_tag_name : 0,
                                          &offset, &size, &elem_size, &pointer_depth, &is_array, 0)) {
        
            report_line_at (get_filename (), member_line, REPORT_ERROR, member_start, member_caret,
                            "unknown member '%s'", member);
            
            free (member);
            return 1;
        
        }
        
        free (member);
        total_offset += (unsigned long) offset;
        
        if (last_found_member_tag_name && last_found_member_tag_name[0]) {
        
            strncpy (current_tag_name, last_found_member_tag_name, sizeof (current_tag_name) - 1);
            current_tag_name[sizeof (current_tag_name) - 1] = '\0';
        
        } else {
            current_tag_name[0] = '\0';
        }
        
        while (tok.kind == TOK_LBRACK) {
        
            enum token_kind kill[2];
            int64_s index;
            
            get_token ();
            
            kill[0] = TOK_RBRACK;
            kill[1] = 0;
            
            index = expr_const64 (kill);
            
            expect (TOK_RBRACK, "]");
            total_offset += index.low * (unsigned long) (elem_size > 0 ? elem_size : (DATA_INT & 0x1f));
        
        }
        
        if (pointer_depth > 0) {
            current_object_size = elem_size > 0 ? elem_size : (DATA_PTR & 0x1f);
        } else if (is_array && elem_size > 0) {
            current_object_size = elem_size;
        } else {
            current_object_size = size;
        }
    
    }
    
    if (need_closing_paren) {
        expect (TOK_RPAREN, ")");
    }
    
    if (out) {
        zext64 (out, total_offset);
    }
    
    return 1;

}

int parsed_string_initializer_elem_size = DATA_CHAR & 0x1f;

void parse_string_initializer_values (int64_s *values, int max_values, int *count) {

    int elem_size = DATA_CHAR & 0x1f;
    
    while (is_string_token ()) {
    
        const char *s;
        int quote;
        
        s = tok.ident;
        
        if (tok.kind == TOK_LSTR && *s == 'L') {
        
            elem_size = DATA_SHORT & 0x1f;
            s++;
        
        }
        
        quote = *s;
        
        if (quote != '"') {
        
            get_token ();
            continue;
        
        }
        
        s++;
        
        while (*s && *s != '"') {
        
            unsigned int ch;
            
            if (*s == '\\') {
            
                s++;
                
                switch (*s) {
                
                    case 'a':
                    
                        ch = '\a';
                        
                        s++;
                        break;
                    
                    case 'b':
                    
                        ch = '\b';
                        
                        s++;
                        break;
                    
                    case 'f':
                    
                        ch = '\f';
                        
                        s++;
                        break;
                    
                    case 'n':
                    
                        ch = '\n';
                        
                        s++;
                        break;
                    
                    case 'r':
                    
                        ch = '\r';
                        
                        s++;
                        break;
                    
                    case 't':
                    
                        ch = '\t';
                        
                        s++;
                        break;
                    
                    case 'v':
                    
                        ch = '\v';
                        
                        s++;
                        break;
                    
                    case '\\':
                    
                        ch = '\\';
                        
                        s++;
                        break;
                    
                    case '\'':
                    
                        ch = '\'';
                        
                        s++;
                        break;
                    
                    case '"':
                    
                        ch = '"';
                        
                        s++;
                        break;
                    
                    case '?':
                    
                        ch = '?';
                        
                        s++;
                        break;
                    
                    case 'x':
                    
                        s++;
                        
                        ch = (unsigned int) parse_hex_escape_value (&s);
                        break;
                    
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7':
                    
                        ch = (unsigned int) parse_octal_escape_value (&s);
                        break;
                    
                    case '\0':
                    
                        ch = 0;
                        break;
                    
                    default:
                    
                        ch = (unsigned char) *s;
                        
                        if (*s) {
                            s++;
                        }
                        
                        break;
                
                }
            
            } else {
                ch = (unsigned char) *s++;
            }
            
            append_global_init_value (values, max_values, count, ch, elem_size);
        
        }
        
        get_token ();
    
    }
    
    append_global_init_value (values, max_values, count, 0, elem_size);
    parsed_string_initializer_elem_size = elem_size;

}

int parse_sizeof_value (void) {

    int saved_type_size = parsed_type_size;
    int saved_storage_class = parsed_storage_class;
    int saved_is_aggregate = parsed_type_is_aggregate;
    int saved_is_void = parsed_type_is_void;
    int saved_is_unsigned = parsed_type_is_unsigned;
    int saved_is_floating = parsed_type_is_floating;
    int saved_has_tag = parsed_type_has_tag;
    int saved_is_inline = parsed_type_is_inline;
    int saved_field_count = parsed_field_count;
    int saved_fields[MAX_AGG_FIELDS];
    
    int saved_declarator_is_pointer = declarator_is_pointer;
    int saved_declarator_has_array = declarator_has_array;
    int saved_declarator_has_function = declarator_has_function;
    int saved_declarator_array_unsized = declarator_array_unsized;
    int saved_declarator_array_dimensions = declarator_array_dimensions;
    
    long saved_declarator_array_count = declarator_array_count;
    long saved_declarator_first_array_count = declarator_first_array_count;
    
    int size = DATA_INT & 0x1f, i;
    
    for (i = 0; i < saved_field_count && i < MAX_AGG_FIELDS; i++) {
        saved_fields[i] = parsed_field_sizes[i];
    }
    
    if (_accept (TOK_LPAREN)) {
    
        if (is_type_start (tok.kind)) {
        
            char *name = 0;
            int base_size;
            
            parse_type_spec ();
            base_size = parsed_type_size;
            
            /*
             * sizeof(type-name) may not contain a declarator, for example
             * sizeof(FILE).  parse_declarator() normally clears these flags,
             * but when the next token is ')' it is not called.  Do not let a
             * previous declaration such as FILE **__gtin(void) make this
             * type-name look like a pointer and collapse the size to 4.
             */
            declarator_is_pointer = 0;
            declarator_pointer_depth = 0;
            declarator_has_array = 0;
            declarator_has_function = 0;
            declarator_function_is_pointer = 0;
            declarator_function_param_count = 0;
            declarator_function_has_prototype = 0;
            declarator_function_is_variadic = 0;
            declarator_array_unsized = 0;
            declarator_array_count = 1;
            declarator_first_array_count = 1;
            declarator_last_array_count = 1;
            
            if (tok.kind != TOK_RPAREN) {
                parse_declarator (&name);
            }
            
            size = declarator_object_size (base_size);
            
            if (name) {
                free (name);
            }
            
            expect (TOK_RPAREN, ")");
        
        } else if (is_string_token ()) {
        
            int64_s values[MAX_STRING_INIT_BYTES];
            int value_count = 0;
            
            parse_string_initializer_values (values, MAX_STRING_INIT_BYTES, &value_count);
            size = value_count;
            expect (TOK_RPAREN, ")");
        
        } else {
        
            int depth = 1;
            int first = 1;
            int leading_stars = 0;
            
            while (tok.kind != TOK_EOF && depth > 0) {
            
                if (first) {
                
                    while (tok.kind == TOK_STAR || tok.kind == TOK_AMPER || tok.kind == TOK_PLUS || tok.kind == TOK_MINUS) {
                    
                        if (tok.kind == TOK_STAR) {
                            leading_stars++;
                        }
                        
                        get_token ();
                    
                    }
                
                }
                
                if (first && tok.kind == TOK_IDENT) {
                
                    parse_sizeof_member_expr_size (leading_stars, &size);
                    
                    first = 0;
                    continue;
                
                }
                
                if (first && tok.kind == TOK_LPAREN && leading_stars > 0) {
                
                    get_token ();
                    
                    if (tok.kind == TOK_IDENT) {
                        parse_sizeof_member_expr_size (leading_stars, &size);
                    }
                    
                    while (tok.kind != TOK_EOF && tok.kind != TOK_RPAREN) {
                        get_token ();
                    }
                    
                    if (tok.kind == TOK_RPAREN) {
                        get_token ();
                    }
                    
                    first = 0;
                    continue;
                
                }
                
                first = 0;
                
                if (tok.kind == TOK_LPAREN) {
                    depth++;
                } else if (tok.kind == TOK_RPAREN) {
                
                    depth--;
                    
                    if (depth == 0) {
                        break;
                    }
                
                }
                
                get_token ();
            
            }
            
            expect (TOK_RPAREN, ")");
        
        }
    
    } else {
    
        int leading_stars = 0;
        
        while (tok.kind == TOK_STAR || tok.kind == TOK_AMPER || tok.kind == TOK_PLUS || tok.kind == TOK_MINUS) {
        
            if (tok.kind == TOK_STAR) {
                leading_stars++;
            }
            
            get_token ();
        
        }
        
        if (is_string_token ()) {
        
            int64_s values[MAX_STRING_INIT_BYTES];
            int value_count = 0;
            
            parse_string_initializer_values (values, MAX_STRING_INIT_BYTES, &value_count);
            size = value_count;
        
        } else if (tok.kind == TOK_IDENT) {
            parse_sizeof_member_expr_size (leading_stars, &size);
        } else if (tok.kind == TOK_LPAREN) {
        
            int depth = 1;
            
            get_token ();
            
            while (tok.kind != TOK_EOF && depth > 0) {
            
                if (tok.kind == TOK_LPAREN) {
                    depth++;
                } else if (tok.kind == TOK_RPAREN) {
                
                    depth--;
                    
                    if (depth == 0) {
                        break;
                    }
                
                }
                
                get_token ();
            
            }
            
            expect (TOK_RPAREN, ")");
        
        } else if (tok.kind != TOK_EOF) {
            get_token ();
        }
        
        while (tok.kind == TOK_LBRACK || tok.kind == TOK_LPAREN) {
        
            enum token_kind close = (tok.kind == TOK_LBRACK) ? TOK_RBRACK : TOK_RPAREN;
            int depth = 1;
            
            get_token ();
            
            while (tok.kind != TOK_EOF && depth > 0) {
            
                if ((close == TOK_RBRACK && tok.kind == TOK_LBRACK) || (close == TOK_RPAREN && tok.kind == TOK_LPAREN)) {
                    depth++;
                } else if (tok.kind == close) {
                
                    depth--;
                    
                    if (depth == 0) {
                        break;
                    }
                
                }
                
                get_token ();
            
            }
            
            expect (close, (close == TOK_RBRACK) ? "]" : ")");
        
        }
    
    }
    
    parsed_type_size = saved_type_size;
    parsed_storage_class = saved_storage_class;
    parsed_type_is_aggregate = saved_is_aggregate;
    parsed_type_is_void = saved_is_void;
    parsed_type_is_unsigned = saved_is_unsigned;
    parsed_type_is_floating = saved_is_floating;
    parsed_type_has_tag = saved_has_tag;
    parsed_type_is_inline = saved_is_inline;
    
    clear_parsed_fields ();
    
    for (i = 0; i < saved_field_count && i < MAX_AGG_FIELDS; i++) {
        append_parsed_field (saved_fields[i]);
    }
    
    declarator_is_pointer = saved_declarator_is_pointer;
    declarator_has_array = saved_declarator_has_array;
    declarator_has_function = saved_declarator_has_function;
    declarator_array_unsized = saved_declarator_array_unsized;
    declarator_array_count = saved_declarator_array_count;
    declarator_first_array_count = saved_declarator_first_array_count;
    declarator_array_dimensions = saved_declarator_array_dimensions;
    
    if (size < 1) {
        size = DATA_INT & 0x1f;
    }
    
    return size;

}

int parse_constexpr_address_of_null_member (int64_s *out) {

    if (tok.kind != TOK_AMPER) {
        return 0;
    }
    
    get_token ();
    
    if (tok.kind != TOK_LPAREN) {
        return 0;
    }
    
    get_token ();
    
    if (parse_constexpr_null_member_address_after_lparen (out)) {
        return 1;
    }
    
    return 0;

}

int token_is_sizeof_keyword (void) {
    return tok.kind == TOK_SIZEOF;
}

int token_is_ms_int_type_name (void) {

    if (tok.kind != TOK_IDENT || !tok.ident) {
        return 0;
    }
    
    return  strcmp (tok.ident, "__int8") == 0 || strcmp (tok.ident, "__int16") == 0 ||
                strcmp (tok.ident, "__int32") == 0 || strcmp (tok.ident, "__int64") == 0;

}

int is_type_start (enum token_kind k) {

    switch (k) {
    
        case TOK_IDENT:
        
            if (token_is_ms_int_type_name ()) {
                return 1;
            }
            
            if (is_current_typedef_name ()) {
                return 1;
            }
            
            return 0;
        
        case TOK_AUTO:          case TOK_REGISTER:      case TOK_STATIC:
        case TOK_DLLEXPORT:     case TOK_DLLIMPORT:     case TOK_PACKED:
        case TOK_EXTERN:        case TOK_TYPEDEF:       case TOK_INLINE:
        case TOK_CONST:         case TOK_VOLATILE:      case TOK_RESTRICT:
        case TOK_SIGNED:        case TOK_UNSIGNED:
        case TOK_SHORT:         case TOK_LONG:
        case TOK_CHAR:          case TOK_INT:           case TOK_VOID:
        case TOK_FLOAT:         case TOK_DOUBLE:
        case TOK_STRUCT:        case TOK_UNION:         case TOK_ENUM:
        
            return 1;
        
        default:
        
            break;
    
    }
    
    return 0;

}

int token_starts_type_name (void) {
    return is_type_start (tok.kind);
}

int parse_cast_type_name (int *out_size, int *out_is_unsigned, int *out_is_pointer) {

    int saved_type_size = parsed_type_size;
    int saved_storage_class = parsed_storage_class;
    int saved_is_aggregate = parsed_type_is_aggregate;
    int saved_is_void = parsed_type_is_void;
    int saved_is_unsigned = parsed_type_is_unsigned;
    int saved_is_floating = parsed_type_is_floating;
    int saved_has_tag = parsed_type_has_tag;
    int saved_is_inline = parsed_type_is_inline;
    int saved_field_count = parsed_field_count;
    int saved_fields[MAX_AGG_FIELDS];
    
    int saved_declarator_is_pointer = declarator_is_pointer;
    int saved_declarator_has_array = declarator_has_array;
    int saved_declarator_has_function = declarator_has_function;
    int saved_declarator_array_unsized = declarator_array_unsized;
    int saved_declarator_array_dimensions = declarator_array_dimensions;
    
    long saved_declarator_array_count = declarator_array_count;
    long saved_declarator_first_array_count = declarator_first_array_count;
    
    char *name = 0;
    int size = DATA_INT & 0x1f;
    int is_unsigned = 0;
    int is_pointer = 0;
    int ok = 0;
    int i;
    
    for (i = 0; i < saved_field_count && i < MAX_AGG_FIELDS; i++) {
        saved_fields[i] = parsed_field_sizes[i];
    }
    
    if (is_type_start (tok.kind)) {
    
        declarator_is_pointer = 0;
        declarator_pointer_depth = 0;
        declarator_has_array = 0;
        declarator_has_function = 0;
        declarator_array_unsized = 0;
        declarator_array_count = 0;
        
        parse_type_spec ();
        
        size = parsed_type_size & 0x1f;
        is_unsigned = parsed_type_is_unsigned;
        
        if (tok.kind != TOK_RPAREN) {
            parse_declarator (&name);
        }
        
        if (declarator_is_pointer) {
        
            size = DATA_PTR;
            
            is_unsigned = 1;
            is_pointer = 1;
        
        }
        
        if (tok.kind == TOK_RPAREN) {
        
            get_token ();
            ok = 1;
        
        } else {
            expect (TOK_RPAREN, ")");
        }
        
        if (name) {
            free (name);
        }
    
    }
    
    last_cast_type_tag_name[0] = '\0';
    last_cast_type_object_size = parsed_type_size & 0x1f;
    last_cast_type_is_floating = (is_pointer ? 0 : parsed_type_is_floating);
    
    if (parsed_type_tag_name[0]) {
    
        strncpy (last_cast_type_tag_name, parsed_type_tag_name, sizeof (last_cast_type_tag_name) - 1);
        last_cast_type_tag_name[sizeof (last_cast_type_tag_name) - 1] = '\0';
    
    }
    
    if (parsed_type_is_aggregate && parsed_type_size > 0) {
        last_cast_type_object_size = parsed_type_size;
    }
    
    parsed_type_size = saved_type_size;
    parsed_storage_class = saved_storage_class;
    parsed_type_is_aggregate = saved_is_aggregate;
    parsed_type_is_void = saved_is_void;
    parsed_type_is_unsigned = saved_is_unsigned;
    parsed_type_is_floating = saved_is_floating;
    parsed_type_has_tag = saved_has_tag;
    parsed_type_is_inline = saved_is_inline;
    
    clear_parsed_fields ();
    
    for (i = 0; i < saved_field_count && i < MAX_AGG_FIELDS; i++) {
        parsed_field_sizes[i] = saved_fields[i];
    }
    
    parsed_field_count = saved_field_count;
    
    declarator_is_pointer = saved_declarator_is_pointer;
    declarator_has_array = saved_declarator_has_array;
    declarator_has_function = saved_declarator_has_function;
    declarator_array_unsized = saved_declarator_array_unsized;
    declarator_array_count = saved_declarator_array_count;
    declarator_first_array_count = saved_declarator_first_array_count;
    declarator_array_dimensions = saved_declarator_array_dimensions;
    
    if (out_size) {
        *out_size = size;
    }
    
    if (out_is_unsigned) {
        *out_is_unsigned = is_unsigned;
    }
    
    if (out_is_pointer) {
        *out_is_pointer = is_pointer;
    }
    
    return ok;

}

int resolve_enum_constant (const char *name, int64_s *out) {

    struct enum_const_entry *entry = find_enum_constant (name);
    
    if (!entry) {
        return 0;
    }
    
    if (out) {
        *out = entry->value;
    }
    
    return 1;

}

struct local_symbol local_symbols[MAX_LOCAL_SYMBOLS];
int local_symbol_count = 0;

struct local_symbol *find_local_symbol (const char *name) {

    int i;
    
    if (!name) {
        return 0;
    }
    
    for (i = local_symbol_count - 1; i >= 0; i--) {
    
        if (local_symbols[i].name && strcmp (local_symbols[i].name, name) == 0) {
            return &local_symbols[i];
        }
    
    }
    
    return 0;

}

struct global_symbol_entry global_symbols[MAX_GLOBAL_SYMBOLS];
int global_symbol_count = 0;

int find_global_symbol (const char *name) {

    int i;
    
    if (!name) {
        return -1;
    }
    
    for (i = 0; i < global_symbol_count; i++) {
    
        if (strcmp (global_symbols[i].name, name) == 0) {
            return i;
        }
    
    }
    
    return -1;

}

int get_global_symbol_size (const char *name) {

    int i = find_global_symbol (name);
    
    if (i >= 0 && global_symbols[i].size > 0) {
        return global_symbols[i].size;
    }
    
    return DATA_INT & 0x1f;

}

int get_global_symbol_param_pointer_depth (const char *name, int param_index) {

    int i = find_global_symbol (name);
    
    if (i >= 0 && param_index >= 0 && param_index < 128) {
       return global_symbols[i].param_pointer_depths[param_index];
    }
    
    return 0;

}

int get_global_symbol_pointer_depth (const char *name) {

    int i = find_global_symbol (name);
    
    if (i >= 0) {
        return global_symbols[i].pointer_depth;
    }
    
    return 0;

}

int get_global_symbol_pointed_size (const char *name) {

    int i = find_global_symbol (name);
    
    if (i >= 0 && global_symbols[i].pointed_size > 0) {
        return global_symbols[i].pointed_size;
    }
    
    return DATA_INT & 0x1f;

}

int get_global_symbol_array (const char *name) {

    int i = find_global_symbol (name);
    
    if (i >= 0) {
        return global_symbols[i].is_array;
    }
    
    return 0;

}

int get_global_symbol_array_element_size (const char *name) {

    int i = find_global_symbol (name);
    
    if (i >= 0) {
    
        if (global_symbols[i].array_element_size > 0) {
            return global_symbols[i].array_element_size;
        }
        
        if (global_symbols[i].tag_name && global_symbols[i].tag_name[0]) {
        
            struct aggregate_tag_entry *entry = find_aggregate_tag (global_symbols[i].tag_name, 0);
            
            if (entry && entry->size > 0) {
                return entry->size;
            }
        
        }
    
    }
    
    return 0;

}

int find_member_info_ex (const char *name, int *offset, int *size, int *elem_size, int *pointer_depth, int *is_array, int *is_floating) {

    int best;
    int i;
    
    last_found_member_tag_name = 0;
    last_found_member_is_unsigned = 0;
    last_found_member_calling_convention = TOK_EOF;
    
    if (!name) {
        return 0;
    }
    
    best = -1;
    
    for (i = member_info_count - 1; i >= 0; i--) {
    
        if (member_infos[i].name && strcmp (member_infos[i].name, name) == 0) {
        
            best = i;
            break;
        
        }
    
    }

    if (best < 0) {
        return 0;
    }
    
    last_found_member_tag_name = member_infos[best].tag_name;
    last_found_member_is_unsigned = member_infos[best].is_unsigned ? 1 : 0;
    last_found_member_calling_convention = member_infos[best].calling_convention;

    if (offset) {
        *offset = member_infos[best].offset;
    }
    
    if (size) {
        *size = member_infos[best].size;
    }
    
    if (elem_size) {
        *elem_size = member_infos[best].elem_size;
    }
    
    if (pointer_depth) {
        *pointer_depth = member_infos[best].pointer_depth;
    }
    
    if (is_array) {
        *is_array = member_infos[best].is_array;
    }
    
    if (is_floating) {
        *is_floating = member_infos[best].is_floating;
    }
    
    return 1;

}

struct aggregate_tag_entry aggregate_tags[MAX_AGG_TAGS];
int aggregate_tag_count = 0;

struct aggregate_tag_entry *find_aggregate_tag (const char *name, int is_union) {

    int i;
    
    if (!name) {
        return 0;
    }
    
    for (i = 0; i < aggregate_tag_count; i++) {
    
        if (aggregate_tags[i].is_union == is_union && strcmp (aggregate_tags[i].name, name) == 0) {
            return &aggregate_tags[i];
        }
    
    }
    
    return 0;

}

static void load_typedef_name (struct typedef_entry *entry) {

    int i;
    clear_parsed_fields ();
    
    if (!entry) {
    
        parsed_type_size = DATA_INT & 0x1f;
        
        parsed_type_is_aggregate = 0;
        parsed_type_is_unsigned = 0;
        parsed_type_is_void = 0;
        parsed_type_is_floating = 0;
        parsed_type_is_array_typedef = 0;
        parsed_type_array_count = 1;
        parsed_type_array_element_size = DATA_NONE;
        parsed_calling_convention = TOK_EOF;
        
        append_parsed_field (DATA_INT & 0x1f);
        return;
    
    }
    
    parsed_type_size = entry->size;
    
    parsed_type_is_aggregate = entry->is_aggregate;
    parsed_type_is_unsigned = entry->is_unsigned;
    parsed_type_is_void = entry->is_void;
    parsed_type_is_floating = 0;
    parsed_type_is_array_typedef = entry->is_array;
    parsed_type_array_count = entry->array_count > 0 ? entry->array_count : 1;
    parsed_type_array_element_size = entry->array_element_size > 0 ? entry->array_element_size : DATA_INT;
    parsed_calling_convention = entry->calling_convention;
    
    parsed_type_has_tag = 0;
    parsed_type_tag_name[0] = '\0';
    
    if (entry->tag_name && entry->tag_name[0]) {
        
        strncpy (parsed_type_tag_name, entry->tag_name, sizeof (parsed_type_tag_name) - 1);
        parsed_type_tag_name[sizeof (parsed_type_tag_name) - 1] = '\0';
        
        parsed_type_has_tag = 1;
    
    }
    
    for (i = 0; i < entry->field_count; i++) {
        append_parsed_field (entry->field_sizes[i]);
    }

}

static void remember_member_info_ex (const char *name, int offset, int size, int elem_size, int pointer_depth, int is_array, int is_floating, int is_packed) {

    if (!name) {
        return;
    }
    
    if (member_info_count >= MAX_MEMBER_INFOS) {
        return;
    }
    
    member_infos[member_info_count].name = xstrdup (name);
    member_infos[member_info_count].offset = offset;
    member_infos[member_info_count].size = size;
    member_infos[member_info_count].elem_size = elem_size > 0 ? elem_size : size;
    member_infos[member_info_count].pointer_depth = pointer_depth;
    member_infos[member_info_count].is_array = is_array;
    member_infos[member_info_count].is_floating = is_floating ? 1 : 0;
    member_infos[member_info_count].is_unsigned = parsed_type_is_unsigned ? 1 : 0;
    member_infos[member_info_count].is_packed = is_packed ? 1 : 0;
    member_infos[member_info_count].calling_convention = (declarator_calling_convention != TOK_EOF) ? declarator_calling_convention : parsed_calling_convention;
    member_infos[member_info_count].tag_name = parsed_type_tag_name[0] ? xstrdup (parsed_type_tag_name) : 0;
    member_infos[member_info_count].owner_size = 0;
    member_infos[member_info_count].owner_tag_name = 0;
    
    member_info_count++;

}

static int fields_storage_size (const int *sizes, int count) {

    int total = 0, i;
    
    for (i = 0; i < count; i++) {
    
        if (sizes[i] < 0) {
            total += -sizes[i];
        } else {
            total += sizes[i];
        }
    
    }
    
    return total;

}

void add_pending_param (const char *name, int size, int align, int is_unsigned, int is_floating, int pointer_depth, int pointed_size, int pointed_is_unsigned) {

    if (pointer_depth > 0 && parsed_type_is_aggregate && parsed_type_size > (DATA_PTR & 0x1f) && pointed_size <= (DATA_PTR & 0x1f)) {
        pointed_size = parsed_type_size;
    }
    
    if (pending_param_count >= MAX_PENDING_PARAMS) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "too many function parameters");
        return;
    
    }
    
    if (size < 1) {
        size = DATA_INT & 0x1f;
    }
    
    if (align < 1) {
        align = type_alignment (size);
    }
    
    pending_params[pending_param_count].name = (name && *name) ? xstrdup (name) : 0;
    pending_params[pending_param_count].size = size;
    pending_params[pending_param_count].align = align;
    pending_params[pending_param_count].is_unsigned = is_unsigned ? 1 : 0;
    pending_params[pending_param_count].is_floating = is_floating ? 1 : 0;
    pending_params[pending_param_count].knr_declared = 0;
    pending_params[pending_param_count].pointer_depth = pointer_depth;
    pending_params[pending_param_count].pointed_size = pointed_size > 0 ? pointed_size : 0;
    pending_params[pending_param_count].pointed_is_floating = pointer_depth > 0 ? (parsed_type_is_floating ? 1 : 0) : 0;
    pending_params[pending_param_count].pointed_is_unsigned = pointer_depth > 0 ? (pointed_is_unsigned ? 1 : 0) : 0;
    pending_params[pending_param_count].pointed_tag_name = (pointer_depth > 0 && parsed_type_tag_name[0]) ? xstrdup (parsed_type_tag_name) : 0;
    
    pending_param_count++;

}

int find_pending_param_from (const char *name, int start) {

    int i;
    
    if (!name) {
        return -1;
    }
    
    if (start < 0) {
        start = 0;
    }
    
    for (i = start; i < pending_param_count; i++) {
    
        if (pending_params[i].name && strcmp (pending_params[i].name, name) == 0) {
            return i;
        }
    
    }
    
    return -1;

}

char *take_ident (void) {

    char *name;
    
    if (!token_is_ident ()) {
        return 0;
    }
    
    name = xstrdup (tok.ident);
    
    last_declarator_name_line = get_line_number ();
    last_declarator_name_start = tok.start;
    last_declarator_name_caret = tok.caret;
    
    if (capture_declarator_name_location && !captured_declarator_name_location) {
    
        captured_declarator_name_location = 1;
        captured_declarator_name_line = last_declarator_name_line;
        captured_declarator_name_start = last_declarator_name_start;
        captured_declarator_name_caret = last_declarator_name_caret;
    
    }
    
    get_token ();
    return name;

}

long align_up_long (long value, int align) {

    long mask;
    
    if (align <= 1) {
        return value;
    }
    
    mask = (long) align - 1;
    return (value + mask) & ~mask;

}

int type_alignment (int size) {

    size &= 0x1f;
    
    if (size <= 1) {
        return 1;
    }
    
    if (size == 2) {
        return 2;
    }
    
    if (size >= DATA_PTR) {
        return DATA_PTR;
    }
    
    return 4;

}

void parse_type_spec (void) {

    int saw = 0;
    
    int saw_float = 0;
    int saw_double = 0;
    int saw_void = 0;
    
    int saw_signed = 0;
    int saw_unsigned = 0;
    int saw_real_type = 0;
    
    parsed_storage_class = STORAGE_NONE;
    parsed_type_is_aggregate = 0;
    parsed_type_is_unsigned = 0;
    parsed_type_has_tag = 0;
    parsed_type_tag_name[0] = '\0';
    parsed_type_is_array_typedef = 0;
    parsed_type_array_count = 1;
    parsed_type_array_element_size = DATA_NONE;
    parsed_type_is_inline = 0;
    parsed_type_is_void = 0;
    parsed_type_only_qualifiers = 0;
    parsed_type_is_packed = 0;
    parsed_type_size = DATA_NONE;
    
    parsed_calling_convention = TOK_EOF;
    
    if (declarator_depth == 0) {
        declarator_calling_convention = TOK_EOF;
    }
    
    parsed_dllexport = 0;
    parsed_dllimport = 0;
    
    clear_parsed_fields ();
    
    while (tok.kind == TOK_AUTO || tok.kind == TOK_REGISTER || tok.kind == TOK_STATIC ||
           tok.kind == TOK_EXTERN || tok.kind == TOK_TYPEDEF || tok.kind == TOK_CONST ||
           tok.kind == TOK_PACKED || tok.kind == TOK_VOLATILE || tok.kind == TOK_SIGNED || tok.kind == TOK_UNSIGNED ||
           tok.kind == TOK_SHORT || tok.kind == TOK_LONG || tok.kind == TOK_CHAR ||
           tok.kind == TOK_INT || tok.kind == TOK_VOID || tok.kind == TOK_FLOAT ||
           tok.kind == TOK_DOUBLE || tok.kind == TOK_INLINE || tok.kind == TOK_RESTRICT ||
           tok.kind == TOK_DLLEXPORT || tok.kind == TOK_DLLIMPORT || tok.kind == TOK_STDCALL || token_is_ms_int_type_name ()) {
        
        saw = 1;
        
        if (tok.kind == TOK_SIGNED || tok.kind == TOK_UNSIGNED ||
            tok.kind == TOK_SHORT || tok.kind == TOK_LONG || tok.kind == TOK_CHAR ||
            tok.kind == TOK_INT || tok.kind == TOK_VOID || tok.kind == TOK_FLOAT ||
            tok.kind == TOK_DOUBLE || token_is_ms_int_type_name ()) {
            saw_real_type = 1;
        }
        
        if (tok.kind == TOK_STDCALL || tok.kind == TOK_DLLEXPORT || tok.kind == TOK_DLLIMPORT) {
        
            parse_decl_modifier ();
            continue;
        
        } else if (tok.kind == TOK_EXTERN) {
        
            if (parsed_storage_class == STORAGE_EXTERN) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'extern'");
            } else if (parsed_storage_class != STORAGE_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple storage classes in declaration specifiers");
            } else {
                parsed_storage_class = STORAGE_EXTERN;
            }
        
        } else if (tok.kind == TOK_STATIC) {
        
            if (parsed_storage_class == STORAGE_STATIC) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'static'");
            } else if (parsed_storage_class != STORAGE_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple storage classes in declaration specifiers");
            } else {
                parsed_storage_class = STORAGE_STATIC;
            }
        
        } else if (tok.kind == TOK_TYPEDEF) {
        
            if (parsed_storage_class == STORAGE_TYPEDEF) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'typedef'");
            } else if (parsed_storage_class != STORAGE_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple storage classes in declaration specifiers");
            } else {
                parsed_storage_class = STORAGE_TYPEDEF;
            }
        
        } else if (tok.kind == TOK_INLINE) {
            parsed_type_is_inline = 1;
        } else if (tok.kind == TOK_PACKED) {
            parsed_type_is_packed = 1;
        } else if (tok.kind == TOK_CHAR) {
        
            if (parsed_type_size == DATA_CHAR) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'char'");
            } else if (parsed_type_size != DATA_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
            } else {
                parsed_type_size = DATA_CHAR;
            }
        
        } else if (tok.kind == TOK_SHORT) {
        
            if (parsed_type_size == DATA_SHORT) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'short'");
            } else if (parsed_type_size != DATA_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
            } else {
                parsed_type_size = DATA_SHORT;
            }
        
        } else if (tok.kind == TOK_INT) {
        
            if (parsed_type_size == DATA_INT) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'int'");
            } else if (parsed_type_size != DATA_NONE) {
                /*report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");*/
            } else {
                parsed_type_size = DATA_INT;
            }
        
        } else if (tok.kind == TOK_LONG) {
        
            if (parsed_type_size == DATA_LONG) {
                parsed_type_size = DATA_LLONG;
            } else if (parsed_type_size == DATA_LLONG) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "too many 'long' keywords");
            } else if (parsed_type_size != DATA_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
            } else {
                parsed_type_size = DATA_LONG;
            }
        
        } else if (tok.kind == TOK_SIGNED) {
            saw_signed = 1;
        } else if (tok.kind == TOK_UNSIGNED) {
            saw_unsigned = 1;
        } else if (tok.kind == TOK_FLOAT) {
        
            if (parsed_type_size == DATA_FLOAT) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'float'");
            } else if (parsed_type_size != DATA_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
            } else {
            
                parsed_type_size = DATA_FLOAT;
                saw_float = 1;
            
            }
        
        } else if (tok.kind == TOK_DOUBLE) {
        
            if (parsed_type_size == DATA_LONG) {
            
                parsed_type_size = DATA_DOUBLE;
                saw_double = 1;
            
            } else if (parsed_type_size == DATA_DOUBLE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'double'");
            } else if (parsed_type_size != DATA_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
            } else {
            
                parsed_type_size = DATA_DOUBLE;
                saw_double = 1;
            
            }
        
        } else if (tok.kind == TOK_VOID) {
        
            if (parsed_type_size == DATA_VOID) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate 'void'");
            } else if (parsed_type_size != DATA_NONE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
            } else {
            
                parsed_type_size = DATA_VOID;
                parsed_type_is_void = 1;
                
                saw_void = 1;
            
            }
        
        } else if (token_is_ms_int_type_name ()) {
        
            if (strcmp (tok.ident, "__int8") == 0) {
            
                if (parsed_type_size != DATA_NONE) {
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
                } else {
                    parsed_type_size = DATA_CHAR & 0x1f;
                }
            
            } else if (strcmp (tok.ident, "__int16") == 0) {
            
                if (parsed_type_size != DATA_NONE) {
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
                } else {
                    parsed_type_size = DATA_SHORT & 0x1f;
                }
            
            } else if (strcmp (tok.ident, "__int32") == 0) {
            
                if (parsed_type_size != DATA_NONE) {
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
                } else {
                    parsed_type_size = DATA_INT & 0x1f;
                }
            
            } else {
            
                if (parsed_type_size != DATA_NONE) {
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "multiple data types in declaration specifiers");
                } else {
                    parsed_type_size = DATA_LLONG & 0x1f;
                }
            
            }
        
        }
        
        get_token ();
    
    }
    
    /*
     * Be tolerant of project typedef chains where the base typedef name is
     * not already known to this parser.  In
     *
     *     typedef some_base new_name;
     *     typedef some_base *new_ptr;
     *
     * the declarator name is new_name/new_ptr, not some_base.  Without
     * consuming the unknown base identifier here, parse_declarator() records
     * some_base as the typedef name and the real declarator is then parsed as
     * junk.
     */
    if (parsed_storage_class == STORAGE_TYPEDEF && !saw_real_type && tok.kind == TOK_IDENT && !is_current_typedef_name ()) {
    
        get_token ();
        
        saw = 1;
        saw_real_type = 1;
        
        parsed_type_size = DATA_INT;
    
    }
    
    if (parsed_type_size == DATA_NONE) {
    
        parsed_type_only_qualifiers = (saw && !saw_real_type) ? 1 : 0;
        parsed_type_size = DATA_INT;
    
    } else if (parsed_type_size == DATA_LONG && state->long64) {
        parsed_type_size = DATA_LLONG;
    }
    
    parsed_type_size &= 0x1f;
    
    if (is_current_typedef_name ()) {

        struct typedef_entry *entry;
        
        entry = find_typedef_name (tok.ident);
        get_token ();
        
        load_typedef_name (entry);
        
        if (entry && entry->is_aggregate && entry->tag_name && entry->tag_name[0]) {
        
            strncpy (parsed_type_tag_name, entry->tag_name, sizeof (parsed_type_tag_name) - 1);
            parsed_type_tag_name[sizeof (parsed_type_tag_name) - 1] = '\0';
            
            parsed_type_has_tag = 1;
        
        }
        
        parsed_type_only_qualifiers = 0;
        return;
    
    }
    
    if (tok.kind == TOK_STRUCT || tok.kind == TOK_UNION) {
    
        int is_union = (tok.kind == TOK_UNION);
        int has_tag = 0;
        
        int aggregate_size = 0;
        int aggregate_align = 1;
        
        int aggregate_fields[MAX_AGG_FIELDS];
        int aggregate_field_count = 0;
        
        int union_init_fields[MAX_AGG_FIELDS];
        int union_init_field_count = 0;
        int union_init_size = 0;
        
        struct aggregate_tag_entry *tag_entry = 0;
        
        int aggregate_storage_class = parsed_storage_class;
        int aggregate_is_inline = parsed_type_is_inline;
        int aggregate_is_packed = parsed_type_is_packed;
        
        char *tag_name = 0;
        int member_info_start = member_info_count;
        
        parsed_type_size = DATA_PTR;
        parsed_type_is_void = 0;
        
        clear_parsed_fields ();
        get_token ();
        
        if (tok.kind == TOK_PACKED) {
        
            aggregate_is_packed = 1;
            get_token ();
        
        }
        
        if (token_is_ident ()) {
        
            tag_name = xstrdup (tok.ident);
            strncpy (parsed_type_tag_name, tok.ident, sizeof (parsed_type_tag_name) - 1);
            
            parsed_type_tag_name[sizeof (parsed_type_tag_name) - 1] = '\0';
            has_tag = 1;
            
            get_token ();
        
        }
        
        if (tok.kind == TOK_PACKED) {
        
            aggregate_is_packed = 1;
            get_token ();
        
        }
        
        if (tok.kind == TOK_LBRACE) {
        
            get_token ();
            
            while (tok.kind != TOK_EOF && tok.kind != TOK_RBRACE) {
            
                int member_type_size;
                int member_type_is_floating;
                int member_is_aggregate;
                int member_has_tag;
                
                int member_fields[MAX_AGG_FIELDS];
                int member_field_count;
                
                int i;
                
                if (!is_type_start (tok.kind)) {
                
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "expected member declaration");
                    
                    skip_balanced_until (TOK_SEMI, TOK_RBRACE, TOK_EOF);
                    _accept (TOK_SEMI);
                    
                    continue;
                
                }
                
                parse_type_spec ();
                
                member_type_size = parsed_type_size;
                member_type_is_floating = parsed_type_is_floating;
                member_is_aggregate = parsed_type_is_aggregate;
                member_has_tag = parsed_type_has_tag;
                member_field_count = parsed_field_count;
                
                for (i = 0; i < member_field_count; i++) {
                    member_fields[i] = parsed_field_sizes[i];
                }
                
                if (tok.kind == TOK_SEMI) {

                    if (member_is_aggregate && !member_has_tag) {

                        int member_init_size = fields_storage_size (member_fields, member_field_count);
                        
                        if (is_union) {
                        
                            if (member_type_size > aggregate_size) {
                                aggregate_size = member_type_size;
                            }
                            
                            if (union_init_field_count == 0) {
                            
                                union_init_field_count = member_field_count;
                                
                                for (i = 0; i < member_field_count; i++) {
                                    union_init_fields[i] = member_fields[i];
                                }
                                
                                union_init_size = member_init_size;
                            
                            }
                        
                        } else {
                        
                            aggregate_size += member_type_size;
                            
                            for (i = 0; i < member_field_count && aggregate_field_count < MAX_AGG_FIELDS; i++) {
                                aggregate_fields[aggregate_field_count++] = member_fields[i];
                            }
                            
                            if (member_type_size > member_init_size && aggregate_field_count < MAX_AGG_FIELDS) {
                                aggregate_fields[aggregate_field_count++] = -(member_type_size - member_init_size);
                            }
                        
                        }
                    
                    }

                } else {
                
                    for (;;) {
                    
                        char *member = 0;
                        
                        int member_size, repeat;
                        int init_size;
                        
                        int member_info_elem_size;
                        int member_align;
                        
                        parse_declarator (&member);
                        
                        if (declarator_is_pointer) {
                        
                            member_size = DATA_PTR;
                            member_field_count = 1;
                            member_fields[0] = DATA_PTR;
                        
                        } else {
                        
                            member_size = member_type_size;
                            
                            if (member_field_count <= 0) {
                            
                                member_field_count = 1;
                                member_fields[0] = member_type_size;
                            
                            }
                        
                        }
                        
                        member_align = aggregate_is_packed ? 1 : (declarator_is_pointer ? type_alignment (DATA_PTR) : type_alignment (member_type_size));
                        
                        if (!aggregate_is_packed && member_align > aggregate_align) {
                            aggregate_align = member_align;
                        }
                        
                        repeat = 1;
                        
                        if (declarator_has_array) {
                        
                            repeat = (int) declarator_array_count;
                            member_size *= declarator_array_count;
                        
                        }
                        
                        if (_accept (TOK_COLON)) {
                            skip_balanced_until (TOK_COMMA, TOK_SEMI, TOK_RBRACE);
                        }
                        
                        if (declarator_has_array && declarator_is_pointer) {
                            member_info_elem_size = DATA_PTR;
                        } else {
                        
                            member_info_elem_size = declarator_is_pointer ?
                                (declarator_pointer_depth > 1 ? DATA_PTR : (parsed_type_is_aggregate ? parsed_type_size : (parsed_type_size & 0x1f))) :
                                    (declarator_has_array ? member_type_size : member_size);
                        
                        }
                        
                        init_size = fields_storage_size (member_fields, member_field_count);
                        
                        if (is_union) {
                        
                            remember_member_info_ex (member, 0, member_size, member_info_elem_size, declarator_is_pointer ? declarator_pointer_depth : 0, declarator_has_array, declarator_is_pointer ? 0 : member_type_is_floating, aggregate_is_packed);
                            
                            if (member_size > aggregate_size) {
                                aggregate_size = member_size;
                            }
                            
                            if (union_init_field_count == 0) {
                            
                                union_init_field_count = member_field_count;
                                
                                for (i = 0; i < member_field_count; i++) {
                                    union_init_fields[i] = member_fields[i];
                                }
                                
                                union_init_size = init_size;
                            
                            }
                        
                        } else {
                        
                            int member_offset = (int) align_up_long (aggregate_size, member_align);
                            int padding = member_offset - aggregate_size;
                            
                            int r;
                            
                            if (padding > 0 && aggregate_field_count < MAX_AGG_FIELDS) {
                                aggregate_fields[aggregate_field_count++] = -padding;
                            }
                            
                            aggregate_size = member_offset;
                            
                            remember_member_info_ex (member, member_offset, member_size, member_info_elem_size, declarator_is_pointer ? declarator_pointer_depth : 0, declarator_has_array, declarator_is_pointer ? 0 : member_type_is_floating, aggregate_is_packed);
                            aggregate_size += member_size;
                            
                            for (r = 0; r < repeat; r++) {
                            
                                for (i = 0; i < member_field_count && aggregate_field_count < MAX_AGG_FIELDS; i++) {
                                    aggregate_fields[aggregate_field_count++] = member_fields[i];
                                }
                            
                            }
                            
                            if (member_size > init_size * repeat && aggregate_field_count < MAX_AGG_FIELDS) {
                                aggregate_fields[aggregate_field_count++] = -(member_size - init_size * repeat);
                            }
                        
                        }
                        
                        if (member) {
                            free (member);
                        }
                        
                        if (!_accept (TOK_COMMA)) {
                            break;
                        }
                    
                    }
                
                }
                
                expect (TOK_SEMI, ";");
            
            }
            
            expect (TOK_RBRACE, "}");
            
            if (aggregate_size <= 0) {
                aggregate_size = DATA_CHAR & 0x1f;
            }
            
            if (!aggregate_is_packed) {
                aggregate_size = (int) align_up_long (aggregate_size, aggregate_align);
            }
            
            {
            
                const char *owner_name = 0;
                char anon_owner_name[64];
                
                int mi;
                
                if (has_tag) {
                    owner_name = tag_name;
                } else {
                
                    sprintf (anon_owner_name, "<anon-aggregate-%d>", anonymous_aggregate_owner_id++);
                    owner_name = anon_owner_name;
                
                }
                
                for (mi = member_info_start; mi < member_info_count; mi++) {
                
                    if (member_infos[mi].owner_size == 0) {
                    
                        /*
                         * Keep union members overlaid.
                         *
                         * The backend may re-layout aggregate member metadata for
                         * target alignment.  That is valid for structs, but not for
                         * unions: all direct union members must remain at offset 0.
                         * Store a negative owner size for union member records so
                         * size-only backend relayout code will not mistake the union
                         * for a struct with the same total size.  Parser lookups that
                         * need the size still treat +/-aggregate_size as matching.
                         */
                        member_infos[mi].owner_size = is_union ? -aggregate_size : aggregate_size;
                    
                    }
                    
                    if (owner_name && member_infos[mi].owner_tag_name == 0) {
                        member_infos[mi].owner_tag_name = xstrdup (owner_name);
                    }
                
                }
                
                if (!has_tag && owner_name) {
                
                    strncpy (parsed_type_tag_name, owner_name, sizeof (parsed_type_tag_name) - 1);
                    parsed_type_tag_name[sizeof (parsed_type_tag_name) - 1] = '\0';
                
                }
            
            }
            
            parsed_type_size = aggregate_size;
            parsed_type_is_aggregate = 1;
            parsed_type_is_void = 0;
            parsed_type_has_tag = has_tag;
            parsed_type_is_packed = aggregate_is_packed;
            
            clear_parsed_fields ();

            if (is_union) {
            
                int i;
                
                for (i = 0; i < union_init_field_count; i++) {
                    append_parsed_field (union_init_fields[i]);
                }
                
                if (aggregate_size > union_init_size) {
                    append_parsed_field (-(aggregate_size - union_init_size));
                }
            
            } else {
            
                int i;
                
                for (i = 0; i < aggregate_field_count; i++) {
                    append_parsed_field (aggregate_fields[i]);
                }
            
            }
            
            if (tag_name) {
            
                save_aggregate_tag (tag_name, is_union, parsed_type_size, parsed_field_sizes, parsed_field_count);
                update_typedef_name_from_aggregate_tag (tag_name, parsed_type_size, parsed_field_sizes, parsed_field_count);
            
            }
        
        } else {
        
            tag_entry = find_aggregate_tag (tag_name, is_union);

            if (tag_entry) {
                load_aggregate_tag_fields (tag_entry);
            } else {
            
                parsed_type_size = DATA_PTR;
                append_parsed_field (DATA_PTR);
            
            }
        
        }
        
        if (tag_name) {
            free (tag_name);
        }
        
        parsed_storage_class = aggregate_storage_class;
        parsed_type_is_inline = aggregate_is_inline;
        
        return;
    
    }
    
    if (tok.kind == TOK_ENUM) {
    
        parsed_type_size = DATA_INT & 0x1f;
        parsed_type_is_unsigned = 0;
        
        get_token ();
        
        if (token_is_ident ()) {
            get_token ();
        }
        
        if (tok.kind == TOK_LBRACE) {
            parse_enum_body ();
        }
        
        append_parsed_field (DATA_INT & 0x1f);
        return;
    
    }
    
    /*if (saw_void) {
        parsed_type_size = DATA_INT;
    }*/
    
    parsed_type_is_floating = (saw_float || saw_double) ? 1 : 0;
    
    if (saw_signed && saw_unsigned) {
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "both signed and unsigned specified");
    }

    if ((saw_signed || saw_unsigned) && (saw_float || saw_double || saw_void)) {
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "signed/unsigned invalid for this type");
    }

    parsed_type_is_unsigned = saw_unsigned ? 1 : 0;
    append_parsed_field (parsed_type_size);
    
    if (!saw) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "expected type");
        get_token ();
    
    }

}

void parse_direct_declarator (char **out_name) {

    if (_accept (TOK_LPAREN)) {
    
        parse_declarator_inner (out_name);
        expect (TOK_RPAREN, ")");
        
        if (declarator_is_pointer && !declarator_has_function) {
            declarator_function_is_pointer = 1;
        }
    
    } else if (token_is_ident ()) {
    
        if (out_name && !*out_name) {
            *out_name = take_ident ();
        } else {
            get_token ();
        }
    
    }
    
    for (;;) {
    
        if (_accept (TOK_LBRACK)) {
        
            int was_array = declarator_has_array;
            long count = 1;
            
            if (tok.kind != TOK_RBRACK) {
            
                enum token_kind kill[3];
                int v;
                
                kill[0] = TOK_RBRACK;
                kill[1] = TOK_EOF;
                kill[2] = 0;
                
                v = expr_const (kill);
                
                if (v > 0) {
                    count = v;
                }
            
            } else {
                declarator_array_unsized = 1;
            }
            
            if (!was_array) {
                declarator_first_array_count = count;
            }
            
            declarator_has_array = 1;
            declarator_array_dimensions++;
            
            if (declarator_array_count <= 0) {
                declarator_array_count = 1;
            }
            
            declarator_array_count *= count;
            declarator_last_array_count = count;
            
            expect (TOK_RBRACK, "]");
            continue;
        
        }
        
        if (_accept (TOK_LPAREN)) {
        
            int old_style_param_start = pending_param_count;
            declarator_has_function = 1;
            
            if (tok.kind != TOK_RPAREN) {
            
                for (;;) {
                
                    if (tok.kind == TOK_ELLIPSIS) {
                    
                        declarator_function_is_variadic = 1;
                        get_token ();
                    
                    } else if (is_type_start (tok.kind)) {
                    
                        enum token_kind saved_calling_convention = declarator_calling_convention;
                        enum token_kind saved_parsed_calling_convention = parsed_calling_convention;
                        
                        long saved_array_count = declarator_array_count;
                        
                        int saved_type_size = parsed_type_size;
                        int saved_storage_class = parsed_storage_class;
                        int saved_is_aggregate = parsed_type_is_aggregate;
                        int saved_is_void = parsed_type_is_void;
                        int saved_is_unsigned = parsed_type_is_unsigned;
                        int saved_is_floating = parsed_type_is_floating;
                        int saved_has_tag = parsed_type_has_tag;
                        int saved_is_pointer = declarator_is_pointer;
                        int saved_pointer_depth = declarator_pointer_depth;
                        int saved_has_array = declarator_has_array;
                        int saved_has_function = declarator_has_function;
                        int saved_function_is_pointer = declarator_function_is_pointer;
                        int saved_function_param_count = declarator_function_param_count;
                        int saved_function_has_prototype = declarator_function_has_prototype;
                        int saved_array_unsized = declarator_array_unsized;
                        int saved_is_inline = parsed_type_is_inline;
                        
                        char *param_name = 0;
                        int param_base_size, param_size;
                        
                        int count_this_param = 0;
                        int saw_void_param_list = 0;
                        
                        parse_type_spec ();
                        param_base_size = parsed_type_size;
                        
                        /*
                         * Parameter declarators must not inherit the outer
                         * declarator state.  In particular, in declarations
                         * such as:
                         *
                         *     FILE **__gtin(void);
                         *
                         * the function return type leaves declarator_is_pointer
                         * set before the parameter list is parsed.  If that flag
                         * is still set while looking at the abstract void
                         * parameter list, void is mistaken for a real pointer
                         * parameter and the prototype is recorded as taking one
                         * argument.
                         */
                        declarator_is_pointer = 0;
                        declarator_pointer_depth = 0;
                        declarator_has_array = 0;
                        declarator_has_function = 0;
                        declarator_function_is_pointer = 0;
                        declarator_array_unsized = 0;
                        declarator_array_count = 1;
                        declarator_last_array_count = 1;
                        
                        if (parsed_type_only_qualifiers && token_is_ident ()) {
                            get_token ();
                        }
                        
                        if (tok.kind != TOK_COMMA && tok.kind != TOK_RPAREN) {
                            parse_declarator (&param_name);
                        }
                        
                        if (!parsed_type_is_void || declarator_is_pointer || declarator_has_array || declarator_has_function || param_name) {
                        
                            count_this_param = 1;
                            param_size = (declarator_is_pointer || declarator_has_array || declarator_has_function || parsed_type_is_array_typedef) ? DATA_PTR : declarator_object_size (param_base_size);
                            
                            if (declarator_depth == 1) {
                            
                                add_pending_param (param_name, param_size, type_alignment (param_size),
                                    (declarator_is_pointer || parsed_type_is_array_typedef ? 0 : parsed_type_is_unsigned),
                                        (declarator_is_pointer || declarator_has_array || declarator_has_function || parsed_type_is_array_typedef) ? 0 : parsed_type_is_floating,
                                            declarator_pointer_depth, parsed_type_is_aggregate ? parsed_type_size : (parsed_type_size & 0x1f), parsed_type_is_unsigned);
                            
                            }
                        
                        } else {
                            saw_void_param_list = 1;
                        }
                        
                        if (param_name) {
                            free (param_name);
                        }
                        
                        parsed_calling_convention = saved_parsed_calling_convention;
                        parsed_type_size = saved_type_size;
                        parsed_storage_class = saved_storage_class;
                        parsed_type_is_aggregate = saved_is_aggregate;
                        parsed_type_is_void = saved_is_void;
                        parsed_type_is_unsigned = saved_is_unsigned;
                        parsed_type_is_floating = saved_is_floating;
                        parsed_type_has_tag = saved_has_tag;
                        parsed_type_is_inline = saved_is_inline;
                        
                        declarator_calling_convention = saved_calling_convention;
                        declarator_is_pointer = saved_is_pointer;
                        declarator_pointer_depth = saved_pointer_depth;
                        declarator_has_array = saved_has_array;
                        declarator_has_function = saved_has_function;
                        declarator_function_is_pointer = saved_function_is_pointer;
                        declarator_function_param_count = saved_function_param_count + (count_this_param ? 1 : 0);
                        declarator_function_has_prototype = saved_function_has_prototype || count_this_param || saw_void_param_list;
                        declarator_array_count = saved_array_count;
                        
                        declarator_array_unsized = saved_array_unsized;
                    
                    } else if (token_is_ident ()) {
                    
                        char *maybe_type_name = xstrdup (tok.ident);
                        int unknown_typedef_pointer = 0;
                        int consumed_as_prototype_param = 0;
                        
                        get_token ();
                        
                        while (tok.kind == TOK_CONST || tok.kind == TOK_VOLATILE || tok.kind == TOK_RESTRICT) {
                            get_token ();
                        }
                        
                        while (tok.kind == TOK_STAR) {
                        
                            unknown_typedef_pointer = 1;
                            get_token ();
                            
                            while (tok.kind == TOK_CONST || tok.kind == TOK_VOLATILE || tok.kind == TOK_RESTRICT) {
                                get_token ();
                            }
                        
                        }
                        
                        if (token_is_ident ()) {
                        
                            char *param_name = xstrdup (tok.ident);
                            int param_size = unknown_typedef_pointer ? DATA_PTR : DATA_PTR;
                            
                            get_token ();
                            
                            if (declarator_depth == 1) {
                            
                                if (find_pending_param_from (param_name, old_style_param_start) >= 0) {
                                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate parameter '%s'", param_name);
                                } else {
                                    add_pending_param (param_name, param_size, type_alignment (param_size), 0, 0, unknown_typedef_pointer ? 1 : 0, DATA_INT & 0x1f, 0);
                                }
                            
                            }
                            
                            free (param_name);
                            
                            declarator_function_param_count++;
                            declarator_function_has_prototype = 1;
                            consumed_as_prototype_param = 1;
                        
                        }
                        
                        if (!consumed_as_prototype_param) {
                        
                            if (declarator_depth == 1) {
                            
                                if (find_pending_param_from (maybe_type_name, old_style_param_start) >= 0) {
                                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, tok.start, tok.caret, "duplicate parameter '%s'", maybe_type_name);
                                } else {
                                    add_pending_param (maybe_type_name, DATA_INT & 0x1f, type_alignment (DATA_INT & 0x1f), 0, 0, 0, 0, 0);
                                }
                            
                            }
                            
                            declarator_function_param_count++;
                        
                        }
                        
                        if (maybe_type_name) {
                            free (maybe_type_name);
                        }
                    
                    } else {
                        skip_balanced_until (TOK_COMMA, TOK_RPAREN, TOK_EOF);
                    }
                    
                    if (!_accept (TOK_COMMA)) {
                        break;
                    }
                
                }
            
            }
            
            expect (TOK_RPAREN, ")");
            continue;
        }
        
        break;
    
    }

}
