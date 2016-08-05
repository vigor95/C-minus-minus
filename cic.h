#ifndef __CIC_H_
#define __CIC_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "input.h"
#include "error.h"
#include "alloc.h"
#include "vector.h"
#include "str.h"
#include "lex.h"
#include "type.h"

#define ALIGN(size, align) ((size + align - 1) & (~(align - 1)))

extern Vector extra_white_space;
extern Vector extra_keywords;
extern FILE *ast_file;
extern FILE *ir_file;
extern char *ext_name;
extern Heap current_heap;
extern struct heap program_heap;
extern struct heap file_heap;
extern struct heap string_heap;
extern int warning_count;
extern int error_count;

#endif
