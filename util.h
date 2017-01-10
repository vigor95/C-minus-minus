#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#define INTERIM_FILENAME "data"
#define DEFAULT_OUTPUT "a.out"

#define TRUE 1
#define FALSE 0

#define DEBUG TRUE

typedef struct file_struct {
    char calling_prog[64];
    char filename[64];
    char default_out[64];
    int cur_line;
    int cur_column;
    char cur_word[64];
} file_struct;

extern file_struct file;
extern size_t total_newlines;

inline char* scan_for_out(int argc, char *argv[]);
inline char** get_files_from_argv(int argc, char *argv[]);

int get_filesize(char*);

void* xmalloc(int);

char fcpeek(FILE*);

char* getword(FILE*);

char* wordpeek(FILE*);

void file_error(char*, char*, char*, char*, char*);

int get_column(FILE*);

char* copy_alloced(char*);

int intlit_comp(const void*, const void*);

int floatlit_comp(const void*, const void*);

char* cstr(char*);

int token_num(char*);

char* strip_whitesp(char*);

#endif
