#include "cic.h"
#include "output.h"

static int string_num;
static struct table global_tags;
static struct table global_ids;
static struct table constants;
static Table tags;
static Table identifiers;
static Symbol *function_tail, *global_tail, *string_tail, *float_tail;

Symbol functions;
Symbol globals;
Symbol strings;
Symbol float_constants;

int level;
int temp_num;
int label_num;

static Symbol LookupSymbol(Table tbl, char *name) {
    Symbol p;
    unsigned h = (unsigned long)name & SYM_HASH_MASK;

    do {
        if (tbl->buckets) {
            for (p = tbl->buckets[h]; p; p = p->link)
                if (p->name == name)
                    return p;
        }
    } while ((tbl = tbl->outer) != NULL);

    return NULL;
}

static Symbol AddSymbol(Table tbl, Symbol sym) {
    unsigned int h = (unsigned long)sym->name & SYM_HASH_MASK;

    if (tbl->buckets) {
        int sz = sizeof(Symbol) * (SYM_HASH_MASK + 1);

        tbl->buckets = (Symbol*)HeapAllocate(current_heap, sz);
        memset(tbl->buckets, 0, sz);
    }

    sym->link = tbl->buckets[h];
    sym->level = tbl->level;
    tbl->buckets[h] = sym;

    return sym;
}

void EnterScope() {
    Table t;

    level++;

    ALLOC(t);
    t->level = level;
    t->outer = identifiers;
    t->buckets = NULL;
    identifiers = t;

    ALLOC(t);
    t->level = level;
    t->outer = tags;
    t->buckets = NULL;
    tags = t;
}

void ExitScope() {
    level--;
    identifiers = identifiers->outer;
    tags = tags->outer;
}

Symbol LookupID(char *name) {
    return LookupSymbol(identifiers, name);
}

Symbol LookupTag(char *name) {
    return LookupSymbol(tags, name);
}

Symbol AddTag(char *name, Type ty) {
    Symbol p;

    CALLOC(p);

    p->kind = SK_Tag;
    p->name = name;
    p->ty = ty;

    return AddSymbol(tags, p);
}

Symbol CreateLabel() {
    Symbol p;

    CALLOC(p);

    p->kind = SK_Label;
    p->name = FormatName("BB%d", label_num++);

    return p;
}

Symbol CreateOffset(Type ty, Symbol base, int coff) {
    VariableSymbol p;
    
    if (coff == 0)
        return base;

    CALLOC(p);

    if (base->kind == SK_Offset) {
        coff += AsVar(base)->offset;
        base = base->link;
    }
    p->addressed = 1;
    p->kind = SK_Offset;
    p->ty = ty;
    p->link = base;
    p->offset = coff;
    p->name = FormatName("%s[%d]", base->name, coff);
    base->ref++;

    return (Symbol)p;
}

void InitSymbolTable() {
    int sz;

    level = 0;

    global_tags.buckets = global_ids.buckets = NULL;
    global_tags.outer = global_ids.outer = NULL;
    global_tags.level = global_ids.level = 0;

    sz = sizeof(Symbol) * (SYM_HASH_MASK + 1);
    constants.buckets = HeapAllocate(current_heap, sz);
    memset(constants.buckets, 0, sz);
    constants.outer = NULL;
    constants.level = 0;

    functions = globals = strings = float_constants = NULL;
    function_tail = &functions;
    global_tail = &globals;
    string_tail = &strings;
    float_tail = &float_constants;

    tags = &global_tags;
    identifiers = &global_ids;

    temp_num = label_num = string_num = 0;
}
