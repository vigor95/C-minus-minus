#include "cic.h"

const int MAX_ALIGN = 16;
SourceLoc *source_loc;

struct cmp {
    bool operator()(const char *s1, const char *s2) {
        return strcmp(s1, s2) < 0;
    }
};

typedef std::map<char*, Node*, cmp> mmap;

mmap *globalenv = new mmap;
mmap *localenv = new mmap;
mmap *tags = new mmap;
mmap *labels = new mmap;

typedef std::vector<Node*> nodevec;
nodevec *toplevels;
nodevec *localvars;
nodevec *gotos;
nodevec *cases;

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
    Case() = default;
    Case(int b, int e, char *l): beg(b), end(e), label(newChar(l)) {}
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

static Type* makePtrType(Type *tp);
static Type* makeArraytype(Type *tp, int size);

void mapInsert(mmap *m, char *key, Node *val) {
    m->insert(std::pair<char*, Node*>(key, val));
}

Token* readToken() {
    return lex();
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
        Token *tmp = lex();
        bufAppend(b, tmp->sval, tmp->slen - 1);
    }
    bufWrite(b, '\0');
    tk->sval = bufBody(b);
    tk->slen = bufLen(b);
}

static Token* get() {
    Token *r = lex();
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

static mmap* env() {
    return localenv ? localenv : globalenv;
}

static Node* makeAst(Node *tmp) {
    Node *r = new Node;
    *r = *tmp;
    r->sl = source_loc;
    return r;
}

static Node* astUop(int kind, Type *tp, Node *operand) {
    return makeAst(new Node{.kind = kind, .tp = new Type(*tp),
            .operand = new Node(*operand)});
}

static Node* astBinop(Type *tp, int kind, Node *left, Node *right) {
    Node *r = makeAst(new Node{kind, tp});
    r->left = new Node(*left);
    r->right = new Node(*right);
    return r;
}

static Node* astInitType(Type* tp, long val) {
    return makeAst(new Node{AST_LITERAL, tp, .ival = val});
}

static Node* astFloattype(Type *tp, double val) {
    return makeAst(new Node{AST_LITERAL, tp, .fval = val});
}

static Node* astLvar(Type *tp, char *name) {
    Node *r = makeAst(new Node{AST_LVAR, tp, .varname = name});
    if (localenv) {
        mapInsert(localenv, name, r);
    } 
    if (localvars) localvars->push_back(r);
    return r;
}

static Node* astGvar(Type *tp, char *name) {
    Node *r = new Node{AST_GVAR, tp, .varname = newChar(name),
        .glabel = newChar(name)};
    mapInsert(globalenv, name, r);
    return r;
}

static Node* astStaticLvar(Type *tp, char *name) {
    Node *r = makeAst(new Node{AST_GVAR, new Type(*tp),
            .varname = newChar(name), .glabel = makeStaticLabel(name)});
    assert(localenv);
    mapInsert(localenv, name, r);
    return r;
}

static Node* astTypedef(Type *tp, char *name) {
    Node *r = makeAst(new Node{AST_TYPEDEF, new Type(*tp)});
    mapInsert(env(), name, r);
    return r;
}

static Node* astString(char *str, int len) {
    Type *tp = makeArraytype(type_char, len);
    return makeAst(new Node{AST_LITERAL, new Type(*tp),
            .sval = newChar(str)});
}

static Node* astFuncall(Type *ftype, char *fname) {
    return makeAst(new Node{AST_FUNCALL, new Type(*ftype->rettype),
        .fname = newChar(fname), .ftype = new Type(*ftype)});
}

static Node* astFuncdesg(Type *tp, char *fname) {
    return makeAst(new Node{AST_FUNCDESG, new Type(*tp),
            .fname = newChar(fname)});
}

static Node* astFuncptrcall(Node *fptr) {
    assert(fptr->tp->kind == KIND_PTR);
    assert(fptr->tp->ptr->kind == KIND_FUNC);
    return makeAst(new Node{AST_FUNCPTR_CALL,
            new Type(*fptr->tp->ptr->rettype),
            .fptr = new Node(*fptr)});
}

static Node* astFunc(Type *tp, char *fname, Node *body) {
    return makeAst(new Node{AST_FUNC, new Type(*tp),
            .fname = newChar(fname), .body = new Node(*body)});
}
static Node* astDecl(Node *var) {
    return makeAst(new Node{AST_DECL, .declvar = new Node(*var)});
}

static Node* astInit(Node *val, Type *totype, int off) {
    return makeAst(new Node{AST_INIT, .initval = new Node(*val),
            .initoff = off, .totype = new Type(*totype)});
}

static Node* astConv(Type *totype, Node *val) {
    return makeAst(new Node{AST_CONV, new Type(*totype),
            .operand = new Node(*val)});
}

static Node* astIf(Node *cond, Node *then, Node *els) {
    return makeAst(new Node{AST_IF, .cond = new Node(*cond),
            .then = new Node(*then), .els = new Node(*els)});
}

static Node* astTernary(Type *tp, Node *cond, Node *then, Node *els) {
    return makeAst(new Node{AST_TERNARY, new Type(*tp),
            .cond = new Node(*cond), .then = new Node(*then),
            .els = new Node(*els)});
}

static Node* astReturn(Node *retval) {
    return makeAst(new Node{AST_RETURN, .retval = new Node(*retval)});
}

static Node* astCompoundstmt() {
    return makeAst(new Node{AST_COMPOUND_STMT});
}

static Node* astStructref(Type *tp, Node *struc, char *name) {
    return makeAst(new Node{AST_STRUCT_REF, new Type(*tp),
            .struc = new Node(*struc), .field = newChar(name)});
}

static Node* astGoto(char *label) {
    return makeAst(new Node{AST_GOTO, .label = newChar(label)});
}

static Node* astJump(char *label) {
    return makeAst(new Node{AST_GOTO, .label = newChar(label),
            .newlabel = newChar(label)});
}

static Node* astComputedgoto(Node *expr) {
    return makeAst(new Node{AST_COMPUTED_GOTO,
            .operand = new Node(*expr)});
}

static Node* astLabel(char *label) {
    return makeAst(new Node{AST_LABEL, .label = newChar(label)});
}

static Node* astDest(char *label) {
    return makeAst(new Node{AST_LABEL, .label = newChar(label),
            .newlabel = newChar(label)});
}

static Node* astLabeladdr(char *label) {
    return makeAst(new Node{OP_LABEL_ADDR, makePtrType(type_void),
            .label = newChar(label)});
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

static Type* makeFunctype(Type *rettype, bool has_varargs, bool oldstyle) {
    Type *r = new Type(KIND_FUNC);
    r->rettype = new Type(*rettype);
    //r->params = paramtypes;
    r->hasva = has_varargs;
    r->oldstyle = oldstyle;
    return makeType(r);
}

static Type* makeStubtyoe() {
    return makeType(new Type(KIND_STUB));
}

bool isInittype(Type *tp) {
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
    return isInittype(tp) || isFlotype(tp); 
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

static void ensureInittype(Node *node) {
    if (!isInittype(node->tp))
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
    Node *node = env()->find(name)->second;
    return (node && node->kind == AST_TYPEDEF) ? node->tp : NULL;
}

static bool isType(Token *tk) {
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

static Type* usualArithconv(Type *t, Type *u) {
    assert(isArithtype(t));
    assert(isArithtype(u));
    if (t->kind < u->kind) {
        Type *temp = t;
        t = u;
        u = temp;
    }
    if (isFlotype(t)) return t;
    assert(isInittype(t) && t->size >= type_int->size);
    assert(isInittype(u) && u->size >= type_int->size);
    if (t->size > u->size) return t;
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
    Type *r = usualArithconv(lhs->tp, rhs->tp);
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
            std::vector<Type*> *ka = dictKeys(a->fields);
            std::vector<Type*> *kb = dictKeys(b->fields);
            if (ka->size() != kb->size()) return 0;
            for (unsigned i = 0; i < ka->size(); i++)
                if (!isSameStruct((*ka)[i], (*kb)[i]))
                    return 0;
            return 1;
        }
        defalut: return 1;
    }
}

static void ensureAssignable(Type *to, Type *from) {
    if ((isArithtype(to) || to->kind == KIND_PTR) &&
            (isArithtype(from) || from->kind == KIND_PTR))
        return;
    if (isSameStruct(to, from)) return;
    error("incompatible kind: <%s> <%s>", ty2s(to), ty2s(from));
}

static int evalStructref(Node *node, int offset) {
    if (node->kind == AST_STRUCT_REF)
        return evalStructref(node->struc, node->td->offset + offset);
    return evalIntexpr(node, NULL) + offset;
}

int evalIntexpr(Node *node, Node **addr) {
    switch (node->kind) {
        case AST_LITERAL:
            if (isInittype(node->td))
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
            if (node->operand->ty->kind == KIND_PTR)
                return evalIntexpr(node->operand, addr);
            goto error;
        case astTernary: {
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
        case OP_LOGAND: return L & R;
        case OP_LOGOR: return L || R;
#undef L
#undef R
        default:
            error("Integer expression expected, but got %s", node2s(node));
    }
}

static int readIntexpr() {
    return evalIntexpr(readConditionalExpr(), NULL);
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
        stroul(s + 2, &end, 2) : stroul(s, &end, 0);
    Type *tp = readIntSuffix(end);
    if (tp) return astInitType(tp, v);
    if (*end != '\0')
        errort(tl, "invalid character '%c': %s", *end, s);
    bool base10 = (*s - '0');
    if (base10) {
        tp = !(v & ~(long)INT_MAX) ? type_int : type_long;
        return astInitType(ty, v);
    }
    ty = !(v & ~(unsigned long)INT_MAX) ? type_int
        : !(v & ~(unsigned long)UINT_MAX) ? type_uint
        : !(v & ~(unsigned long)LONG_MAX) ? type_long
        : type_ulong;
    return astInitType(tp, v);
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
        expect('()');
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
    return astInitType(type_ulong, sz);
}
/*
 * alignof op
 */

static Node* readAlignofOperand() {
    expect('(');
    Type *tp = readCastType();
    expect(')');
    return astInitType(type_ulong, tp->align);
}

/*
 * function args
 */

static readFuncArgs(std::vector<Node*> *params) {
    std::vector<Node*> *args = new std::vector<Node*>;
    int i = 0;
    while (1) {
        if (nextToken(')')) break;
        Node *arg = conv(readAssignmentExpr());
        Type *paramtype;
        if (i < params->size()) paramtype = (*params)[i++];
        else {
            paramtype = isFlotype(arg->tp) ? type_double :
                isInittype(arg->tp) ? type_int : arg->tp;
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
    if (fp->kind = AST_ADDR && fp->operand->kind == AST_FUNCDESG) {
        Node *desg = fp->operand;
        auto args = readFuncArgs(desg->tp->params);
        return astFuncall(desg->tp, desg->fname, args);
    }
}

/*
 * expression
 */

static Node* readVarOrFunc(char *name) {
    Node *v = env()->find(name)->second;
    if (!v) {
        Token *tk = peek();
        if (!isKeyword(tk, '()'))
            errort(tk, "undefined variable: %s", name);
        Type *tp = makeFunctype(type_int, 1, 0);
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
        case OP_A_AND: return '+';
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
        case TCHAR: return astInitType(charType(), tk->c);
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
    Token *tk = get();
    if (tk->kind != TIDENT)
        errort(tk, "label name expected after &&, but got %s", tk2s(tk));
    Node *r = astLabeladdr(tk->sval);
    gotos->push_back(r);
    return r;
}

static Node* readUnaryAddr() {
    Node *operand = readCastExpr();
    if (operand->kind == astFuncdesg)
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
    if (isInittype(expr->tp))
        return binop('-', conv(astInitType(expr->tp, 0)), conv(expr));
    return binop('-', astFloattype(expr->tp, 0), expr);
}

static Node* readUnaryBitnot(Token *tk) {
    Node *expr = readCastExpr();
    expr = conv(expr);
    if (!isInittype(expr->tp))
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
            op = node->tp->usig ? OP_SHR ? OP_SAR;
        else break;
        Node *right = readAddiExpr();
        ensureInittype(node);
        ensureInittype(right);
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
    Node *node = readBitorExpr();
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
        Type *r = usualArithconv(t, u);
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
    if (!tk) return tk;
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
    if (tk->kind == TIDENT) retuen tk->sval;
    ungetToken(tk);
    return NULL;
}

static int computePadding(int offset, int align) {
    return (offset % align == 0) ? 0 : align - offset % align;
}

static void squashUnnamedStruct(Dict *dc, Type *unnamed, int offset) {
    std::vector<char*> *keys = dictKeys(unnamed->fields);
    for (unsigned i = 0; i < keys->size(); i++) {
        char *name = (*keys)[i];
        Type *t = copyType(dictGet(unnamed->fields, name));
        t->offset += offset;
        dictPut(dc, name, t);
    }
}

static int readBitsize(char *name, Type *tp) {
    if (isInittype(tp))
        error("non-integer type cannot be a bitfield: %s", tp2s(tp));
    Token *tk == peek();
    int r = readIntexpr();
    int maxsize = tp->kind == KIND_BOOL ? 1 : tp->size * 8;
    if (r < 0 || maxsize < r)
        errort(tk, "invalid bitfield size for %s: %d", tp2s(tp), r);
    if (r == 0 && name != NULL)
        errort(tk, "zero-width bitfield needs to be unnamed: %s", name);
    return r;
}

static std::vector<std::pair<char*, Type*>* > readRectypeFieldsSub() {
    std::vector<std::pair<char*, Type*>* > *r;
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
                error("flexible member may only appear as the last
                        member: %s %s", tp2s(tp));
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

static Dict* updateStructOffset(int *rsize, int *align, std::vector<
        std::pair<char*, Type*>* > *fields) {
    int off = 0, bitoff = 0;
    Dict *r = makeDict();
    for (unsigned i = 0; i < fields->size(); i++) {
        std::pair<char*, Type*> *pp = (*fields)[i];
        char *name = pp->first;
        Type *fieldtype = pp->second;
        if (name) *align = std::max(*align, fieldtype->align);
        if (name == NULL && fieldtype->kind == KIND_STRUCT) {
            finishBitfield(&off, &bitoff);
            off += computePadding(r, fieldtype->align);
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

static Dict* updateUnionOffset(int *rsize, int *align, std::vector<
        std::pair<char*, Type*>* > *fields) {
    int maxsize = 0;
    Dict *r = makeDict();
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
    continue;
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
