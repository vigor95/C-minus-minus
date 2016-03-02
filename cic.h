#include <set>
#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>

char* newChar(char *src) {
    char *ret = NULL;
    if (src) {
        ret = new char[strlen(src) + 1];
        strcpy(ret, src);
    }
    return ret;
}

char* newChar(const char *src) {
    char *ret = NULL;
    if (src) {
        ret = new char[strlen(src) + 1];
        strcpy(ret, src);
    }
    return ret;
}

struct cmp {
    bool operator()(const char *s1, const char *s2) {
        return strcmp(s1, s2) < 0;
    }
};

enum {
    TIDENT,
    TKEYWORD,
    TNUMBER,
    TCHAR,
    TSTRING,
    TEOF,
    TINVALID,
                                // Only in CPP
    MIN_CPP_TOKEN,
    TNEWLINE,
    TSPACE,
    TMACRO_PARAM
};

enum {
    ENC_NONE,
    ENC_CHAR16,
    ENC_CHAR32,
    ENC_UTF8,
    ENC_WCHAR
}; 

struct Type;
struct Node;
template <class T>
struct Map {
    Map<T> *parent;
    std::map<char*, T*, cmp> *body;
    Map(): parent(NULL), body(NULL) {}
    Map(Map<T> *p): parent(p), body(new std::map<char*, T*, cmp>) {}
    Node* get(char *key) {
        auto it = body->find(key);
        if (it != body->end()) return it->second;
        if (parent) return parent->get(key);
        return NULL;
    }
};

template <class T>
struct Dict {
    Map<T> *m;
    std::vector<char*> *key;
};

struct Buffer {
    char *body;
    int nalloc;
    int len;
};

struct File {
    FILE *file;
    char *p;
    char *name;
    int line, column;
    int ntok;
    int last;
    int buf[3];
    int buflen;
};

struct Token {
    int kind;
    File *file;
    int line, column;
    bool space, bol;
    int cnt;
    std::set<std::string> hideset;
    union {
        int id;
        struct {
            char *sval;
            int slen;
            int c;
            int enc;
        };
        struct {
            bool is_vararg;
            int pos;
        };
    };
    Token() {}
    Token(int k): kind(k) {}
    Token(int k, const char *p): kind(k) {
        printf("Token(int k, const char *p)\n");
        sval = new char[strlen(p) + 1];
        strcpy(sval, p);
    }
    Token(int k, const char *s, int len, int _enc):
        kind(k), slen(len), enc(_enc) {
            printf("Token(int k, const char *s, int len, int _enc)\n");
            sval = new char[strlen(s) + 1];
            strcpy(sval, s);
        }
    Token(int k, int i): kind(k), id(i) {}
    Token(int k, char _c): kind(k), c(_c) {}
    Token(int k, int _c, int _enc): kind(k), c(_c), enc(_enc) {}
    Token& operator=(const Token &other) {
        kind = other.kind;
        return *this;
    }
};

struct Node;

enum {
    AST_LITERAL = 256,
    AST_LVAR,
    AST_GVAR,
    AST_TYPEDEF,
    AST_FUNCALL,
    AST_FUNCPTR_CALL,
    AST_FUNCDESG,
    AST_FUNC,
    AST_DECL,
    AST_INIT,
    AST_CONV,
    AST_ADDR,
    AST_DEREF,
    AST_IF,
    AST_TERNARY,
    AST_DEFAULT,
    AST_RETURN,
    AST_COMPOUND_STMT,
    AST_STRUCT_REF,
    AST_GOTO,
    AST_COMPUTED_GOTO,
    AST_LABEL,
    OP_SIZEOF,
    OP_CAST,
    OP_SHR,
    OP_SHL,
    OP_A_SHR,
    OP_A_SHL,
    OP_PRE_INC,
    OP_PRE_DEC,
    OP_POST_INC,
    OP_POST_DEC,
    OP_LABEL_ADDR,
#define op(name, _) name,
#define keyword(name, x, y) name,
#include "keyword.inc"
#undef keyword
#undef op
};

enum {
    KIND_VOID,
    KIND_BOOL,
    KIND_CHAR,
    KIND_SHORT,
    KIND_INT,
    KIND_LONG,
    KIND_LLONG,
    KIND_FLOAT,
    KIND_DOUBLE,
    KIND_LDOUBLE,
    KIND_ARRAY,
    KIND_ENUM,
    KIND_PTR,
    KIND_STRUCT,
    KIND_FUNC,
    KIND_STUB
};

struct Type {
    int kind;
    int size;
    int align;
    bool usig;
    bool isstatic;
    Type *ptr;
    int len;
    Dict<std::pair<char*, Type*> > *fields;
    int offset;
    bool isstruct;
    int bitoff;
    int bitsize;
    Type *rettype;
    std::vector<Type*> *params;
    bool hasva;
    bool oldstyle;
    Type() {}
    Type(int k): kind(k) {}
    Type(int k, int s, int a, bool u):
        kind(k), size(s), align(a), usig(u) {}
    Type(int k, Type *t, int s, int a): kind(k), size(s), 
        align(a), ptr(new Type(*t)) {}
    Type(int k, Type *t, int s, int l, int a): kind(k),
        size(s), align(a), ptr(new Type(*t)), len(l) {}
};

struct SourceLoc {
    char *file;
    int line;
};

struct Node {
    int kind;
    Type *tp;
    SourceLoc *sl;
    union {
        long ival;
        struct {
            double fval;
            char *flabel;
        };
        struct {
            char *sval;
            char *slabel;
        };
        struct {
            char *varname;
            int loff;
            char *glabel;
        };
        struct {
            Node *left;
            Node *right;
        };
        struct {
            Node *operand;
        };
        struct {
            char *fname;
            Type *ftype;
            Node *fptr;
            Node *body;
        };
        struct {
            Node *declvar;
            std::vector<Node*> *declinit;
        };
        struct {
            Node *initval;
            int initoff;
            Type *totype;
        };
        struct {
            Node *cond, *then, *els;
        };
        struct {
            char *label, *newlabel;
        };
        Node *retval;
        std::vector<Node*> *stmts;
        struct {
            Node *struc;
            char *field;
            Type *fieldtype;
        };
    };
    /*Node() = default;
    Node(int k, Type *t, Node *o): kind(k), tp(t ? new Type(*t) : NULL),
        operand(o ? new Node(*o) : NULL) {}
    Node(int k, Type *t): kind(k), tp(t ? new Type(*t) : NULL) {}
    Node(int k, Type *t, long iv): kind(k), tp(t ? new Type(*t) : NULL),
        ival(iv) {}
    Node(int k, Type *t, double fv): kind(k), tp(t ? new Type(*t) : NULL),
        fval(fv) {}
    Node(int k, Type *t, char *vn, char *gn): kind(k),
        tp(t ? new Type(*t) : NULL), varname(newChar(vn)), glabel(newChar(          gn)) {}
    Node(int k, Type *t, char *sv, int flag): kind(k), 
        tp(t ? new Type(*t) : NULL) {
        if (flag == 1) {
            varname = newChar(sv);
        }
        else if (flag == 2) {
            sval = newChar(sv);
        }
        else if (flag == 3) {
            fname = newChar(sv);
        }
    }
    Node(int k, Type *t, char *fn, Type *ft): kind(k),
        tp(t ? new Type(*t) : NULL), fname(newChar(fn)),
        ftype(ft ? new Type(*ft) : NULL) {}
    Node(int k, Type *t, Node *fp, std::vector<void*> as): kind(k), tp(t ? new Type(*t) : NULL),
        fptr(fp ? new Node(*fp) : NULL) {}
    Node(int k, Type *t, char *fn, std::vector<void*> ps, std::vector<void*> ls, Node *b): kind(k), tp(t ? new Type(*t) : NULL), fname(newChar(fn)),
        localvars(ls), body(b ? new Node(*b) : NULL) {}
    Node(int k, Node *v, std::vector<void*> di) {}
    */
};

extern Type *type_void;
extern Type *type_bool;
extern Type *type_char;
extern Type *type_short;
extern Type *type_int;
extern Type *type_long;
extern Type *type_uchar;
extern Type *type_ushort;
extern Type *type_uint;
extern Type *type_ulong;
extern Type *type_ullong;
extern Type *type_float;
extern Type *type_double;
extern Type *type_ldouble;

// buffer.cpp
Buffer* makeBuffer();
char* bufBody(Buffer *b);
int bufLen(Buffer *b);
void bufWrite(Buffer *b, char c);
void bufAppend(Buffer *b, char *s, int len);
void bufPrintf(Buffer *b, const char *fmt, ...);
char *vformat(const char *fmt, va_list ap);
char *format(const char *fmt, ...);
char *quoteCstring(char *p);
char *quoteCstringLen(char *p, int len);
char *quoteChar(char c);

// debug.cpp
char* ty2s(Type *tp);
char* node2s(Node *node);
char* tk2s(Token *tk);

// dict.cpp
template <class T>
Dict<T>* makeDict();
template <class T>
Type* dictGet(Dict<T>*, char*);
template <class T>
void dictPut(Dict<T>*, char*, Type*);
template <class T>
std::vector<char*, Type*>* dictKeys(Dict<T>*);

//file.cpp
File *makeFile(FILE *file, char *name);
File *makeFileString(char *s);
int readc(void);
void unreadc(int c);
File *currentFile(void);
void streamPush(File *file);
int streamDepth(void);
char *inputPosition(void);
void streamStash(File *f);
void streamUnstash(void);

//lex.cpp
void lexInit(char *filename);
bool isKeyword(Token *tk, int c);
void tokenBufferStash(std::vector<Token*> *buf);
void tokenBufferUnstash();
void ungetToken(Token *tk);
Token* lexString(char *s);
Token* lex();

//error.cpp
extern bool enable_warning;
extern bool dumpstack;
extern bool dumpsource;
extern bool w_e;

#define STR2(x) #x
#define STR(x) STR2(x)
#define error(...)       errorf(__FILE__ ":" STR(__LINE__), NULL, __VA_ARGS__)
#define errort(tok, ...) errorf(__FILE__ ":" STR(__LINE__), tokenPos(tok), __VA_ARGS__)
#define warn(...)        warnf(__FILE__ ":" STR(__LINE__), NULL, __VA_ARGS__)
#define warnt(tok, ...)  warnf(__FILE__ ":" STR(__LINE__), tokenPos(tok), __VA_ARGS__)

[[noreturn]] void errorf(const char *line, const char *pos, const char *fmt, ...);
void warnf(const char *line, const char *pos, const char *fmt, ...);
const char* tokenPos(Token *tk);

// map.cpp
template <class T>
Map<T>* makeMapParent(Map<T>*);
