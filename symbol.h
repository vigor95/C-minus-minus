#ifndef _SYMBOL_H_
#define _SYMBOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "util.h"
#include "error.h"

#define MAX_SLOTS 10000
#define EMPTY_SLOT -1

typedef struct record {
    int addr;
    char type;
    char name[64];
    char *value;
    int slot;
    char scope;
    int seen;
    char isarray;
    int arraysize;
    char *block_info;
} record;

typedef struct symbol_table {
    char t_name[32];
    double load_factor;
    size_t in_use;
    size_t size;
    record *table;
} symbol_table;

void print_stab(symbol_table*);

symbol_table* create_stab(char*, size_t);

void stab_insert(char*, record*, symbol_table*);

record* get_record(char*, char*, char, int, char*);

void purge_record(record*);

size_t hash(char*, size_t);

int is_keyword(char*);

int return_value(symbol_table*, int);

#endif
