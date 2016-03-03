#include "cic.h"

static char* decorateInt(const char *name, Type *tp) {
    const char *u = tp->usig ? "u" : "";
    if (tp->bitsize > 0)
        return format("%s%s:%d:%d", u, name, tp->bitoff,
                tp->bitoff + tp->bitsize);
    return format("%s%s", u, name);
}

static const char* doTp2s(Dict<Type> *dc, Type *tp) {
    if (!tp) return "(nil)";
    switch (tp->kind) {
        case KIND_VOID: return "void";
        case KIND_BOOL: return "bool";
        case KIND_CHAR: return decorateInt("char", tp);
        case KIND_SHORT: return decorateInt("short", tp);
        case KIND_INT: return decorateInt("int", tp);
        case KIND_LONG: return decorateInt("long", tp);
        case KIND_LLONG: return decorateInt("llong", tp);
        case KIND_FLOAT: return "float";
        case KIND_DOUBLE: return "double";
        case KIND_LDOUBLE: return "long double";
        case KIND_PTR:
            return format("*%s", doTp2s(dc, tp->ptr));
        case KIND_ARRAY:
            return format("[%d]%s", tp->len, doTp2s(dc, tp->ptr));
        case KIND_STRUCT: {
            const char *kind = tp->isstruct ? "struct" : "union";
            if (dictGet(dc, format("%p", tp)))
                return format("(%s)", kind);
            dictPut(dc, format("%p", tp), (Type*)1);
            if (tp->fields) {
                Buffer *b = makeBuffer();
                bufPrintf(b, "(%s", kind);
                std::vector<char*> *keys = tp->fields->key;
                //std::vector<char*> *keys = dictKeys(tp->fields);
                for (unsigned i = 0; i < keys->size(); i++) {
                    char *key = (*keys)[i];
                    Type *fieldtype = dictGet(tp->fields, key);
                    bufPrintf(b, " (%s)", doTp2s(dc, fieldtype));
                }
                bufPrintf(b, ")");
                return bufBody(b);
            }
        }
        case KIND_FUNC: {
            Buffer *b = makeBuffer();
            bufPrintf(b, "(");
            if (tp->params) {
                for (unsigned i = 0; i < tp->params->size(); i++) {
                    if (i > 0) bufPrintf(b, ",");
                    Type *t = (*tp->params)[i];
                    bufPrintf(b, "%s", doTp2s(dc, t));
                }
            }
            bufPrintf(b, ")=>%s", doTp2s(dc, tp->rettype));
            return bufBody(b);
        }
        default:
            return format("(Unknwon type: %d)", tp->kind);
    }
}

char* tp2s(Type *tp) {
    return doTp2s(makeDict<Type>(), tp);
}

static void uopToString(Buffer *b, char *op, Node *node) {
    bufPrintf(b, "(%s %s)", op, node2s(node->operand));
}

static void binopToString(Buffer *b, char *op, Node *node) {
    bufPrintf(b, "(%s %s %s)", op, node2s(node->left),
            node2s(node->right));
}

static void a2sDeclinit(Buffer *b, std::vector<Node*> *initlist) {
    for (unsigned i = 0; i < initlist->size(); i++) {
        if (i > 0) bufPrintf(b, " ");
        Node *init = (*initlist)[i];
        bufPrintf(b, "%s", node2s(init));
    }
}

static void doNode2s(Buffer *b, Node *node) {
    if (!node) {
        bufPrintf(b, "(nil)");
        return;
    }
    switch (node->kind) {
        case AST_LITERAL:
            switch (node->tp->kind) {
                case KIND_CHAR:
                    if (node->ival == '\n') bufPrintf(b, "'\n'");
                    else if (node->ival == '\\') bufPrintf(b, "'\\\\'");
                    else if (node->ival == '\0') bufPrintf(b, "'\\0'");
                    else bufPrintf(b, "'%c'", node->ival);
                    break;
                case KIND_INT:
                    bufPrintf(b, "%d", node->ival);
                    break;
                case KIND_LONG:
                    bufPrintf(b, "%ldL", node->ival);
                    break;
                case KIND_LLONG:
                    bufPrintf(b, "%lldL", node->ival);
                    break;
                case KIND_FLOAT:
                case KIND_DOUBLE:
                case KIND_LDOUBLE:
                    bufPrintf(b, "%f", node->fval);
                    break;
                case KIND_ARRAY:
                    bufPrintf(b, "\"%s\"", quoteCstring(node->sval));
                    break;
                default:
                    error("internal error");
            }
            break;
        case AST_LABEL:
            bufPrintf(b, "%s:", node->label);
            break;
        case AST_LVAR:
            bufPrintf(b, "lv=%s", node->varname);
            if (node->lvarinit) {
                bufPrintf(b, "(");
                a2sDeclinit(b, node->lvarinit);
                bufPrintf(b, ")");
            }
            break;
        case AST_GVAR:
            bufPrintf(b, "gv=%s", node->varname);
            break;
        case AST_FUNCALL:
        case AST_FUNCPTR_CALL: {
            bufPrintf(b, "(%s)%s(", tp2s(node->tp)),
                node->kind == AST_FUNCALL ? node->fname : node2s(node);
            for (int i = 0; i < node->args->size(); i++) {
                if (i > 0) bufPrintf(b, ",");
                bufPrintf(b, "%s", node2s((*node->args)[i]));
            }
        }
    }
}

char* node2s(Node *node) {
    Buffer *b = makeBuffer();
    doNode2s(b, node);
    return bufBody(b);
}

char* tk2s(Token *tk) {
    if (!tk) {
        return newChar("(null)");
    } 
    switch (tk->kind) {
        case TIDENT: return tk->sval;
        case TKEYWORD:
            switch (tk->id) {
#define op(id, str) case id: return newChar(str);
#define keyword(id, str, _) case id: return newChar(str);
#include "keyword.inc"
#undef keyword
#undef op
                default: return format("%c", tk->id);
            }
        case TCHAR:
            return format("%s'%s'", "", quoteChar(tk->c));
        case TNUMBER: return tk->sval;
        case TSTRING:
            return format("%s\"%s\"", "", quoteCstring(tk->sval));
        case TEOF: return newChar("(eof)");
        case TINVALID: return format("%c", tk->c);
        case TNEWLINE: return newChar("(newline)");
        case TSPACE: return newChar("space");
    }
    error("internal error: unknown token kind: %d", tk->kind);
}
