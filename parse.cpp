#include "cic.h"

const int MAX_ALIGN = 16;
SourceLoc *source_loc;

static std::map<std::string, Node*> *globalenv = new std::map<std::string, Node*>;
static std::map<std::string, Node*> *localenv = new std::map<std::string, Node*>;
static std::map<std::string, Node*> *tags = new std::map<std::string, Node*>;

//static Map *globalenv = new Map;
//static Map *localenv;
//static Map *tags = new Map;

static char *defalutcase;
static char *lbreak;
static char *lcontinue;

Type *type_void = new Type(KIND_VOID, 0, 0, false);
Type *type_bool = new Type(KIND_BOOL, 1, 1, true);
Type *type_char = new Type(KIND_CHAR, 1, 1, false);
Type *type_short = new Type(KIND_SHORT, 2, 2, false);
Type *type_int = new Type(KIND_INT, 4, 4, false);
Type *type_long = new Type(KIND_LONG, 8, 8, false);
Type *type_llong = new Type(KIND_LLONG, 8, 8, false);
Type *type_uchar = new Type(KIND_CHAR, 1, 1, true);
Type *type_ushort = new Type(KIND_SHORT, 2, 2, true);
Type *type_uint = new Type(KIND_INT, 4, 4, true);
Type *type_ulong = new Type(KIND_LONG, 8, 8, true);
Type *type_ullong = new Type(KIND_LLONG, 8, 8, true);
Type *type_float = new Type(KIND_FLOAT, 4, 4, false);
Type *type_double = new Type(KIND_DOUBLE, 8, 8, false);
Type *type_ldouble = new Type(KIND_LDOUBLE, 8, 8, false);
Type *type_enum = new Type(KIND_ENUM, 4, 4, false);

struct Case {
    int beg;
    int end;
    char *label;
};

enum {
    S_TYPEDEF = 1,
    S_EXTERN,
    S_STATIC,
    S_AUTO,
    S_REGISTER
};

enum {
    DECL_BODY = 1,
    DECL_PARAM,
    DECL_PARAM_TYPEONLY,
    DECL_CAST
};

static void markLocation() {
    Token *tk = peek();
    source_loc = new SourceLoc;
    source_loc->file = tk->file->name;
    source_loc->line = tk->line;
}

char* makeTempname() {
    static int num = 0;
    return format("@T%d", num++);
}

char* makeLable() {
    static int num = 0;
    return format("@L%d", num++);
}

static char* makeStaticLabel(char *name) {
    static int num = 0;
    return format("@S%d@%s", num++, name);
}

static Case* makeCase(int beg, int end, char *label) {
    Case *r = new Case;
    r->beg = beg;
    r->end = end;
    r->label = label;
    return r;
}

static Map* env() {
    return localenv ? localenv : globalenv;
}

static Node* makeAst(Node *tmp) {
    Node *r = new Node;
    *r = *tmp;
    r->sl = source_loc;
    return r;
}

static Node* astUop(int kind, Type *tp, Node *operand) {
    return makeAst(new Node(kind, tp, operand));
}

static Node* astBinop(Type *tp, int kind, Node *left, Node *right) {
    Node *r = makeAst(new Node(kind, tp));
    r->left = left;
    r->right = right;
    return r;
}

static Node* astInitType(Type* tp, long val) {
    return makeAst(new Node(AST_LITERAL, tp, val));
}

static Node* astFloattype(Type *tp, double val) {
    return makeAst(&(Node){AST_LITERAL, tp, .fval = val});
    return makeAst(new Node(AST_LITERAL, tp, val));
}

static Node* astLvar(Type *tp, char *name) {
    Node *r = makeAst(new Node(AST_LVAR, tp, name));
    if (localenv) localenv->insert(); 
}

static Type* makeType(Type *tmpl) {
    Type *r = new Type;
    *r = *tmpl;
    return r;
}

static Type *makePtrType(Type *tp) {
    return makeType(new Type(KIND_PTR, tp, 8, 8));
}

static Node* astGvar(Type *tp, char *name) {
    Node *r = makeAst();
}

static void defineBuiltin(char *name, Type *rettype) {
    astGvar()
}

void parseInit() {
    std::vector<Type*> *voidptr = new std::vector<Type*>;
    voidptr->push_back(makePtrType(type_void));
    std::vector<Type*> *two_voidptrs = new std::vector<Type*>;
    two_voidptrs->push_back(makePtrType(type_void));
    two_voidptrs->push_back(makePtrType(type_void));
}
