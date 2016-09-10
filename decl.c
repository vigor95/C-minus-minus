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

static AstInitDeclarator ParseInitDeclarator() {
    AstInitDeclarator init_dec;

    CREATE_AST_NODE(init_dec, InitDeclarator);

    init_dec->dec = ParseDeclarator(DEC_CONCRETE);
    if (current_token == TK_ASSIGN) {
        NEXT_TOKEN;
        init_dec ->init = ParseInitializer();
    }

    return init_dec;
}

static AstDeclarator ParseDirectDeclarator(int kind) {
    AstDeclarator dec;

    if (current_token == TK_LPAREN) {
        NEXT_TOKEN;
        dec = ParseDeclarator(kind);
        Expect(TK_RPAREN);

        return dec;
    }

    CREATE_AST_NODE(dec, NameDeclarator);

    if (current_token == TK_ID) {
        if (kind == DEC_ABSTRACT) {
            Error(&token_coord,
                "Identifier is not permitted in the abstract declarator");
        }

        dec->id = token_value.p;

        NEXT_TOKEN;
    }
    else if (kind == DEC_CONCRETE) {
        Error(&token_coord, "Expect identifier");
    }

    return dec;
}

static AstParameterDeclaration ParseParameterDeclaration() {
    AstParameterDeclaration param_decl;

    CREATE_AST_NODE(param_decl, ParameterDeclaration);

    param_decl->specs = ParseDeclarationSpecifiers();
    param_decl->dec = ParseDeclarator(DEC_ABSTRACT | DEC_CONCRETE);

    return param_decl;
}

AstParameterTypelist ParseParmeterTypeList() {
    AstParameterTypelist param_typelist;
    AstNode *tail;

    CREATE_AST_NODE(param_typelist, ParameterTypeList);

    param_typelist->param_decls = (AstNode)ParseParameterDeclaration();
    tail = &param_typelist->param_decls->next;
    while (current_token == TK_COMMA) {
        NEXT_TOKEN;
        if (current_token == TK_ELLIPSE) {
            param_typelist->ellipse = 1;
            NEXT_TOKEN;
            break;
        }
        *tail = (AstNode)ParseParameterDeclaration();
        tail = &(*tail)->next;
    }

    return param_typelist;
}

static AstDeclarator ParsePostfixDeclarator(int kind) {
    AstDeclarator dec = ParseDirectDeclarator(kind);

    while (1) {
        if (current_token == TK_LBRACKET) {
            AstArrayDeclarator arr_dec;

            CREATE_AST_NODE(arr_dec, ArrayDeclarator);
            arr_dec->dec = dec;

            NEXT_TOKEN;
            if (current_token != TK_RBRACKET) {
                arr_dec->expr = ParseConstExpr();
            }
            Expect(TK_RBRACKET);

            dec = (AstDeclarator)arr_dec;
        }
        else if (current_token == TK_LPAREN) {
            AstFunctionDeclarator func_dec;

            CREATE_AST_NODE(func_dec, FunctionDeclarator);
            func_dec->dec = dec;

            NEXT_TOKEN;
            if (IsTypeName(current_token)) {
                func_dec->param_typelist = ParseParmeterTypeList();
            }
            else {
                func_dec->ids = CreateVector(4);
                if (current_token == TK_ID) {
                    InsertItem(func_dec->ids, token_value.p);

                    NEXT_TOKEN;
                    while (current_token == TK_COMMA) {
                        NEXT_TOKEN;
                        if (current_token == TK_ID) {
                            InsertItem(func_dec->ids, token_value.p);
                        }
                        Expect(TK_ID);
                    }
                }
            }
            Expect(TK_RPAREN);
            dec = (AstDeclarator)func_dec;
        }
        else {
            return dec;
        }
    }
}
