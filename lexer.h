#ifndef _LEXER_H_
#define _LEXER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "util.h"
#include "symbol.h"

#define INTLIT_TABLE_MAX    1000
#define FLOATLIT_TABLE_MAX  1000
#define STRINGLIT_TABLE_MAX 1000

#define TK_LEFTBRACKET         0
#define TK_RIGHTBRACKET        1

#define TK_LEFTPAREN           2
#define TK_RIGHTPAREN          3
#define TK_LEFT_SQR_BRACKET    4
#define TK_RIGHT_SQR_BRACKET   5
#define TK_DOT                 6

#define TK_UNARY_EXCLAMATION   7
#define TK_UNARY_TILDA         8
#define TK_UNARY_PLUSPLUS      9 
#define TK_UNARY_MINUSMINUS    10
#define TK_UNARY_PLUS          11
#define TK_UNARY_MINUS         12
#define TK_UNARY_STAR          13
#define TK_UNARY_AMPERSAND     14
#define TK_UNARY_CASTING       15
#define TK_UNARY_SIZEOF        16

#define TK_MULT_STAR           17
#define TK_DIV                 18
#define TK_MOD                 19

#define TK_PLUS                20
#define TK_MINUS               21

#define TK_LEFT_SHIFT          22
#define TK_RIGHT_SHIFT         23

#define TK_LESS_LOGIC          24
#define TK_GREATER_LOGIC       25
#define TK_GREATER_EQU_LOGIC   26
#define TK_LESS_EQU_LOGIC      27

#define TK_EQU_EQU_LOGIC       28
#define TK_NOT_EQU_LOGIC       29

#define TK_BIT_AMPERSAND       30
#define TK_BIT_XOR             31
#define TK_BIT_OR              32

#define TK_LOGIC_AND           33
#define TK_LOGIC_OR            34

#define TK_QUESTION            35
#define TK_COLON               36

#define TK_EQU                 37
#define TK_PLUS_EQU            38
#define TK_MINUS_EQU           39
#define TK_STAR_EQU            40
#define TK_DIV_EQU             41
#define TK_MOD_EQU             42
#define TK_AND_EQU             43
#define TK_XOR_EQU             44
#define TK_OR_EQU              45
#define TK_LSHIFT_EQU          46
#define TK_RSHIFT_EQU          47

#define TK_COMMA               48

#define TK_KEYWORD             49
#define TK_IDENTIFIER          50
#define TK_CONSTANT            51
#define TK_STRING              52

#define TK_AUTO                53
#define TK_BREAK               54
#define TK_CASE                55
#define TK_CHAR                56
#define TK_CONST               57
#define TK_CONTINUE            58
#define TK_DEFAULT             59
#define TK_DO                  60
#define TK_DOUBLE              61
#define TK_ELSE                62
#define TK_ENUM                63
#define TK_EXTERN              64
#define TK_FLOAT               65
#define TK_FOR                 66
#define TK_GOTO                67
#define TK_IF                  68
#define TK_INT                 69
#define TK_LONG                70
#define TK_REGISTER            71
#define TK_RETURN              72
#define TK_SHORT               73
#define TK_SIGNED              74
#define TK_SIZEOF              75
#define TK_STATIC              76
#define TK_STRUCT              77
#define TK_SWITCH              78
#define TK_TYPEDEF             79
#define TK_UNION               80
#define TK_UNSIGNED            81
#define TK_VOID                82
#define TK_VOLATILE            83
#define TK_WHILE               84

#define TK_SEMICOLON           85
#define TK_INTLIT              86   /**> need array for this */
#define TK_STRINGLIT           87   /**> need a symbol table for this */
#define TK_FLOATLIT            88

#define TK_LABEL               89
#define TK_FUNCTION_CALL       90

#define TK_PRINTF              91

typedef struct token_package {
    int val;
    double fval;
    int type;
} token_package;

extern size_t total_newlines;

static char tk_buffer0[5];
static char tk_buffer1[5];

static size_t intlit_t_counter;
static size_t floatlit_t_counter;
static int intlit_table[INTLIT_TABLE_MAX];
static double floatlit_table[FLOATLIT_TABLE_MAX];

extern symbol_table* string_table;
extern symbol_table* id_table;

size_t total_char;
size_t total_char_per_line;
size_t total_newlines;

void run_lexer(file_struct*);

void put_lexeme(FILE*, char*, char*);

void put_ulexeme(FILE*, char*);

int is_valid_id(char*);

int parse_tokens(FILE*, char*);

char* extract_token(char*);

char* return_char(char*);

char* return_integral(char*);

char* return_string(char*);

char* return_keyword(char*);

int extract_char(char*);

token_package get_sval(char*);

inline static void scan_a(FILE*, char*);
inline static void scan_b(FILE*, char*);
inline static void scan_c(FILE*, char*);
inline static void scan_d(FILE*, char*);
inline static void scan_e(FILE*, char*);
inline static void scan_f(FILE*, char*);
inline static void scan_g(FILE*, char*);
inline static void scan_i(FILE*, char*);
inline static void scan_l(FILE*, char*);
inline static void scan_r(FILE*, char*);
inline static void scan_s(FILE*, char*);
inline static void scan_t(FILE*, char*);
inline static void scan_p(FILE*, char*);
inline static void scan_u(FILE*, char*);
inline static void scan_v(FILE*, char*);
inline static void scan_w(FILE*, char*);
inline static void scan_leftbracket(FILE*, char*);
inline static void scan_rightbracket(FILE*, char*);
inline static void scan_leftparen(FILE*, char*);
inline static void scan_rightparen(FILE*, char*);
inline static void scan_left_sqr_bracket(FILE*, char*);
inline static void scan_right_sqr_bracket(FILE*, char*);
inline static void scan_dot(FILE*, char*);
inline static void scan_comma(FILE*, char*);
inline static void scan_exclamation(FILE*, char*);
inline static void scan_tilda(FILE*, char*);
inline static void scan_plus(FILE*, char*);
inline static void scan_minus(FILE*, char*);
inline static void scan_unary_star(FILE*, char*);
inline static void scan_ampersand(FILE*, char*);
inline static void scan_div(FILE*, char*);
inline static void scan_mod(FILE*, char*);
inline static void scan_less(FILE*, char*);
inline static void scan_greater(FILE*, char*);
inline static void scan_equal(FILE*, char*);
inline static void scan_or(FILE*, char*);
inline static void scan_xor(FILE*, char*);
inline static void scan_question(FILE*, char*);
inline static void scan_colon(FILE*, char*);
inline static void scan_semicolon(FILE*, char*);
inline static void scan_constant(FILE*, char*);
inline static void scan_stringlit(FILE*, char*);
inline static void scan_default(FILE*, char*);

#endif
