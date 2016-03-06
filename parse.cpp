#include "cic.h"

const int MAX_ALIGN = 16;
SourceLoc *source_loc;

struct Case {
    int beg;
    int end;
    char *label;
    Case() = default;
    Case(int b, int e, char *l): beg(b), end(e), label(newChar(l)) {}
};
typedef Map<Node> nmap;
typedef Map<Type> tmap;

static nmap *globalenv = new nmap;
static nmap *localenv;
static tmap *tags = new tmap;
static nmap *labels;
static auto keywords = new Map<void>;

typedef std::vector<Node*> nvec;
typedef std::vector<Type*> tvec;
static nvec *toplevels;
static nvec *localvars;
static nvec *gotos;
std::vector<Case*> *cases;
static Type *current_func_type;

//static Map *globalenv = new Map;
//static Map *localenv;
//static Map *tags = new Map;

static char *defaultcase;
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

static Type* makePtrType(Type*);
static Type* makeArrayType(Type*, int);
static Node* readCompoundStmt();
static void readDeclOrStmt(nvec*);
static Node* conv(Node*);
static Node* readStmt();
static bool isType(Token*);
static Node* readUnaryExpr();
static void readDecl(nvec*, bool);
static Type* readDeclaratorTail(Type*, nvec*);
static Type* readDeclarator(char**, Type*, nvec*, int);
static Type *readAbstractDecl(Type*);
static Type *readDeclSpec(int*);
static Node *readStructField(Node*);
static void readInitList(nvec*, Type*, int, bool);
static Type* readCastType();
static nvec* readDeclInit(Type*);
static Node* readBooleanExpr();
Node* readExpr();
static Node* readExprOpt();
static Node* readConditionExpr();
static Node* readAssignmentExpr();
static Node* readCastExpr();
static Node* readCommaExpr();
static Token* get();
static Token* peek();
int evalIntexpr(Node*, Node**);

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

static Type* makePtrType(Type *tp);
static Type* makeArraytype(Type *tp, int size);

template <class T>
void mapInsert(Map<T> *m, char *key, T *val) {
    if (m->body->find(key) != m->body->end())
        m->body->erase(key);
    m->body->insert(std::pair<char*, T*>(key, val));
}

static Token* copyToken(Token *tk) {
    Token *r = new Token(*tk);
    return r;
}

static Token* maybeConvertKeyword(Token *tk) {
    if (tk->kind != TIDENT) return tk;
    int id = (intptr_t)(keywords->get(tk->sval));
    if (!id) return tk;
    Token *r = copyToken(tk);
    r->kind = TKEYWORD;
    r->id = id;
    return r;
}

Token* readToken() {
    return lex();
    Token *tk;
    while (1) {
        tk = lex();
        return maybeConvertKeyword(tk);
    }
}

Token* peekToken() {
    Token *r = readToken();
    ungetToken(r);
    return r;
}

static Token* peek() {
    return peekToken();
}

static void linkString(Token *tk) {
    Buffer *b = makeBuffer();
    bufAppend(b, tk->sval, tk->slen - 1);
    while (peek()->kind == TSTRING) {
        Token *tmp = readToken();
        bufAppend(b, tmp->sval, tmp->slen - 1);
    }
    bufWrite(b, '\0');
    tk->sval = bufBody(b);
    tk->slen = bufLen(b);
}

static Token* get() {
    Token *r = readToken();
    if (r->kind == TINVALID)
        errort(r, "stray character in program: '%c'", r->c);
    if (r->kind == TSTRING && peek()->kind == TSTRING)
        linkString(r);
    return r;
}

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

char* makeLabel() {
    static int num = 0;
    return format("@L%d", num++);
}

static char* makeStaticLabel(char *name) {
    static int num = 0;
    return format("@S%d@%s", num++, name);
}

static Case* makeCase(int beg, int end, char *label) {
    return new Case(beg, end, label);
}

static nmap* env() {
    return localenv ? localenv : globalenv;
}

static Node* makeAst(Node *tmp) {
    Node *r = new Node;
    *r = *tmp;
    r->sl = source_loc;
    return r;
}

static Node* astUop(int kind, Type *tp, Node *operand) {
    Node *r = new Node(kind, tp);
    r->operand = operand;
    return makeAst(r);
}

static Node* astBinop(Type *tp, int kind, Node *left, Node *right) {
    Node *r = makeAst(new Node(kind, tp));
    r->left = new Node(*left);
    r->right = new Node(*right);
    return r;
}

static Node* astIntType(Type* tp, long val) {
    Node *r = new Node(AST_LITERAL, tp);
    r->ival = val;
    return makeAst(r);
}

static Node* astFloattype(Type *tp, double val) {
    auto r = new Node(AST_LITERAL, tp);
    r->fval = val;
    return makeAst(r);
}

static Node* astLvar(Type *tp, char *name) {
    auto r = new Node(AST_LVAR, tp);
    r->varname = name;
    r = makeAst(r);
    if (localenv) {
        mapInsert(localenv, name, r);
    } 
    if (localvars) localvars->push_back(r);
    return r;
}

static Node* astGvar(Type *tp, char *name) {
    auto r = new Node(AST_GVAR, tp);
    r->varname = r->glabel = name;
    r = makeAst(r);
    mapInsert(globalenv, name, r);
    return r;
}

static Node* astStaticLvar(Type *tp, char *name) {
    auto r = new Node(AST_GVAR, tp);
    r->varname = name;
    r->glabel = makeStaticLabel(name);
    r = makeAst(r);
    assert(localenv);
    mapInsert(localenv, name, r);
    return r;
}

static Node* astTypedef(Type *tp, char *name) {
    auto r = makeAst(new Node(AST_TYPEDEF, tp));
    mapInsert(env(), name, r);
    return r;
}

static Node* astString(char *str, int len) {
    Type *tp = makeArraytype(type_char, len);
    auto r = new Node(AST_LITERAL, tp);
    r->sval = str;
    return makeAst(r);
}

static Node* astFuncall(Type *ftype, char *fname, nvec *args) {
    auto r = new Node(AST_FUNCALL, ftype->rettype);
    r->fname = fname;
    r->args = args;
    r->ftype = ftype;
    return makeAst(r);
}

static Node* astFuncdesg(Type *tp, char *fname) {
    auto r = new Node(AST_FUNCDESG, tp);
    r->fname = fname;
    return makeAst(r);
}

static Node* astFuncptrcall(Node *fptr, nvec *args) {
    assert(fptr->tp->kind == KIND_PTR);
    assert(fptr->tp->ptr->kind == KIND_FUNC);
    auto r = new Node(AST_FUNCPTR_CALL, fptr->tp->ptr->rettype);
    r->fptr = fptr;
    r->args = args;
    return makeAst(r);
}

static Node* astFunc(Type *tp, char *fname, nvec *params,
        Node *body, nvec *localvars) {
    auto r = new Node(AST_FUNC, tp);
    r->fname = fname;
    r->params = params;
    r->localvars = localvars;
    r->body = body;
    return makeAst(r);
}
static Node* astDecl(Node *var, nvec *init) {
    auto r = new Node(AST_DECL);
    r->declvar = var;
    r->declinit = init;
    return makeAst(r);
}

static Node* astInit(Node *val, Type *totype, int off) {
    auto r = new Node(AST_INIT);
    r->initval = val;
    r->initoff = off;
    r->totype = totype;
    return makeAst(r);
}

static Node* astConv(Type *totype, Node *val) {
    auto r = new Node(AST_CONV, totype);
    r->operand = val;
    return makeAst(r);
}

static Node* astIf(Node *cond, Node *then, Node *els) {
    auto r = new Node(AST_IF);
    r->cond = cond;
    r->then = then;
    r->els = els;
    return makeAst(r);
}

static Node* astTernary(Type *tp, Node *cond, Node *then, Node *els) {
    auto r = new Node(AST_TERNARY, tp);
    r->cond = cond;
    r->then = then;
    r->els = els;
    return makeAst(r);
}

static Node* astReturn(Node *retval) {
    auto r = new Node(AST_RETURN);
    r->retval = retval;
    return makeAst(r);
}

static Node* astCompoundstmt(nvec *stmts) {
    auto r = new Node(AST_COMPOUND_STMT);
    r->stmts = stmts;
    return makeAst(r);
}

static Node* astStructref(Type *tp, Node *struc, char *name) {
    auto r = new Node(AST_STRUCT_REF, tp);
    r->struc = struc;
    r->field = name;
    return makeAst(r);
}

static Node* astGoto(char *label) {
    auto r = new Node(AST_GOTO);
    r->label = label;
    return makeAst(r);
}

static Node* astJump(char *label) {
    auto r = new Node(AST_GOTO);
    r->label = label;
    r->newlabel = label;
    return makeAst(r);
}

static Node* astComputedgoto(Node *expr) {
    auto r = new Node(AST_COMPUTED_GOTO);
    r->operand = expr;
    return makeAst(r);
}

static Node* astLabel(char *label) {
    auto r = new Node(AST_LABEL);
    r->label = label;
    return makeAst(r);
}

static Node* astDest(char *label) {
    auto r = new Node(AST_LABEL);
    r->label = label;
    r->newlabel = label;
    return makeAst(r);
}

static Node* astLabelAddr(char *label) {
    auto r = new Node(OP_LABEL_ADDR, makePtrType(type_void));
    r->label = label;
    return makeAst(r);
}

static Type* makeType(Type *tmpl) {
    return new Type(*tmpl);
}

static Type* copyType(Type *tp) {
    Type *r = new Type;
    memcpy(r, tp, sizeof(Type));
    return r;
}

static Type* makeNumtype(int kind, bool usig) {
    Type *r = new Type;
    r->kind = kind;
    r->usig = usig;
    int sz = 0;
    if (kind == KIND_VOID) sz = 0;
    else if (kind == KIND_BOOL) sz = 1;
    else if (kind == KIND_CHAR) sz = 1;
    else if (kind == KIND_SHORT) sz = 2;
    else if (kind == KIND_INT) sz = 4;
    else if (kind == KIND_LONG) sz = 8;
    else if (kind == KIND_LLONG) sz = 8;
    else if (kind == KIND_FLOAT) sz = 4;
    else if (kind == KIND_DOUBLE) sz = 8;
    else if (kind == KIND_LDOUBLE) sz = 8;
    else error("internal error");
    r->size = r->align = sz;
    return r;
}

static Type *makePtrType(Type *tp) {
    return makeType(new Type(KIND_PTR, tp, 8, 8));
}

static Type* makeArraytype(Type *tp, int len) {
    int sz = -1;
    if (len >= 0) sz = tp->size * len;
    return makeType(new Type(KIND_ARRAY, tp, sz, len, tp->align));
}

static Type* makeRectype(bool is_struct) {
    Type *r = new Type(KIND_STRUCT);
    r->isstruct = is_struct;
    return makeType(r);
}

static Type* makeFunctype(Type *rettype, tvec *paramtypes,
        bool hv, bool os) {
    return makeType(new Type(KIND_FUNC, rettype, paramtypes, hv, os));
}

static Type* makeStubType() {
    return makeType(new Type(KIND_STUB));
}

bool isInttype(Type *tp) {
    switch (tp->kind) {
        case KIND_BOOL: case KIND_CHAR: case KIND_SHORT:
        case KIND_LONG: case KIND_INT: case KIND_LLONG:
            return 1;
        default: return 0;
    }
}

bool isFlotype(Type *tp) {
    switch (tp->kind) {
        case KIND_FLOAT: case KIND_DOUBLE: case KIND_LDOUBLE:
            return  1;
        default: return 0;
    }
}

static bool isArithtype(Type *tp) {
    return isInttype(tp) || isFlotype(tp); 
}

static bool isString(Type *tp) {
    return tp->kind == KIND_ARRAY && tp->ptr->kind == KIND_CHAR;
}

static void ensureLvalue(Node *node) {
    switch (node->kind) {
        case AST_LVAR: case AST_GVAR: case AST_DEREF: case AST_STRUCT_REF:
            return;
        default: error("lvalue expected, but got %s", node2s(node));
    }
}

static void ensureInttype(Node *node) {
    if (!isInttype(node->tp))
        error("integer type expected, but got %s", node2s(node));
}

static void ensureArithtype(Node *node) {
    if (!isArithtype(node->tp))
        error("arithmetic type expected, but got %s", node2s(node));
}

static void ensureNotvoid(Type *tp) {
    if (tp->kind == KIND_VOID)
        error("void is not allowed");
}

static void expect(char id) {
    Token *tk = get();
    if (!isKeyword(tk, id))
        errort(tk, "'%c' expected, but got %s", id, tk2s(tk));
}

static Type* copyIncompletetype(Type *tp) {
    if (!tp) return NULL;
    return (tp->len == -1) ? copyType(tp) : tp;
}

static Type* getTypedef(char *name) {
    puts(name);
    auto node = env()->get(name);
    //Node *node = env()->body->find(name)->second;
    return (node && node->kind == AST_TYPEDEF) ? node->tp : NULL;
}

static bool isType(Token *tk) {
    puts("istype");
    if (tk->kind == TIDENT) return getTypedef(tk->sval);
    if (tk->kind != TKEYWORD) return 0;
    switch (tk->kind) {
#define op(x, y)
#define keyword(id, _, istype) case id: return istype;
#include "keyword.inc"
#undef keyword
#undef op
        default: return 0;
    }
}

static bool nextToken(int kind) {
    Token *tk = get();
    if (isKeyword(tk, kind)) return 1;
    ungetToken(tk);
    return 0;
}

static Node* conv(Node *node) {
    if (!node) return NULL;
    Type *tp = node->tp;
    switch (tp->kind) {
        case KIND_ARRAY:
            return astUop(AST_CONV, makePtrType(tp->ptr), node);
        case KIND_FUNC:
            return astUop(AST_ADDR, makePtrType(tp), node);
        case KIND_SHORT: case KIND_CHAR: case KIND_BOOL:
            return astConv(type_int, node);
        case KIND_INT:
            if (tp->bitsize > 0) return astConv(type_int, node);
    }
    return node;
}

static bool sameArithtype(Type *t, Type *u) {
    return t->kind == u->kind && t->usig == u->usig;
}

static Node* wrap(Type *t, Node *node) {
    if (sameArithtype(t, node->tp)) return node;
    return astUop(AST_CONV, t, node);
}

static Type* usualArithConv(Type *t, Type *u) {
    assert(isArithtype(t));
    assert(isArithtype(u));
    if (t->kind < u->kind) {
        Type *temp = t;
        t = u;
        u = temp;
    }
    if (isFlotype(t)) return t;
    assert(isInttype(t) && t->size >= type_int->size);
    assert(isInttype(u) && u->size >= type_int->size);
    if (t->size > u->size) return t;
    assert(t->size == u->size);
    if (t->usig == u->usig) return t;
    Type *r = copyType(t);
    r->usig = 1;
    return r;
}

static bool validPointbinop(int op) {
    switch (op) {
        case '-': case '<': case '>': case OP_EQ:
        case OP_NE: case OP_GE: case OP_LE:
            return 1;
        default: return 0;
    }
}

static Node* binop(int op, Node *lhs, Node *rhs) {
    if (lhs->tp->kind == KIND_PTR && rhs->tp->kind == KIND_PTR) {
        if (!validPointbinop(op))
            error("invalid pointer arith");
        if (op == '-')
            return astBinop(type_long, op, lhs, rhs);
        return astBinop(type_int, op, lhs, rhs);
    }
    if (lhs->tp->kind == KIND_PTR)
        return astBinop(lhs->tp, op, lhs, rhs);
    if (rhs->tp->kind == KIND_PTR)
        return astBinop(rhs->tp, op, rhs, lhs);
    assert(isArithtype(lhs->tp));
    assert(isArithtype(rhs->tp));
    Type *r = usualArithConv(lhs->tp, rhs->tp);
    return astBinop(r, op, wrap(r, lhs), wrap(r, rhs));
}

static bool isSameStruct(Type *a, Type *b) {
    if (a->kind != b->kind) return 0;
    switch (a->kind) {
        case KIND_ARRAY:
            return a->len = b->len && isSameStruct(a->ptr, b->ptr);
        case KIND_PTR:
            return isSameStruct(a->ptr, b->ptr);
        case KIND_STRUCT: {
            if (a->isstruct != b->isstruct) return 0;
            auto ka = a->fields->key;
            auto kb = b->fields->key;
            if (ka->size() != kb->size()) return 0;
            for (unsigned i = 0; i < ka->size(); i++)
                if (!isSameStruct((Type*)(*ka)[i], (Type*)(*kb)[i]))
                    return 0;
            return 1;
        }
        default: return 1;
    }
}

static void ensureAssignable(Type *to, Type *from) {
    if ((isArithtype(to) || to->kind == KIND_PTR) &&
            (isArithtype(from) || from->kind == KIND_PTR))
        return;
    if (isSameStruct(to, from)) return;
    error("incompatible kind: <%s> <%s>", tp2s(to), tp2s(from));
}

static int evalStructref(Node *node, int offset) {
    if (node->kind == AST_STRUCT_REF)
        return evalStructref(node->struc, node->tp->offset + offset);
    return evalIntexpr(node, NULL) + offset;
}

int evalIntexpr(Node *node, Node **addr) {
    switch (node->kind) {
        case AST_LITERAL:
            if (isInttype(node->tp))
                return node->ival;
            error("Integer expression expected, but got %s", node2s(node));
        case '!': return !evalIntexpr(node->operand, addr);
        case '~': return ~evalIntexpr(node->operand, addr);
        case OP_CAST: return evalIntexpr(node->operand, addr);
        case AST_CONV: return evalIntexpr(node->operand, addr);
        case AST_ADDR:
            if (node->operand->kind == AST_STRUCT_REF)
                return evalStructref(node->operand, 0);
        case AST_GVAR:
            if (addr) {
                *addr = conv(node);
                return 0;
            }
            goto error;
            goto error;
        case AST_DEREF:
            if (node->operand->tp->kind == KIND_PTR)
                return evalIntexpr(node->operand, addr);
            goto error;
        case AST_TERNARY: {
            long cond = evalIntexpr(node->cond, addr);
            if (cond)
                return node->then ? evalIntexpr(node->then, addr) : cond;
            return evalIntexpr(node->els, addr);
        }
#define L (evalIntexpr(node->left, addr))
#define R (evalIntexpr(node->right, addr))
        case '+': return L + R;
        case '-': return L - R;
        case '*': return L * R;
        case '/': return L / R;
        case '<': return L < R;
        case '^': return L ^ R;
        case '&': return L & R;
        case '|': return L | R;
        case '%': return L % R;
        case OP_EQ: return L == R;
        case OP_LE: return L <= R;
        case OP_NE: return L != R;
        case OP_SAL: return L << R;
        case OP_SAR: return L >> R;
        case OP_SHR: return ((unsigned long)L) >> R;
        case OP_LOGAND: return L && R;
        case OP_LOGOR: return L || R;
#undef L
#undef R
        default:
        error:
            error("Integer expression expected, but got %s", node2s(node));
    }
}

static int readIntexpr() {
    return evalIntexpr(readConditionExpr(), NULL);
}

/*
 * Numeric literal
 */

static Type* readIntSuffix(char *s) {
    if (!strcasecmp(s, "u")) return type_uint;
    if (!strcasecmp(s, "l")) return type_long;
    if (!strcasecmp(s, "ul")) return type_ulong;
    if (!strcasecmp(s, "ll")) return type_llong;
    if (!strcasecmp(s, "ull")) return type_ullong;
    return NULL;
}

static Node* readInt(Token *tk) {
    char *s = tk->sval;
    char *end;
    long v = !strncasecmp(s, "0b", 2) ?
        strtoul(s + 2, &end, 2) : strtoul(s, &end, 0);
    Type *tp = readIntSuffix(end);
    if (tp) return astIntType(tp, v);
    if (*end != '\0')
        errort(tk, "invalid character '%c': %s", *end, s);
    bool base10 = (*s - '0');
    if (base10) {
        tp = !(v & ~(long)INT_MAX) ? type_int : type_long;
        return astIntType(tp, v);
    }
    tp = !(v & ~(unsigned long)INT_MAX) ? type_int
        : !(v & ~(unsigned long)UINT_MAX) ? type_uint
        : !(v & ~(unsigned long)LONG_MAX) ? type_long
        : type_ulong;
    return astIntType(tp, v);
}

static Node* readFloat(Token *tk) {
    char *s = tk->sval;
    char *end;
    double v = strtod(s, &end);
    if (!strcasecmp(end, "l")) return astFloattype(type_ldouble, v);
    if (!strcasecmp(end, "f")) return astFloattype(type_float, v);
    if (*end != '\0')
        errort(tk, "invalid character '%c': '%s'", *end, s);
    return astFloattype(type_double, v);
}

static Node* readNumber(Token *tk) {
    char *s = tk->sval;
    bool isfloat = strpbrk(s, ".pP") ||
        (strncasecmp(s, "0x", 2) && strpbrk(s, "eE"));
    return isfloat ? readFloat(tk) : readInt(tk);
}

/*
 * sizeof operator
 */

static Type* readSizeofOperandSub() {
    Token *tk = get();
    if (isKeyword(tk, '(') && isType(peek())) {
        Type *r = readCastType();
        expect(')');
        return r;
    }
    ungetToken(tk);
    return readUnaryExpr()->tp;
}

static Node* readSizeofOperand() {
    Type *tp = readSizeofOperandSub();
    int sz = (tp->kind == KIND_VOID || tp->kind == KIND_FUNC)
        ? 1 : tp->size;
    assert(sz >= 0);
    return astIntType(type_ulong, sz);
}
/*
 * alignof op
 */

static Node* readAlignofOperand() {
    expect('(');
    Type *tp = readCastType();
    expect(')');
    return astIntType(type_ulong, tp->align);
}

/*
 * function args
 */

static nvec* readFuncArgs(std::vector<Type*> *params) {
    std::vector<Node*> *args = new std::vector<Node*>;
    int i = 0;
    while (1) {
        if (nextToken(')')) break;
        Node *arg = conv(readAssignmentExpr());
        Type *paramtype;
        if (i < params->size()) paramtype = (*params)[i++];
        else {
            paramtype = isFlotype(arg->tp) ? type_double :
                isInttype(arg->tp) ? type_int : arg->tp;
        }
        ensureAssignable(paramtype, arg->tp);
        if (paramtype->kind != arg->tp->kind)
            arg = astConv(paramtype, arg);
        args->push_back(arg);
        Token *tk = get();
        if (isKeyword(tk, ')')) break;
        if (!isKeyword(tk, ','))
            errort(tk, "unexpected token: '%s'", tk2s(tk));
    }
    return args;
}

static Node* readFuncall(Node *fp) {
    if (fp->kind == AST_ADDR && fp->operand->kind == AST_FUNCDESG) {
        Node *desg = fp->operand;
        auto args = readFuncArgs(desg->tp->params);
        return astFuncall(desg->tp, desg->fname, args);
    }
    auto args = readFuncArgs(fp->tp->ptr->params);
    return astFuncptrcall(fp, args);
}

/*
 * expression
 */

static Node* readVarOrFunc(char *name) {
    auto v = env()->get(name);
    //Node *v = env()->body->find(name)->second;
    if (!v) {
        Token *tk = peek();
        if (!isKeyword(tk, '('))
            errort(tk, "undefined variable: %s", name);
        Type *tp = makeFunctype(type_int, new tvec, 1, 0);
        warnt(tk, "assume returning int: %s()", name);
        return astFuncdesg(tp, name);
    }
    if (v->tp->kind == KIND_FUNC)
        return astFuncdesg(v->tp, name);
    return v;
}

static int getCompoundAssignOp(Token *tk) {
    if (tk->kind != TKEYWORD) return 0;
    switch (tk->id) {
        case OP_A_ADD: return '+';
        case OP_A_SUB: return '-';
        case OP_A_MUL: return '*';
        case OP_A_DIV: return '/';
        case OP_A_MOD: return '%';
        case OP_A_AND: return '&';
        case OP_A_OR:  return '|';
        case OP_A_XOR: return '^';
        case OP_A_SAL: return OP_SAL;
        case OP_A_SAR: return OP_SAR;
        case OP_A_SHR: return OP_SHR;
        default: return 0;
    }
}

static Node* readStmtExpr() {
    Node *r = readCompoundStmt();
    expect(')');
    Type *rtype = type_void;
    if (r->stmts->size() > 0) {
        Node *lastexpr = (*r->stmts)[r->stmts->size()-1];
        if (lastexpr->tp) rtype = lastexpr->tp;
    }
    r->tp = rtype;
    return r;
}

static Type* charType() {
    return type_int;
}

static Node* readPrimaryExpr() {
    Token *tk = get();
    if (!tk) return NULL;
    if (isKeyword(tk, '(')) {
        if (nextToken('{'))
            return readStmtExpr();
        Node *r = readExpr();
        expect(')');
        return r;
    }
    switch (tk->kind) {
        case TIDENT: return readVarOrFunc(tk->sval);
        case TNUMBER: return readNumber(tk);
        case TCHAR: return astIntType(charType(), tk->c);
        case TSTRING: return astString(tk->sval, tk->slen);
        case TKEYWORD:
            ungetToken(tk);
            return NULL;
        default:
            error("internal error: unknown token kind: %d", tk->kind);
    }
}

static Node* readSubscriptExpr(Node *node) {
    Token *tk = peek();
    Node *sub = readExpr();
    if (!sub)
        errort(tk, "subscription expected");
    expect(']');
    Node *t = binop('+', conv(node), conv(sub));
    return astUop(AST_DEREF, t->tp->ptr, t);
}

static Node* readPostfixExprTail(Node *node) {
    if (!node) return NULL;
    while (1) {
        if (nextToken('(')) {
            Token *tk = peek();
            node = conv(node);
            Type *tp = node->tp;
            if (tp->kind != KIND_PTR || tp->ptr->kind != KIND_FUNC)
                errort(tk, "function expected, but got %s", node2s(node));
            node = readFuncall(node);
            continue;
        }
        if (nextToken('[')) {
            node = readSubscriptExpr(node);
            continue;
        }
        if (nextToken('.')) {
            node = readStructField(node);
            continue;
        }
        if (nextToken(OP_ARROW)) {
            if (node->tp->kind != KIND_PTR)
                error("pointer type expected, but got %s %s",
                        tp2s(node->tp), node2s(node));
            node = astUop(AST_DEREF, node->tp->ptr, node);
            node = readStructField(node);
            continue;
        }
        Token *tk = peek();
        if (nextToken(OP_INC) || nextToken(OP_DEC)) {
            ensureLvalue(node);
            int op = isKeyword(tk, OP_INC) ? OP_POST_INC : OP_POST_DEC;
            return astUop(op, node->tp, node);
        }
        return node;
    }
}

static Node* readPostfixExpr() {
    Node *node = readPrimaryExpr();
    return readPostfixExprTail(node);
}

static Node* readUnaryIncdec(int op) {
    Node *operand = readUnaryExpr();
    operand = conv(operand);
    ensureLvalue(operand);
    return astUop(op, operand->tp, operand);
}

static Node* readLabelAddr(Token *tk) {
    Token *tk2 = get();
    if (tk2->kind != TIDENT)
        errort(tk, "label name expected after &&, but got %s", tk2s(tk2));
    Node *r = astLabelAddr(tk2->sval);
    gotos->push_back(r);
    return r;
}

static Node* readUnaryAddr() {
    Node *operand = readCastExpr();
    if (operand->kind == AST_FUNCDESG)
        return conv(operand);
    ensureLvalue(operand);
    return astUop(AST_ADDR, makePtrType(operand->tp), operand);
}

static Node* readUnaryDeref(Token *tk) {
    Node *operand = conv(readCastExpr());
    if (operand->tp->kind != KIND_PTR)
        errort(tk, "pointer type expected, but got %s", node2s(operand));
    if (operand->tp->ptr->kind == KIND_FUNC)
        return operand;
    return astUop(AST_DEREF, operand->tp->ptr, operand);
}

static Node* readUnaryMinus() {
    Node* expr = readCastExpr();
    ensureArithtype(expr);
    if (isInttype(expr->tp))
        return binop('-', conv(astIntType(expr->tp, 0)), conv(expr));
    return binop('-', astFloattype(expr->tp, 0), expr);
}

static Node* readUnaryBitnot(Token *tk) {
    Node *expr = readCastExpr();
    expr = conv(expr);
    if (!isInttype(expr->tp))
        errort(tk, "invalid use of ~: %s", node2s(expr));
    return astUop('~', expr->tp, expr);
}

static Node* readUnaryLognot() {
    Node *operand = readCastExpr();
    operand = conv(operand);
    return astUop('!', type_int, operand);
}

static Node* readUnaryExpr() {
    Token *tk = get();
    if (tk->kind == TKEYWORD) {
        switch (tk->id) {
            case KSIZEOF: return readSizeofOperand();
            case KALIGNOF: return readAlignofOperand();
            case OP_INC: return readUnaryIncdec(OP_PRE_INC);
            case OP_DEC: return readUnaryIncdec(OP_PRE_DEC);
            case OP_LOGAND: return readLabelAddr(tk);
            case '&': return readUnaryAddr();
            case '*': return readUnaryDeref(tk);
            case '+': return readCastExpr();
            case '-': return readUnaryMinus();
            case '~': return readUnaryBitnot(tk);
            case '!': return readUnaryLognot();
        }
    }
    ungetToken(tk);
    return readPostfixExpr();
}

static Node* readCompoundLiteral(Type *tp) {
    char *name = makeLabel();
    auto init = readDeclInit(tp);
    readDeclInit(tp);
    Node *r = astLvar(tp, name);
    r->lvarinit = init;
    return r;
}

static Type* readCastType() {
    return readAbstractDecl(readDeclSpec(NULL));
}

static Node* readCastExpr() {
    Token *tk = get();
    if (isKeyword(tk, '(' && isType(peek()))) {
        Type *tp = readCastType();
        expect(')');
        if (isKeyword(peek(), '{')) {
            Node *node = readCompoundLiteral(tp);
            return readPostfixExprTail(node);
        }
        return astUop(OP_CAST, tp, readCastExpr());
    }
    ungetToken(tk);
    return readUnaryExpr();
}

static Node* readMultiExpr() {
    Node *node = readCastExpr();
    while (1) {
        if (nextToken('*')) 
            node = binop('*', conv(node), conv(readCastExpr()));
        else if (nextToken('/'))
            node = binop('/', conv(node), conv(readCastExpr()));
        else if (nextToken('%'))
            node = binop('%', conv(node), conv(readCastExpr()));
        else return node;
    }
}

static Node* readAddiExpr() {
    Node *node = readMultiExpr();
    while (1) {
        if (nextToken('+'))
            node = binop('+', conv(node), conv(readMultiExpr()));
        else if (nextToken('-'))
            node = binop('-', conv(node), conv(readMultiExpr()));
        else return node;
    }
}

static Node* readShiftExpr() {
    Node *node = readAddiExpr();
    while (1) {
        int op;
        if (nextToken(OP_SAL)) op = OP_SAL;
        else if (nextToken(OP_SAR))
            op = node->tp->usig ? OP_SHR : OP_SAR;
        else break;
        Node *right = readAddiExpr();
        ensureInttype(node);
        ensureInttype(right);
        node = astBinop(node->tp, op, conv(node), conv(right));
    }
    return node;
}

static Node* readRelationExpr() {
    Node *node = readShiftExpr();
    while (1) {
        if (nextToken('<'))
            node = binop('<', conv(node), conv(readShiftExpr()));
        else if (nextToken('>'))
            node = binop('>', conv(readShiftExpr()), conv(node));
        else if (nextToken(OP_LE))
            node = binop(OP_LE, conv(node), conv(readShiftExpr()));
        else if (nextToken(OP_GE))
            node = binop(OP_LE, conv(readShiftExpr()), conv(node));
        else return node;
        node->tp = type_int;
    }
}

static Node* readEqualityExpr() {
    Node *node = readRelationExpr();
    Node *r;
    if (nextToken(OP_EQ))
        r = binop(OP_EQ, conv(node), conv(readEqualityExpr()));
    else if (nextToken(OP_NE))
        r = binop(OP_NE, conv(node), conv(readEqualityExpr()));
    else return node;
    r->tp = type_int;
    return r;
}

static Node* readBitandExpr() {
    Node *node = readEqualityExpr();
    while (nextToken('&'))
        node = binop('&', conv(node), conv(readEqualityExpr()));
    return node;
}

static Node* readBitxorExpr() {
    Node *node = readBitandExpr();
    while (nextToken('^'))
        node = binop('^', conv(node), conv(readBitandExpr()));
    return node;
}

static Node* readBitorExpr() {
    Node *node = readBitxorExpr();
    while (nextToken('|'))
        node = binop('|', conv(node), conv(readBitxorExpr()));
    return node;
}

static Node* readLogandExpr() {
    Node *node = readBitorExpr();
    while (nextToken(OP_LOGAND))
        node = astBinop(type_int, OP_LOGAND, node, readBitorExpr());
    return node;
}

static Node* readLogorExpr() {
    Node *node = readLogandExpr();
    while (nextToken(OP_LOGOR))
        node = astBinop(type_int, OP_LOGOR, node, readLogandExpr());
    return node;
}

static Node* doReadConditionExpr(Node *cond) {
    Node *then = conv(readCommaExpr());
    expect(':');
    Node *els = conv(readConditionExpr());
    Type *t = then ? then->tp : cond->tp;
    Type *u = els->tp;
    if (isArithtype(t) && isArithtype(u)) {
        Type *r = usualArithConv(t, u);
        return astTernary(r, cond, (then ? wrap(r, then) : NULL),
                wrap(r, els));
    }
    return astTernary(u, cond, then, els);
}

static Node* readConditionExpr() {
    Node *cond = readLogorExpr();
    if (!nextToken('?')) return cond;
    return doReadConditionExpr(cond);
}

static Node* readAssignmentExpr() {
    Node *node = readLogorExpr();
    Token *tk = get();
    if (!tk) return node;
    if (isKeyword(tk, '?'))
        return doReadConditionExpr(node);
    int cop = getCompoundAssignOp(tk);
    if (isKeyword(tk, '=') || cop) {
        Node *value = conv(readAssignmentExpr());
        if (isKeyword(tk, '=') || cop)
            ensureLvalue(node);
        Node *right = cop ? binop(cop, conv(node), value) : value;
        if (isArithtype(node->tp) && node->tp->kind != right->tp->kind)
            right = astBinop(node->tp, '=', node, right);
    }
    ungetToken(tk);
    return node;
}

static Node* readCommaExpr() {
    Node *node = readAssignmentExpr();
    while (nextToken(',')) {
        Node *expr = readAssignmentExpr();
        node = astBinop(expr->tp, ',', node, expr);
    }
    return node;
}

Node* readExpr() {
    Token *tk = peek();
    Node *r = readCommaExpr();
    if (!r) errort(tk, "expression expected");
    return r;
}

static Node* readExprOpt() {
    return readCommaExpr();
}

/*
 * struct or union
 */

static Node* readStructField(Node *struc) {
    if (struc->tp->kind != KIND_STRUCT)
        error("struct expected, but got %s", node2s(struc));
    Token *name = get();
    if (name->kind != TIDENT)
        error("field name expected, but got %s", tk2s(name));
    Type *field = dictGet(struc->tp->fields, name->sval);
    if (!field)
        error("struct has no such field: %s", tk2s(name));
    return astStructref(field, struc, name->sval);
}

static char* readRectypeTag() {
    Token *tk = get();
    if (tk->kind == TIDENT) return tk->sval;
    ungetToken(tk);
    return NULL;
}

static int computePadding(int offset, int align) {
    return (offset % align == 0) ? 0 : align - offset % align;
}

static void squashUnnamedStruct(Dict<Type> *dc, Type *unnamed, int offset) {
    auto keys = unnamed->fields->key;
    for (unsigned i = 0; i < keys->size(); i++) {
        char *name = (char*)(*keys)[i];
        Type *t = copyType(dictGet(unnamed->fields, name));
        t->offset += offset;
        dictPut(dc, name, t);
    }
}

static int readBitsize(char *name, Type *tp) {
    if (isInttype(tp))
        error("non-integer type cannot be a bitfield: %s", tp2s(tp));
    Token *tk = peek();
    int r = readIntexpr();
    int maxsize = tp->kind == KIND_BOOL ? 1 : tp->size * 8;
    if (r < 0 || maxsize < r)
        errort(tk, "invalid bitfield size for %s: %d", tp2s(tp), r);
    if (r == 0 && name != NULL)
        errort(tk, "zero-width bitfield needs to be unnamed: %s", name);
    return r;
}

static std::vector<std::pair<char*, Type*>* >* readRectypeFieldsSub() {
    auto r = new std::vector<std::pair<char*, Type*>*>;
    while (1) {
        if (!isType(peek())) break;
        Type *basetype = readDeclSpec(NULL);
        if (basetype->kind == KIND_STRUCT && nextToken(';')) {
            r->push_back(new std::pair<char*, Type*>(NULL, basetype));
            continue;
        }
    }
    expect('}');
    return r;
}

static void fixRectypeFlexibleMember(std::vector<std::pair<char*,
        Type*>* > *fields) {
    for (unsigned i = 0; i < fields->size(); i++) {
        std::pair<char*, Type*> *pp = (*fields)[i];
        char *name = pp->first;
        Type *tp = pp->second;
        if (tp->kind != KIND_ARRAY) continue;
        if (tp->len == -1) {
            if (i != fields->size() - 1)
                error("flexible member may only appear as the last member: %s %s", tp2s(tp));
            if (fields->size() == 1)
                error("flexible member with no other fields: %s %s",
                        tp2s(tp), name);
            tp->len = 0;
            tp->size = 0;
        }
    }
}

static void finishBitfield(int *off, int *bitoff) {
    *off += (*bitoff + 7) / 8;
    *bitoff = 0;
}

static Dict<Type>* updateStructOffset(int *rsize, int *align, std::vector<
        std::pair<char*, Type*>* > *fields) {
    int off = 0, bitoff = 0;
    auto *r = makeDict<Type>();
    for (unsigned i = 0; i < fields->size(); i++) {
        std::pair<char*, Type*> *pp = (*fields)[i];
        char *name = pp->first;
        Type *fieldtype = pp->second;
        if (name) *align = std::max(*align, fieldtype->align);
        if (name == NULL && fieldtype->kind == KIND_STRUCT) {
            finishBitfield(&off, &bitoff);
            off += computePadding(off, fieldtype->align);
            squashUnnamedStruct(r, fieldtype, off);
            off += fieldtype->size;
            continue;
        }
        if (fieldtype->bitsize == 0) {
            finishBitfield(&off, &bitoff);
            off += computePadding(off, fieldtype->align);
            bitoff = 0;
            continue;
        }
        if (fieldtype->bitsize > 0) {
            int bit = fieldtype->size * 8;
            int room = bit - (off * 8 + bitoff) % bit;
            if (fieldtype->bitsize <= room) {
                fieldtype->offset = off;
                fieldtype->bitoff = bitoff;
            }
            else {
                finishBitfield(&off, &bitoff);
                off += computePadding(off, fieldtype->align);
                fieldtype->offset = off;
                fieldtype->bitoff = 0;
            }
            bitoff += fieldtype->bitsize;
        }
        else {
            finishBitfield(&off, &bitoff);
            off += computePadding(off, fieldtype->align);
            fieldtype->offset = off;
            off += fieldtype->size;
        }
        if (name) dictPut(r, name, fieldtype);
    }
}

static Dict<Type>* updateUnionOffset(int *rsize, int *align, std::vector<
        std::pair<char*, Type*>* > *fields) {
    int maxsize = 0;
    auto *r = makeDict<Type>();
    for (unsigned i = 0; i < fields->size(); i++) {
        std::pair<char*, Type*> *pp = (*fields)[i];
        char *name = pp->first;
        Type *fieldtype = pp->second;
        maxsize = std::max(maxsize, fieldtype->size);
        *align = std::max(*align, fieldtype->align);
        if (name == NULL && fieldtype->kind == KIND_STRUCT) {
            squashUnnamedStruct(r, fieldtype, 0);
            continue;
        }
        fieldtype->offset = 0;
        if (fieldtype->bitsize >= 0) fieldtype->bitoff = 0;
        if (name) dictPut(r, name, fieldtype);
    }
    *rsize = maxsize + computePadding(maxsize, *align);
    return r;
}

static Dict<Type>* readRectypeFields(int *rsize,
        int *align, bool is_struct) {
    if (nextToken('{')) return NULL;
    auto fields = readRectypeFieldsSub();
    fixRectypeFlexibleMember(fields);
    if (is_struct)
        return updateStructOffset(rsize, align, fields);
    return updateUnionOffset(rsize, align, fields);
}

static Type* readRectypeDef(bool is_struct) {
    char *tag = readRectypeTag();
    Type *r;
    if (tag) {
        r = tags->get(tag);
        //r = tags->body->find(tag)->second;
        if (r && (r->kind == KIND_ENUM || r->isstruct != is_struct))
            error("declarations of %s does not match", tag);
        if (!r) {
            r = makeRectype(is_struct);
            mapInsert(tags, tag, r);
        }
    }
    else r = makeRectype(is_struct);
    int sz = 0, al = 1;
    auto fields = readRectypeFields(&sz, &al, is_struct);
    r->align = al;
    if (fields) {
        r->fields = fields;
        r->size = sz;
    }
    return r;
}

static Type* readStructDef() {
    return readRectypeDef(1);
}

static Type* readUnionDef() {
    return readRectypeDef(0);
}

/*
 * enum
 */

static Type* readEnumDef() {
    char *tag = NULL;
    Token *tk = get();
    if (tk->kind == TIDENT) {
        tag = tk->sval;
        tk = get();
    }
    if (tag) {
        auto tp = tags->get(tag);
        //Type *tp = tags->body->find(tag)->second;
        if (tp && tp->kind != KIND_ENUM)
            errort(tk, "declaration of %s does not match", tag);
    }
    if (!isKeyword(tk, '{')) {
        if (!tag || !tags->get(tag))
            errort(tk, "enum tag %s is not defined", tag);
        ungetToken(tk);
        return type_int;
    }
    if (tag)
        mapInsert(tags, tag, type_enum);
    int val = 0;
    while (1) {
        tk = get();
        if (isKeyword(tk, '}')) break;
        if (tk->kind != TIDENT)
            errort(tk, "identifer expected, but got %s", tk2s(tk));
        char *name = tk->sval;
        if (nextToken('=')) val = readIntexpr();
        Node *constval = astIntType(type_int, val++);
        mapInsert(env(), name, constval);
        if (nextToken('.')) continue;
        if (nextToken('}')) break;
        errort(peek(), "',' or '}' expected, but got %s", tk2s(peek()));
    }
    return type_int;
}

/*
 * initializer
 */

static void assignString(std::vector<Node*> *inits,
        Type *tp, char *p, int off) {
    if (tp->len == -1)
        tp->len = tp->size = strlen(p) + 1;
    int i = 0;
    for (; i < tp->len && *p; i++)
        inits->push_back(astInit(astIntType(type_char, *p++),
                    type_char, off + i));
    for (; i < tp->len; i++)
        inits->push_back(astInit(astIntType(type_char, 0),
                    type_char, off + i));
}

static bool maybeReadBrace() {
    return nextToken('{');
}

static void maybeSkipComma() {
    nextToken('.');
}

static void skipToBrace() {
    while (1) {
        if (nextToken('}')) return;
        if (nextToken('.')) {
            get();
            expect('=');
        }
        Token *tk = peek();
        Node *ignore = readAssignmentExpr();
        if (!ignore) return;
        warnt(tk, "excessive initializer: %s", node2s(ignore));
        maybeSkipComma();
    }
}

static void readInitElem(std::vector<Node*> *inits,
        Type *tp, int off, bool designated) {
    nextToken('=');
    if (tp->kind == KIND_ARRAY || tp->kind == KIND_STRUCT)
        readInitList(inits, tp, off, designated);
    else if (nextToken('{')) {
        readInitElem(inits, tp, off, 1);
        expect('}');
    }
    else {
        Node *expr = conv(readAssignmentExpr());
        ensureAssignable(tp, expr->tp);
        inits->push_back(astInit(expr, tp, off));
    }
}

static int compInit(const Node *p, const Node *q) {
    int x = p->initoff, y = q->initoff;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void sortInits(std::vector<Node*> *inits) {
    std::sort(inits->begin(), inits->end(), compInit);
}

static void readStructInitSub(std::vector<Node*> *inits,
        Type *tp, int off, bool designated) {
    bool has_brace = maybeReadBrace();
    std::vector<void*> *keys = tp->fields->key;
    int i = 0;
    while (1) {
        Token *tk = get();
        if (isKeyword(tk, '}')) {
            if (!has_brace) ungetToken(tk);
            return;
        }
        char *fieldname;
        Type *fieldtype;
        if ((isKeyword(tk, '.') || isKeyword(tk, '[')) &&
                !has_brace && !designated) {
            ungetToken(tk);
            return;
        }
        if (isKeyword(tk, '.')) {
            tk = get();
            if (!tk || tk->kind != TIDENT)
                errort(tk, "malformed desginated initializer: %s",
                        tk2s(tk));
            fieldname = tk->sval;
            fieldtype = dictGet(tp->fields, fieldname);
            if (!fieldtype)
                errort(tk, "field does not exist: %s", tk2s(tk));
            keys = tp->fields->key;
            i = 0;
            while (i < keys->size()) {
                char *s = (char*)(*keys)[i++];
                if (strcmp(fieldname, s) == 0) break;
            }
            designated = 1;
        }
        else {
            ungetToken(tk);
            if (i == keys->size()) break;
            fieldname = (char*)(*keys)[i++];
            fieldtype = dictGet(tp->fields, fieldname);
        }
        readInitElem(inits, fieldtype,
                off + fieldtype->offset, designated);
        maybeSkipComma();
        designated = 0;
        if (!tp->isstruct) break;
    }
    if (has_brace) skipToBrace();
}

static void readStructInit(std::vector<Node*> *inits,
        Type *tp, int off, bool designated) {
    readStructInitSub(inits, tp, off, designated);
    sortInits(inits);
}

static void readArrayInitSub(std::vector<Node*> *inits,
        Type *tp, int off, bool designated) {
    bool has_brace = maybeReadBrace();
    bool flexible = (tp->len <= 0);
    int elemsize = tp->ptr->size;
    int i;
    for (i = 0; flexible || i < tp->len; i++) {
        Token *tk = get();
        if (isKeyword(tk, '}')) {
            if (!has_brace) ungetToken(tk);
            goto finish;
        }
        if ((isKeyword(tk, '.') || isKeyword(tk, '[')) &&
                !has_brace && !designated) {
            ungetToken(tk);
            return;
        }
        if (isKeyword(tk, '[')) {
            Token *tk = peek();
            int idx = readIntexpr();
            if (idx < 0 || (!flexible && tp->len <= idx))
                errort(tk, "array designator exceeds array bounds: %d",
                        idx);
            i = idx;
            expect(']');
            designated = 1;
        }
        else ungetToken(tk);
        readInitElem(inits, tp->ptr, off + elemsize * i, designated);
        maybeSkipComma();
        designated = 0;
    }
    if (has_brace) skipToBrace();
finish:
    if (tp->len < 0) {
        tp->len = i;
        tp->size = elemsize * i;
    }
}

static void readArrayInitializer(std::vector<Node*> *inits,
        Type *tp, int off, bool designated) {
    readArrayInitSub(inits, tp, off, designated);
    sortInits(inits);
}

static void readInitList(std::vector<Node*> *inits,
        Type *tp, int off, bool designated) {
    Token *tk = get();
    if (isString(tp)) {
        if (tk->kind == TSTRING) {
            assignString(inits, tp, tk->sval, off);
            return;
        }
        if (isKeyword(tk, '{') && peek()->kind == TSTRING) {
            tk = get();
            assignString(inits, tp, tk->sval, off);
            expect('}');
            return;
        }
    }
    ungetToken(tk);
    if (tp->kind == KIND_ARRAY)
        readArrayInitializer(inits, tp, off, designated);
    else if (tp->kind == KIND_STRUCT)
        readStructInit(inits, tp, off, designated);
    else {
        Type *arraytype = makeArraytype(tp, 1);
        readArrayInitializer(inits, arraytype, off, designated);
    }
}

static nvec* readDeclInit(Type *tp) {
    nvec *r = new std::vector<Node*>;
    if (isKeyword(peek(), '{') || isString(tp))
        readInitList(r, tp, 0, 0);
    else {
        Node *init = conv(readAssignmentExpr());
        if (isArithtype(init->tp) && init->tp->kind != tp->kind)
            init = astConv(tp, init);
        r->push_back(astInit(init, tp, 0));
    }
    return r;
}

/*
 * declarator
 */

static Type* readFuncParam(char **name, bool optional) {
    int sclass = 0;
    Type *basety = type_int;
    if (isType(peek())) basety = readDeclSpec(&sclass);
    else if (optional)
        errort(peek(), "type expected, but got %s", tk2s(peek()));
    Type *tp = readDeclarator(name, basety, NULL, optional ?
            DECL_PARAM_TYPEONLY : DECL_PARAM);
    if (tp->kind == KIND_ARRAY)
        return makePtrType(tp->ptr);
    if (tp->kind == KIND_FUNC)
        return makePtrType(tp);
    return tp;
}

static void readDeclaratorParams(std::vector<Type*> *types,
        std::vector<Node*> *vars, bool *ellipsis) {
    bool typeonly = !vars;
    *ellipsis = 0;
    while (1) {
        Token *tk = peek();
        if (nextToken(KELLIPSIS)) {
            if (types->size() == 0)
                errort(tk, "at least one parameter is required before \"...\"");
            expect(')');
            *ellipsis = 1;
            return;
        }
        char *name;
        Type *tp = readFuncParam(&name, typeonly);
        ensureNotvoid(tp);
        types->push_back(tp);
        if (!typeonly)
            vars->push_back(astLvar(tp, name));
        tk = get();
        if (isKeyword(tk, ')')) return;
        if (!isKeyword(tk, ','))
            errort(tk, "comma expected, but got %s", tk2s(tk));
    }
}

static void readDeclaratorParamsOldstyle(std::vector<Node*> *vars) {
    while (1) {
        Token *tk = get();
        if (tk->kind != TIDENT)
            errort(tk, "identifier expected, but got %s", tk2s(tk));
        vars->push_back(astLvar(type_int, tk->sval));
        if (nextToken(')')) return;
        if (!nextToken(','))
            errort(tk, "comma expected, but got %s", tk2s(get()));
    }
}

static Type* readFuncParamList(std::vector<Node*> *paramvars,
        Type *rettype) {
    Token *tk = get();
    if (isKeyword(tk, KVOID) && nextToken(')'))
        return makeFunctype(rettype, new tvec, 0, 0);
    if (isKeyword(tk, ')'))
        return makeFunctype(rettype, new tvec, 1, 1);
    ungetToken(tk);
    Token *tk2 = peek();
    if (nextToken(KELLIPSIS))
        errort(tk2, "at least one parameter is required before \"...\"");
    if (isType(peek())) {
        bool ellipsis;
        std::vector<Type*> *paramtypes = new std::vector<Type*>;
        readDeclaratorParams(paramtypes, paramvars, &ellipsis);
        return makeFunctype(rettype, paramtypes, ellipsis, 0);
    }
    if (!paramvars)
        errort(tk, "invalid function definition");
    readDeclaratorParamsOldstyle(paramvars);
}

static Type* readDeclaratorArray(Type *basety) {
    int len = -1;
    if (!nextToken(']')) {
        len = readIntexpr();
        expect(']');
    }
    Token *tk = peek();
    Type *tp = readDeclaratorTail(basety, NULL);
    if (tp->kind == KIND_FUNC)
        errort(tk, "array of functions");
    return makeArraytype(tp, len);
}

static Type* readDeclaratorFunc(Type *basety,
        std::vector<Node*> *param) {
    if (basety->kind == KIND_FUNC)
        error("function returning a function");
    if (basety->kind == KIND_ARRAY)
        error("function returning a array");
    return readFuncParamList(param, basety);
}

static Type* readDeclaratorTail(Type *basety,
        std::vector<Node*> *params) {
    if (nextToken('['))
        return readDeclaratorArray(basety);
    if (nextToken('('))
        return readDeclaratorFunc(basety, params);
    return basety;
}

static void skipTypeQualifiers() {
    while (nextToken(KCONST) || nextToken(KVOLATILE) ||
            nextToken(KRESTRICT));
}

/*
 * declarators
 */

static Type* readDeclarator(char **rname, Type *basety,
        std::vector<Node*> *params, int ctx) {
    if (nextToken('(')) {
        if (isType(peek()))
            return readDeclaratorFunc(basety, params);
        Type *stub = makeStubType();
        Type *tp = readDeclarator(rname, stub, params, ctx);
        expect(')');
        *stub = *readDeclarator(rname, stub, params, ctx);
        return tp;
    }
    if (nextToken('*')) {
        skipTypeQualifiers();
        return readDeclarator(rname, makePtrType(basety), params, ctx);
    }
    Token *tk = get();
    if (tk->kind == TIDENT) {
        if (ctx == DECL_CAST)
            errort(tk, "identifier is not expected, but got %s", tk2s(tk));
        *rname = tk->sval;
        return readDeclaratorTail(basety, params);
    }
    if (ctx == DECL_BODY || ctx == DECL_PARAM)
        errort(tk, "identifier, ( or * are expected, but got %s",
                tk2s(tk));
    ungetToken(tk);
    return readDeclaratorTail(basety, params);
}

static Type* readAbstractDecl(Type *basety) {
    return readDeclarator(NULL, basety, NULL, DECL_CAST);
}

/*
 * typeof()
 */

static Type* readTypeof() {
    expect('(');
    Type *r = isType(peek()) ? readCastType() : readCommaExpr()->tp;
    expect(')');
    return r;
}

/*
 * declaration specifier
 */

static bool isPowerofTwo(int x) {
    return (x <= 0) ? 0 : !(x & (x-1));
}

static int readAlignas() {
    expect('(');
    int r = isType(peek()) ? readCastType()->align : readIntexpr();
    expect(')');
    return r;
}

static Type* readDeclSpec(int *rsclass) {
    int sclass = 0;
    Token *tk = peek();
    if (!isType(tk))
        errort(tk, "type name expected, but got %s", tk2s(tk));
    Type *usertype = NULL;
    enum {
        kvoid = 1, kbool, kchar, kint, kfloat, kdouble
    } kind = 0;
    enum {
        kshort = 1, klong, kllong
    } sz = 0;
    enum {
        ksigned = 1, kunsigned
    } sig = 0;
    int al = -1;
    while (1) {
        tk = get();
        if (tk->kind == EOF)
            error("premature end input");
        if (kind == 0 && tk->kind == TIDENT && !usertype) {
            Type *def = getTypedef(tk->sval);
            if (def) {
                if (usertype) goto err;
                usertype = def;
                goto errcheck;
            }
        }
        if (tk->kind != TKEYWORD) {
            ungetToken(tk);
            break;
        }
        switch (tk->id) {
            case KTYPEDEF:
                if (sclass) goto err; 
                sclass = S_TYPEDEF;
                break;
            case KEXTERN:
                if (sclass) goto err;
                sclass = S_EXTERN;
                break;
            case KSTATIC:
                if (sclass) goto err;
                sclass = S_STATIC;
                break;
            case KAUTO:
                if (sclass) goto err;
                sclass = S_AUTO;
                break;
            case KREGISTER:
                if (sclass) goto err;
                sclass = S_REGISTER;
                break;
            case KCONST: break;
            case KVOLATILE: break;
            case KINLINE: break;
            case KNORETURN: break;
            case KVOID: if (kind) goto err;kind = kvoid;break;
            case KBOOL: if (kind) goto err;kind = kbool;break;
            case KCHAR: if (kind) goto err;kind = kchar;break;
            case KINT: if (kind) goto err;kind = kint;break;
            case KFLOAT: if (kind) goto err;kind = kfloat;break;
            case KDOUBLE: if (kind) goto err;kind = kdouble;break;
            case KSIGNED: if (sig) goto err;sig = ksigned;break;
            case KUNSIGNED: if (sig) goto err;sig = kunsigned;break;
            case KSHORT: if (sz) goto err;sz = kshort;break;
            case KSTRUCT: if (usertype) goto err;
                usertype = readStructDef();
                break;
            case KUNION: if (usertype) goto err;
                usertype = readUnionDef();
                break;
            case KENUM: if (usertype) goto err;
                usertype = readEnumDef();
                break;
            case KALIGNAS: {
                int val = readAlignas();
                if (val < 0)
                    errort(tk, "negative alignment: %d", val);
                if (val == 0) break;
                if (al == -1 || val < al) al = val;
                break;
            }
            case KLONG: {
                if (sz == 0) sz = klong;
                else if (sz == klong) sz = kllong;
                else goto err;
                break;
            }
            case KTYPEOF: {
                if (usertype) goto err;
                usertype = readTypeof();
                break;
            }
            default:
                ungetToken(tk);
                goto done;
        }
	errcheck:
        if (kind == kbool && (sz && sig)) goto err;
        if (sz == kshort && (kind && kind != kint)) goto err;
        if (sz == klong && (kind && kind != kint && kint != kdouble))
            goto err;
        if (sig && (kind == kvoid || kind == kfloat || kind == kdouble))
            goto err;
        if (usertype && (kind || sz || sig)) goto err;
    }
done:
    if (rsclass) *rsclass = sclass;
    if (usertype) return usertype;
    if (al != -1 && !isPowerofTwo(al))
        errort(tk, "alignment must be power of 2, but got %d", al);
    Type *tp;
    switch (kind) {
        case kvoid: tp = type_void; goto end;
        case kbool: tp = makeNumtype(KIND_BOOL, 0); goto end;
        case kchar: tp = makeNumtype(KIND_CHAR, sig == kunsigned);
            goto end;
        case kfloat: tp = makeNumtype(KIND_FLOAT, 0); goto end;
        case kdouble: tp = makeNumtype(sz == klong ?
            KIND_LDOUBLE : KIND_DOUBLE, 0);
            goto end;
        default: break;
    }
    switch (sz) {
        case kshort: tp = makeNumtype(KIND_SHORT, sig == kunsigned);
            goto end;
        case klong: tp = makeNumtype(KIND_LONG, sig == kunsigned);
            goto end;
        case kllong: tp = makeNumtype(KIND_LLONG, sig == kunsigned);
            goto end;
        default: tp = makeNumtype(KIND_INT, sig == kunsigned);
            goto end;
    }
    error("internal error: kind: %d, size: %d", kind, sz);
end:
    if (al != -1) tp->align = al;
    return tp;
err:
    errort(tk, "type mismatch: %s", tk2s(tk));
}

/*
 * declaration
 */

static void readStaticLocalVar(Type *tp, char *name) {
    Node *var = astStaticLvar(tp, name);
    nvec *init = NULL;
    if (nextToken('=')) {
        auto orig = localenv;
        localenv = NULL;
        init = readDeclInit(tp);
        localenv = orig;
    }
    toplevels->push_back(astDecl(var, init));
}

static Type* readDeclSpecOpt(int *sclass) {
    if (isType(peek()))
        return readDeclSpec(sclass);
    warnt(peek(), "type specifier missing, assuming int");
    return type_int;
}

static void readDecl(nvec *block, bool isglobal) {
    int sclass = 0;
    Type *basetype = readDeclSpecOpt(&sclass);
    if (nextToken(';')) return;
    while (1) {
        char *name = NULL;
        Type *tp = readDeclarator(&name, copyIncompletetype(basetype),
                NULL, DECL_BODY);
        tp->isstatic = (sclass == S_STATIC);
        if (sclass == S_TYPEDEF) astTypedef(tp, name);
        else if (tp->isstatic && !isglobal) {
            ensureNotvoid(tp);
            readStaticLocalVar(tp, name);
        }
        else {
            ensureNotvoid(tp);
            Node *var = (isglobal ? astGvar: astLvar)(tp, name);
            if (nextToken('='))
                block->push_back(astDecl(var, readDeclInit(tp)));
            else if (sclass != S_EXTERN && tp->kind != KIND_FUNC)
                block->push_back(astDecl(var, NULL));
        }
        if (nextToken(';')) return;
        if (!nextToken(','))
            errort(peek(), "';' or ',' are expected, but got %s",
                    tk2s(peek()));
    }
}

/*
 * parameter types
 */

static nvec* readOldstyleParamArgs() {
    auto orig = localenv;
    localenv = NULL;
    auto r = new nvec;
    while (1) {
        if (isKeyword(peek(), '{')) break;
        if (!isType(peek()))
            errort(peek(), "k&r-style declarator expected, but got %s",
                    tk2s(peek()));
        readDecl(r, 0);
    }
    localenv = orig;
    return r;
}

static void updateOldstyleParamType(std::vector<Node*> *params,
        std::vector<Node*> *vars) {
    for (unsigned i = 0; i < vars->size(); i++) {
        Node *decl = (*vars)[i];
        assert(decl->kind == AST_DECL);
        Node *var = decl->declvar;
        assert(var->kind == AST_LVAR);
        for (unsigned j = 0; j < params->size(); j++) {
            auto param = (*params)[j];
            assert(param->kind == AST_LVAR);
            if (strcmp(param->varname, var->varname)) continue;
            param->tp = var->tp;
            goto found;
        }
        error("missing parameter: %s", var->varname);
found:;
    }
}

static void readOldstyleParamType(std::vector<Node*> *params) {
    auto vars = readOldstyleParamArgs();
    updateOldstyleParamType(params, vars);
}

static std::vector<Type*>* paramTypes(std::vector<Node*> *params) {
    auto r = new std::vector<Type*>;
    for (unsigned i = 0; i < params->size(); i++) {
        Node *param = (*params)[i];
        r->push_back(param->tp);
    }
    return r;
}

/*
 * function definition
 */

static Node* readFuncBody(Type *functype, char *fname,
        std::vector<Node*> *params) {
    localenv = makeMapParent(localenv);
    localvars = new std::vector<Node*>;
    current_func_type = functype;
    Node *funcname = astString(fname, strlen(fname) + 1);
    char *s = newChar("__func__");
    mapInsert(localenv, s, funcname);
    s = newChar("__FUNCTION__");
    mapInsert(localenv, s, funcname);
    Node *body = readCompoundStmt();
    Node *r = astFunc(functype, fname, params, body, localvars);
    current_func_type = NULL;
    localenv = NULL;
    localvars = NULL;
    return r;
}

static void skipParentheses(std::vector<Token*> *buf) {
    while (1) {
        Token *tk = get();
        if (tk->kind == TEOF)
            error("premature end of input");
        buf->push_back(tk);
        if (isKeyword(tk, ')')) return;
        if (isKeyword(tk, '(')) skipParentheses(buf);
    }
}

static bool isFuncdef() {
    puts("isfuncdef");
    auto buf = new std::vector<Token*>;
    bool r = 0;
    while (1) {
        puts("get begin");
        Token *tk = get();
        std::cout << table[tk->kind] << std::endl;
        buf->push_back(tk);
        if (tk->kind == TEOF)
            error("premature end of input");
        if (isKeyword(tk, ';')) break;
        if (isType(tk)) continue;
        if (isKeyword(tk, '(')) {
            skipParentheses(buf);
            continue;
        }
        if (tk->kind != TIDENT) continue;
        if (!isKeyword(peek(), '(')) {
            continue;
        }
        buf->push_back(get());
        skipParentheses(buf);
        r = (isKeyword(peek(), '{') || isType(peek()));
        break;
    }
    while (buf->size() > 0) {
        ungetToken((*buf)[buf->size()-1]);
        assert(buf->size() > 0);
        buf->pop_back();
        //ungetToken(buf->back());
    } 
    return r;
}

static void backfillLabels() {
    for (unsigned i = 0; i < gotos->size(); i++) {
        Node *src = (*gotos)[i];
        char *label = src->label;
        Node *dst = labels->get(label);
        if (!dst)
            error("stray %s: %s", src->kind == AST_GOTO ?
                    "gogo" : "unary &&", label);
        if (dst->newlabel)
            src->newlabel = dst->newlabel;
        else
            src->newlabel = dst->newlabel = makeLabel();
    }
}

static Node* readFuncdef() {
    int sclass = 0;
    Type *basetype = readDeclSpecOpt(&sclass);
    localenv = makeMapParent(globalenv);
    gotos = new std::vector<Node*>;
    labels = new nmap;
    char *name;
    auto params = new std::vector<Node*>;
    Type *functype = readDeclarator(&name, basetype, params, DECL_BODY);
    if (functype->oldstyle) {
        if (params->size() == 0) functype->hasva = 0;
        readOldstyleParamType(params);
        functype->params = paramTypes(params);
    }
    functype->isstatic = (sclass == S_STATIC);
    astGvar(functype, name);
    expect('{');
    auto r = readFuncBody(functype, name, params);
    backfillLabels();
    localenv = NULL;
    return r;
}

/*
 * if
 */

static Node* readBoolExpr() {
    Node *cond = readExpr();
    return isFlotype(cond->tp) ? astConv(type_bool, cond) : cond;
}

static Node* readIfStmt() {
    expect('(');
    Node *cond = readBoolExpr();
    expect(')');
    Node *then = readStmt();
    if (!nextToken(KELSE))
        return astIf(cond, then, NULL);
    Node *els = readStmt();
    return astIf(cond, then, els);
}

/*
 * for
 */

static Node* readOptDeclOrStmt() {
    if (nextToken(';')) return NULL;
    auto alist = new std::vector<Node*>;
    readDeclOrStmt(alist);
    return astCompoundstmt(alist);
}

#define SET_JUMP_LABELS(cont, brk)  \
    char *ocontinue = lcontinue;    \
    char *obreak = lbreak;          \
    lcontinue = cont;               \
    lbreak = brk;

#define RESTORE_JUMP_LABELS()       \
    lcontinue = ocontinue;          \
    lbreak = obreak;

static Node* readForStmt() {
    expect('(');
    char *beg = makeLabel();
    char *mid = makeLabel();
    char *end = makeLabel();
    auto orig = localenv;
    localenv = makeMapParent(localenv);
    auto init = readOptDeclOrStmt();
    auto cond = readExprOpt();
    if (cond && isFlotype(cond->tp))
        cond = astConv(type_bool, cond);
    expect(';');
    auto *step = readExprOpt();
    expect(')');
    SET_JUMP_LABELS(mid, end);
    auto body = readStmt();
    RESTORE_JUMP_LABELS();
    localenv = orig;

    auto v = new std::vector<Node*>;
    if (init) v->push_back(init);
    v->push_back(astDest(beg));
    if (cond) v->push_back(astIf(cond, NULL, astJump(end)));
    if (body) v->push_back(body);
    v->push_back(astDest(mid));
    if (step) v->push_back(step);
    v->push_back(astJump(beg));
    v->push_back(astDest(end));
    return astCompoundstmt(v);
}

/*
 * while
 */

static Node* readWhileStmt() {
    expect('(');
    auto cond = readBoolExpr();
    expect(')');
    auto beg = makeLabel();
    auto end = makeLabel();
    SET_JUMP_LABELS(beg, end);
    auto body = readStmt();
    RESTORE_JUMP_LABELS();
    auto v = new std::vector<Node*>;
    v->push_back(astDest(beg));
    v->push_back(astIf(cond, body, astJump(end)));
    v->push_back(astJump(beg));
    v->push_back(astDest(end));
    return astCompoundstmt(v);
}

/*
 * do
 */

static Node* readDoStmt() {
    auto beg = makeLabel();
    auto end = makeLabel();
    SET_JUMP_LABELS(beg, end);
    auto body = readStmt();
    RESTORE_JUMP_LABELS();
    auto tk = get();
    if (!isKeyword(tk, KWHILE))
        errort(tk, "'while' is expected, but got %s", tk2s(tk));
    expect('(');
    auto cond = readBoolExpr();
    expect(')');
    expect(';');

    auto v = new std::vector<Node*>;
    v->push_back(astDest(beg));
    if (body) v->push_back(body);
    v->push_back(astIf(cond, astJump(beg), NULL));
    v->push_back(astDest(end));
    return astCompoundstmt(v);
}

/*
 * switch
 */

static Node* makeSwitchJump(Node *var, Case *c) {
    Node *cond;
    if (c->beg == c->end)
        cond = astBinop(type_int, OP_EQ, var,
                astIntType(type_int, c->beg));
    else {
        auto x = astBinop(type_int, OP_LE,
                astIntType(type_int, c->beg), var);
        auto y = astBinop(type_int, OP_LE, var,
                astIntType(type_int, c->end));
        cond = astBinop(type_int, OP_LOGAND, x, y);
    }
    return astIf(cond, astJump(c->label), NULL);
}

static void checkCaseDuplicates(std::vector<Case*> *cases) {
    unsigned len = cases->size();
    auto x = (*cases)[len-1];
    for (unsigned i = 0; i < len - 1; i++) {
        auto y = (*cases)[i];
        if (x->end < y->beg || y->end < x->beg) continue;
        if (x->beg == x->end)
            error("duplicate case value: %d", x->beg);
        error("duplicate case value: %d ... %d", x->beg, x->end);
    }
}

#define SET_SWITCH_CONTEXT(brk)         \
    auto ocases = cases;                \
    auto odefaultcase = defaultcase;    \
    auto obreak = lbreak;               \
    cases = new std::vector<Case*>;     \
    defaultcase = NULL;                 \
    lbreak = brk

#define RESTORE_SWITCH_CONTEXT()        \
    cases = ocases;                     \
    defaultcase = odefaultcase;         \
    lbreak = obreak

static Node* readSwitchStmt() {
    expect('(');
    Node *expr = conv(readExpr());
    ensureInttype(expr);
    expect(')');

    auto end = makeLabel();
    SET_SWITCH_CONTEXT(end);
    auto body = readStmt();
    auto v = new std::vector<Node*>;
    auto var = astLvar(expr->tp, makeTempname());
    v->push_back(astBinop(expr->tp, '=', var, expr));
    for (int i = 0; i < cases->size(); i++)
        v->push_back(makeSwitchJump(var, (*cases)[i]));
    v->push_back(astJump(defaultcase ? defaultcase : end));
    if (body) v->push_back(body);
    v->push_back(astDest(end));
    RESTORE_SWITCH_CONTEXT();
    return astCompoundstmt(v);
}

static Node* readLabelTail(Node *label) {
    auto stmt = readStmt();
    auto v = new std::vector<Node*>;
    v->push_back(label);
    if (stmt) v->push_back(stmt);
    return astCompoundstmt(v);
}

static Node* readCaseLabel(Token *tk) {
    if (!cases)
        errort(tk, "stray case label");
    auto label = makeLabel();
    int beg = readIntexpr();
    if (nextToken(KELLIPSIS)) {
        int end = readIntexpr();
        expect(':');
        if (beg > end)
            errort(tk, "case region is not correct order: %d ... %d",
                    beg, end);
        cases->push_back(makeCase(beg, end, label));
    }
    else {
        expect(':');
        cases->push_back(makeCase(beg, beg, label));
    }
    checkCaseDuplicates(cases);
    return readLabelTail(astDest(label));
}

static Node* readDefaultLabel(Token *tk) {
    expect(':');
    if (defaultcase)
        errort(tk, "duplicate default");
    defaultcase = makeLabel();
    return readLabelTail(astDest(defaultcase));
}

/*
 * jump statements
 */

static Node* readBreakStmt(Token *tk) {
    expect(';');
    if (!lbreak)
        errort(tk, "stray break statement");
    return astJump(lbreak);
}

static Node* readContinueStmt(Token *tk) {
    expect(';');
    if (!lcontinue)
        errort(tk, "stray continue statement");
    return astJump(lcontinue);
}

static Node* readReturnStmt() {
    auto retval = readExprOpt();
    expect(';');
    if (retval)
        return astReturn(astConv(current_func_type->rettype, retval));
    return astReturn(NULL);
}

static Node* readGotoStmt() {
    auto tk = get();
    if (!tk || tk->kind != TIDENT)
        errort(tk, "identifier expected, but got %s", tk2s(tk));
    expect(';');
    auto r = astGoto(tk->sval);
    gotos->push_back(r);
    return r;
}

static Node* readLabel(Token *tk) {
    auto label = tk->sval;
    if (labels->get(label))
        errort(tk, "duplicate label: %s", tk2s(tk));
    auto r = astLabel(label);
    mapInsert(labels, label, r);
    return readLabelTail(r);
}

/*
 * statement
 */

static Node* readStmt() {
    auto tk = get();
    if (tk->kind == TKEYWORD) {
        switch (tk->id) {
            case '{':       return readCompoundStmt();
            case KIF:       return readIfStmt();
            case KFOR:      return readForStmt();
            case KWHILE:    return readWhileStmt();
            case KDO:       return readDoStmt();
            case KRETURN:   return readReturnStmt();
            case KSWITCH:   return readSwitchStmt();
            case KCASE:     return readCaseLabel(tk);
            case KDEFAULT:  return readDefaultLabel(tk);
            case KBREAK:    return readBreakStmt(tk);
            case KCONTINUE: return readContinueStmt(tk);
            case KGOTO:     return readGotoStmt();
        }
    }
    if (tk->kind == TIDENT && nextToken(':'))
        return readLabel(tk);
    ungetToken(tk);
    auto r = readExprOpt();
    expect(';');
    return r;
}

static Node* readCompoundStmt() {
    auto orig = localenv;
    localenv = makeMapParent(localenv);
    auto list = new std::vector<Node*>;
    while (1) {
        if (nextToken('}')) break;
        readDeclOrStmt(list);
    }
    localenv = orig;
    return astCompoundstmt(list);
}

static void readDeclOrStmt(std::vector<Node*> *list) {
    auto tk = peek();
    if (tk->kind == TEOF)
        error("premature end of input");
    markLocation();
    if (isType(tk)) readDecl(list, 0);
    else {
        auto stmt = readStmt();
        if (stmt) list->push_back(stmt);
    }
}

/*
 * compliation unit
  */

std::vector<Node*>* readToplevels() {
    toplevels = new std::vector<Node*>;
    while (1) {
        if (peek()->kind == TEOF) return toplevels;
        if (isFuncdef()) toplevels->push_back(readFuncdef());
        else readDecl(toplevels, 1);
    }
}

static void defineBuiltin(char *name, Type *rettype,
        tvec *paramtypes) {
    astGvar(makeFunctype(rettype, paramtypes, 1, 0), name);
}

void parseInit() {
#define op(id, str) mapInsert(keywords, str, (void*)id);
#define keyword(id, str, _) mapInsert(keywords, str, (void*)id);
#include "keyword.inc"
#undef keyword
#undef op
    std::vector<Type*> *voidptr = new std::vector<Type*>;
    voidptr->push_back(makePtrType(type_void));
    std::vector<Type*> *two_voidptrs = new std::vector<Type*>;
    two_voidptrs->push_back(makePtrType(type_void));
    two_voidptrs->push_back(makePtrType(type_void));
    puts("parseInit finished");
}
