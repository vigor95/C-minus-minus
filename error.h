#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdio.h>
#include <stdlib.h>

enum {
    TYPE_ERR,
    TOKEN_ERR,
    SYNTAX_ERR,
    TYPE_WARN,
    TOKEN_WARN,
    SYNTAX_WARN
};

size_t lines;
size_t columns;

inline void error(char*, int, int, char*);
inline void warning(char*, int, int, char*);

inline int get_line(FILE*);
inline int get_column(FILE*);

#endif
