#include "cic.h"
#include "grammar.h"
#include "ast.h"
#include "decl.h"

static int first_struct_declaration[] = { FIRST_DECLARATION, 0 };
static int ff_struct_declaration[] = { FIRST_DECLARATION, TK_RBRACE, 0 };
static int first_function[] = { FIRST_DECLARATION, TK_LBRACE, 0 }; 
static int first_external_declaration[] =
    { FIRST_DECLARATION, TK_MUL, TK_LPAREN, 0 };
static Vector typedef_names, overload_names;

int first_declaration[] = { FIRST_DECLARATION, 0 };

static AstDeclarator ParseDeclarator(int);
static AstSpecifiers ParseDeclarationSpecifiers();

static char* GetOutermostID(AstDeclarator dec) {
    if (dec->kind == NK_NameDeclarator)
        return dec->id;
    return GetOutermostID(dec->dec);
}

static int IsTypedefName(char *id) {
    Vector v = typedef_names;
    TDName tn;

    FOR_EACH_ITEM(TDName, tn, v)
        if (tn->id == id && tn->level <= level && !tn->overload)
            return 1;
    ENDFOR

    return 0;
}

static void CheckTypedefName(int sclass, char *id) {
    Vector v;
    TDName tn;

    if (id == NULL) return;

    v = typedef_names;
    if (sclass == TK_TYPEDEF) {
        FOR_EACH_ITEM(TDName, tn, v)
            if (tn->id == id) {
                if (level < tn->level)
                    tn->level = level;
                return;
            }
        ENDFOR

        ALLOC(tn);
        tn->id = id;
        tn->level = level;
        tn->overload = 0;
        InsertItem(v, tn);
    }
    else {
        FOR_EACH_ITEM(TDName, tn, v)
            if (tn->id == id && level > tn->level) {
                tn->overload = 1;
                InsertItem(overload_names, tn);
                return;
            }
        ENDFOR
    }
}

static void PreCheckTypedef(AstDeclaration decl) {
    AstNode p;
    int sclass = 0;

    if (decl->specs->stg_classes != NULL) {
        sclass = ((AstToken)decl->specs->stg_classes)->token;
    }

    p = decl->init_decs;
    while (p != NULL) {
        CheckTypedefName(sclass, GetOutermostID(((AstDeclarator)p)->dec));
        p = p->next;
    }
}

void PostCheckTypedef() {
    TDName tn;

    FOR_EACH_ITEM(TDName, tn, overload_names)
        tn->overload = 0;
    ENDFOR
    
        overload_names->len = 0;
}

static AstInitializer ParseInitializer() {
    AstInitializer init;
    AstNode *tail;

    CREATE_AST_NODE(init, Initializer);

    if (current_token == TK_LBRACE) {
        init->lbrace = 1;
        NEXT_TOKEN;
        init->initials = (AstNode)ParseInitializer();
        tail = &init->initials->next;
        while (current_token == TK_COMMA) {
            NEXT_TOKEN;
            if (current_token == TK_RBRACE)
                break;
            *tail = (AstNode)ParseInitializer();
            tail = &(*tail)->next;
        }
        Expect(TK_RBRACE);
    }
    else {
        init->lbrace = 0;
        init->expr = ParseAssignExpr();
    }

    return init;
}
