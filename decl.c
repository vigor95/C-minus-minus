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

static AstDeclarator ParseDeclarator(int kind) {
    if (current_token == TK_MUL) {
        AstPointerDeclarator ptr_dec;
        AstToken tok;
        AstNode *tail;

        CREATE_AST_NODE(ptr_dec, PointerDeclarator);
        tail = &ptr_dec->type_quals;

        NEXT_TOKEN;
        while (current_token == TK_CONST ||
                current_token == TK_VOLATILE) {
            CREATE_AST_NODE(tok, Token);
            *tail = (AstNode)tok;
            tail = &tok->next;
            NEXT_TOKEN;
        }
        ptr_dec->dec = ParseDeclarator(kind);

        return (AstDeclarator)ptr_dec;
    }

    return ParsePostfixDeclarator(kind);
}

static AstStructDeclarator ParseStructDeclarator() {
    AstStructDeclarator st_dec;

    CREATE_AST_NODE(st_dec, StructDeclarator);

    if (current_token != TK_COLON) {
        st_dec->dec = ParseDeclarator(DEC_CONCRETE);
    }
    if (current_token == TK_COLON) {
        NEXT_TOKEN;
        st_dec->expr = ParseConstExpr();
    }

    return st_dec;
}

static AstStructDeclaration ParseStructDeclaration() {
    AstStructDeclaration st_decl;
    AstNode *tail;

    CREATE_AST_NODE(st_decl, StructDeclaration);

    st_decl->specs = ParseDeclarationSpecifiers();
    if (st_decl->specs->stg_classes != NULL) {
        Error(&st_decl->cd, "Struct/union member should not have storage class");
        st_decl->specs->stg_classes = NULL;
    }
    if (st_decl->specs->type_quals == NULL &&
            st_decl->specs->type_specs == NULL) {
        Error(&st_decl->cd, "Expect type specifier or qualifier");
    }

    if (current_token == TK_SEMICOLON) {
        NEXT_TOKEN;
        return st_decl;
    }

    st_decl->st_decs = (AstNode)ParseStructDeclarator();
    tail = &st_decl->st_decs->next;
    while (current_token == TK_COMMA) {
        NEXT_TOKEN;
        *tail = (AstNode)ParseStructDeclarator();
        tail = &(*tail)->next;
    }
    Expect(TK_SEMICOLON);

    return st_decl;
}

static AstStructSpecifier ParseStructOrUnionSpecifier() {
    AstStructSpecifier st_spec;
    AstNode *tail;

    CREATE_AST_NODE(st_spec, StructSpecifier);
    if (current_token == TK_UNION) {
        st_spec->kind = NK_UnionSpecifier;
    }

    NEXT_TOKEN;
    switch (current_token) {
        case TK_ID:
            st_spec->id = token_value.p;
            NEXT_TOKEN;
            if (current_token == TK_LBRACE)
                goto lbrace;

            return st_spec;

        case TK_LBRACE:
lbrace:
            NEXT_TOKEN;
            if (current_token == TK_RBRACE) {
                NEXT_TOKEN;
                return st_spec;
            }

            tail = &st_spec->st_decls;
            while (CurrentTokenIn(first_struct_declaration)) {
                *tail = (AstNode)ParseStructDeclaration();
                tail = &(*tail)->next;
                SkipTo(ff_struct_declaration,
                        "the start of struct declaration or }");
            }
            Expect(TK_RBRACE);
            return st_spec;
        default:
            Error(&token_coord,
                    "Expect identifier or { after struct/union");
            return st_spec;
    }
}

static AstEnumerator ParseEnumerator() {
    AstEnumerator enumer;

    CREATE_AST_NODE(enumer, Enumerator);

    if (current_token != TK_ID) {
        Error(&token_coord, "The enumeration constant must be identifier");
        return enumer;
    }

    enumer->id = token_value.p;
    NEXT_TOKEN;
    if (current_token == TK_ASSIGN) {
        NEXT_TOKEN;
        enumer->expr = ParseConstExpr();
    }

    return enumer;
}

static AstEnumSpecifier ParseEnumSpecifier() {
    AstEnumSpecifier enum_spec;
    AstNode *tail;

    CREATE_AST_NODE(enum_spec, EnumSpecifier);

    NEXT_TOKEN;
    if (current_token == TK_ID) {
        enum_spec->id = token_value.p;
        NEXT_TOKEN;
        if (current_token == TK_LBRACE)
            goto enumerator_list;
    }
    else if (current_token == TK_LBRACE) {
enumerator_list:
        NEXT_TOKEN;
        if (current_token == TK_RBRACE)
            return enum_spec;

        enum_spec->enumers = (AstNode)ParseEnumerator();
        tail = &enum_spec->enumers->next;
        while (current_token == TK_COMMA) {
            NEXT_TOKEN;
            if (current_token == TK_RBRACE)
                break;
            *tail = (AstNode)ParseEnumerator();
            tail = &(*tail)->next;
        }
        Expect(TK_RBRACE);
    }
    else {
        Error(&token_coord, "Expect identifier or { after enum");
    }

    return enum_spec;
}

static AstSpecifiers ParseDeclarationSpecifiers() {
    AstSpecifiers specs;
    AstToken tok;
    AstNode *sc_tail, *tq_tail, *ts_tail;
    int see_ty = 0;

    CREATE_AST_NODE(specs, Specifiers);
    sc_tail = &specs->stg_classes;
    tq_tail = &specs->type_quals;
    ts_tail = &specs->type_specs;

next_specifier:
    switch (current_token) {
        case TK_AUTO: case TK_REGISTER:
        case TK_STATIC: case TK_EXTERN: case TK_TYPEDEF:
            CREATE_AST_NODE(tok, Token);
            tok->token = current_token;
            *sc_tail = &tok->next;
            NEXT_TOKEN;
            break;
        case TK_CONST: case TK_VOLATILE:
            CREATE_AST_NODE(tok, Token);
            tok->token = current_token;
            *tq_tail = (AstNode)tok;
            tq_tail = &tok->next;
            NEXT_TOKEN;
            break;
        case TK_VOID: case TK_CHAR: case TK_SHORT: case TK_INT:
        case TK_INT64: case TK_LONG: case TK_FLOAT: case TK_DOUBLE:
        case TK_SIGNED: case TK_UNSIGNED:
            CREATE_AST_NODE(tok, Token);
            tok->token = current_token;
            *ts_tail = (AstNode)tok;
            ts_tail = &tok->next;
            see_ty = 1;
            NEXT_TOKEN;
            break;
        case TK_ID:
            if (!see_ty && IsTypedefName(token_value.p)) {
                AstTypedefName tname;
                CREATE_AST_NODE(tname, TypedefName);
                tname->id = token_value.p;
                *ts_tail = (AstNode)tname;
                ts_tail = &tname->next;
                NEXT_TOKEN;
                see_ty = 1;
                break;
            }
            return specs;
        case TK_STRUCT: case TK_UNION:
            *ts_tail = (AstNode)ParseStructOrUnionSpecifier();
            ts_tail = &(*ts_tail)->next;
            see_ty = 1;
            break;
        case TK_ENUM:
            *ts_tail = (AstNode)ParseEnumSpecifier();
            ts_tail = &(*ts_tail)->next;
            see_ty = 1;
            break;
        default:
            return specs;
    }
    goto next_specifier;
}

int IsTypeName(int tok) {
    return tok == TK_ID ? IsTypedefName(token_value.p) :
        (TK_AUTO <= tok && tok <= TK_VOID);
}

AstTypeName ParseTypeName() {
    AstTypeName ty_name;

    CREATE_AST_NODE(ty_name, TypeName);

    ty_name->specs = ParseDeclarationSpecifiers();
    if (ty_name->specs->stg_classes != NULL) {
        Error(&ty_name->cd, "Type name should not have storage class");
        ty_name->specs->stg_classes = NULL;
    }
    ty_name->dec = ParseDeclarator(DEC_ABSTRACT);

    return ty_name;
}

static AstDeclaration ParseCommonHeader() {
    AstDeclaration decl;
    AstNode *tail;

    CREATE_AST_NODE(decl, Declaration);

    decl->specs = ParseDeclarationSpecifiers();
    if (current_token != TK_SEMICOLON) {
        decl->init_decs = (AstNode)ParseInitDeclarator();
        tail = &decl->init_decs->next;
        while (current_token == TK_COMMA) {
            NEXT_TOKEN;
            *tail = (AstNode)ParseInitDeclarator();
            tail = &(*tail)->next;
        }
    }

    return decl;
}

AstDeclaration ParseDeclaration() {
    AstDeclaration decl;

    decl = ParseCommonHeader();
    Expect(TK_SEMICOLON);
    PreCheckTypedef(decl);

    return decl;
}

static AstFunctionDeclarator
GetFunctionDeclarator(AstInitDeclarator init_dec) {
    AstDeclarator dec;

    if (init_dec == NULL || init_dec->next != NULL
            || init_dec->init != NULL) {
        return NULL;
    }

    dec = init_dec->dec;
    while (dec && dec->kind != NK_FunctionDeclarator)
        dec = dec->dec;

    if (dec == NULL || dec->dec->kind != NK_NameDeclarator)
        return NULL;

    return (AstFunctionDeclarator)dec;
}

static AstNode ParseExternalDeclaration() {
    AstDeclaration decl = NULL;
    AstInitDeclarator init_dec = NULL;
    AstFunctionDeclarator fdec;

    decl = ParseCommonHeader();
    init_dec = (AstInitDeclarator)decl->init_decs;
    if (decl->specs->stg_classes != NULL &&
            ((AstToken)decl->specs->stg_classes)->token == TK_TYPEDEF)
        goto not_func;

    fdec = GetFunctionDeclarator(init_dec);
    if (fdec != NULL) {
        AstFunction func;
        AstNode *tail;

        if (current_token == TK_SEMICOLON) {
            NEXT_TOKEN;
            if (current_token != TK_LBRACE)
                return (AstNode)decl;

            Error(&decl->cd, "Maybe you accidently add the ;");
        }
        else if (fdec->param_typelist && current_token != TK_LBRACE) {
            goto not_func;
        }

        CREATE_AST_NODE(func, Function);

        func->cd = decl->cd;
        func->specs = decl->specs;
        func->dec = init_dec->dec;
        func->fdec = fdec;

        level++;
        if (func->fdec->param_typelist) {
            AstNode p = func->fdec->param_typelist->param_decls;

            while (p) {
                CheckTypedefName(0,
                    GetOutermostID(((AstParameterDeclaration)p)->dec));
                p = p->next;
            }
        }
        tail = &func->dec;
        while (CurrentTokenIn(first_declaration)) {
            *tail = (AstNode)ParseDeclaration();
            tail = &(*tail)->next;
        }
        level++;

        func->stmt = ParseCompoundStmt();

        return (AstNode)func;
    }

not_func:
    Expect(TK_SEMICOLON);
    PreCheckTypedef(decl);

    return (AstNode)decl;
}

AstTransUnit ParseTransUnit(char *filename) {
    AstTransUnit trans_unit;
    AstNode *tail;

    ReadSourceFile(filename);

    token_coord.filename = filename;
    token_coord.line = token_coord.col = token_coord.ppline = 1;
    typedef_names = CreateVector(8);
    overload_names = CreateVector(8);

    CREATE_AST_NODE(trans_unit, TranslationUnit);
    tail = &trans_unit->ext_decls;

    NEXT_TOKEN;
    while (current_token != TK_END) {
        *tail = ParseExternalDeclaration();
        tail = &(*tail)->next;
        SkipTo(first_external_declaration,
                "The beginning of external declaration");
    }

    CloseSourceFile();

    return trans_unit;
}
