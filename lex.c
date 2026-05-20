/******************************************************************************
 * @file            lex.c
 *****************************************************************************/
#include    "lex.h"

char is_end_of_line[256] = { 0 };
char lex_table[256] = { 0 };

void lex_init (void) {

    int i;
    
    is_end_of_line[0] = 1;
    is_end_of_line[10] = 1;
    
    /*lex_table[36] = LEX_NAME_START | LEX_NAME_PART;*/
    /*lex_table[46] = LEX_NAME_START | LEX_NAME_PART;*/
    
    for (i = 48; i < 58; i++) {
        lex_table[i] = LEX_NAME_PART;
    }
    
    /*lex_table[63] = LEX_NAME_START | LEX_NAME_PART;*/
    
    for (i = 65; i < 91; i++) {
        lex_table[i] = LEX_NAME_START | LEX_NAME_PART;
    }
    
    lex_table[95] = LEX_NAME_START | LEX_NAME_PART;
    
    for (i = 97; i < 123; i++) {
        lex_table[i] = LEX_NAME_START | LEX_NAME_PART;
    }

}
