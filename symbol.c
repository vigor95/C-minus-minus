#include "symbol.h"

void print_stab(symbol_table *stab) {
    size_t i;
    printf("\n\nCurrent symbol table: '%s' \n - Status -\n"
            "Total used: %4d\t Size: %4d\t Load Factor: %4.2f\n"
            "\n", stab->t_name, (int)stab->in_use, (int)stab->size, stab->load_factor);
    printf("-------\t----\t-----\t----\t-----\n"
            "Address\tName\tValue\tSlot\tType\tScope\tIsArray\tArray-Size\n");
    for (i = 0; i < stab->size; i++) {
        if (stab->table[i].slot != EMPTY_SLOT) {
            printf("%4d\t%s\t%s\t%d\t%c\t%d\t%c\t%d\n",
                    stab->table[i].addr, stab->table[i].name,
                    stab->table[i].value, stab->table[i].slot,
                    stab->table[i].type, stab->table[i].scope,
                    stab->table[i].isarray, stab->table[i].arraysize);
        }
    }
}

size_t hash(char *key, size_t limit) {
    int hash_value = 0;
    size_t i;
    size_t size = strlen(key);
    for (i = 0; i < size; i++) {
        hash_value = 31 * hash_value + key[i];
    }
    return (size_t)(hash_value % limit);
}

symbol_table* create_stab(char *t_name, size_t max) {
    size_t i;
    symbol_table *stab = (symbol_table*)xmalloc(sizeof(symbol_table));
    stab->table = (record*)xmalloc(sizeof(record) * max);
    stab->in_use = 0;
    stab->load_factor = 0;
    stab->size = max;
    strcpy(stab->t_name, t_name);
    for (i = 0; i < max; i++) {
        stab->table[i].addr = -1;
        stab->table[i].slot = EMPTY_SLOT;
        stab->table[i].seen = 0;
    }
    return stab;
}

void stab_insert(char *filename, record *rec, symbol_table *stab) {
    size_t index = hash(rec->name, stab->size);
    rec->slot = index;
    stab->table[index] = *rec;
    stab->in_use++;
    stab->load_factor = ((float)stab->in_use) / stab->size;
}

record* get_record(char *name, char *val, char type, int slot, char *scope) {
    record *rec = (record*)xmalloc(sizeof(record));
    rec->value = (char*)xmalloc(sizeof(char) * strlen(val));
    strcpy(rec->name, name);
    strcpy(rec->value, val);
    rec->type = type;
    rec->slot = slot;
    rec->scope = 0;
    return rec;
}

int return_value(symbol_table *stab, int target) {
    return 1;
}

void purge_record(record *rec) {

}

void destroy_stab(symbol_table *stab) {

}

static const char* keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
    "else", "enum", "extern", "float", "for", "goto", "int", "if", "long", "register",
    "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef",
    "union", "unsigned", "void", "volatile", "while"
};

int is_keyword(char *lexeme) {
    for (size_t i = 0; i < 32; i++) {
        if (strcmp(keywords[i], lexeme) == 0)
            return TRUE;
    }
    return FALSE;
}
