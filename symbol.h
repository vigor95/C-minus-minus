#ifndef __SYMBOL_H_
#define __SYMBOL_H_

enum {
    SK_Tag, SK_TypedefName, SK_EnumConstant, SK_Constant, SK_Variable,
    SK_Temp, SK_Offset, SK_String, SK_Label, SK_Function, SK_Register
};

#define SYM_HASH_MASK 127

#define SYMBOL_COMMON   \
    int kind;           \
    char *name;         \
    char *aname;        \
    Type ty;            \
    int level;          \
    int sclass;         \
    int ref;            \
    int defined:    1;  \
    int addressed:  1;  \
    int needwb:     1;  \
    int unused:     29; \
    union value val;    \
    struct symbol *reg; \
    struct symbol *link;\
    struct symbol *next;

typedef struct bblock *BBlock;
typedef struct initData *InitData;

typedef struct symbol {
    SYMBOL_COMMON;
} *Symbol;

typedef struct valueDef {
    Symbol dst;
    int op;
    Symbol src1;
    Symbol src2;
    BBlock ownBB;
    struct valueDef *link;
} *ValueDef;

typedef struct valueUse {
    ValueDef def;
    struct valueUse *next;
} *ValueUse;

typedef struct variableSymbol {
    SYMBOL_COMMON
    InitData idata;
    ValueDef def;
    ValueUse uses;
    int offset;
} *VariableSymbol;

typedef struct functionSymbol {
    SYMBOL_COMMON
    Symbol params;
    Symbol locals;
    Symbol *lastv;
    int nbblock;
    BBlock entryBB;
    BBlock exitBB;
    ValueDef val_numtable[16];
} *FunctionSymbol;

typedef struct table {
    Symbol *buckets;
    int level;
    struct table *outer;
} *Table;

#define AsVar(sym) ((VariableSymbol)sym)
#define AsFunc(sym) ((FunctionSymbol)sym)

void InitSymbolTable();
void EnterScope();
void ExitScope();

Symbol LookupID(char*);
Symbol LookupTag(char*);
Symbol AddConstant(Type, union value);
Symbol AddEnumConstant(char*, Type, int);
Symbol AddTypedefName(char*, Type);
Symbol AddVariable(char*, Type, int);
Symbol AddFunction(char*, Type, int);
Symbol AddTag(char*, Type);
Symbol IntConstant(int);
Symbol CreateTemp(Type);
Symbol CreateLabel();
Symbol AddString(Type, String);
Symbol CreateOffset(Type, Symbol, int);

extern int level;
extern int label_num, temp_num;
extern Symbol functions;
extern Symbol globals;
extern Symbol strings;
extern Symbol float_constants;
extern FunctionSymbol fsym;

#endif
