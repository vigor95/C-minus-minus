#ifndef __DECL_H_
#define __DECL_H_

enum {
    DEC_ABSTRACT = 0x01, DEC_CONCRETE = 0x02
};

enum {
    POINTER_TO, ARRAY_OF, FUNCTION_RETURN
};

#define AST_DECLARATOR_COMMON   \
    AST_NODE_COMMON             \
    struct astDeclarator *dec;  \
    char *id;                   \
    TypeDeriveList tydrvlist;

typedef struct tdname {
    char *id;
    int level;
    int overload;
} *TDName;

typedef struct typeDeriveList {
    int ctor;
    union {
        int len;
        int qual;
        Signature sig;
    };
    struct typeDeriveList *next;
} *TypeDeriveList;

typedef struct astSpecifiers *AstSpecifiers;

typedef struct astDeclarator {
    AST_DECLARATOR_COMMON
} *AstDeclarator;

typedef struct astInitializer {
    AST_NODE_COMMON
    int lbrace;
    union {
        AstNode initials;
        AstExpression expr;
    };
    InitData idata;
} *AstInitializer;

typedef struct astInitDeclarator {
    AST_NODE_COMMON
    AstDeclarator dec;
    AstInitializer init;
} *AstInitDeclarator;

typedef struct astParameterDeclaration {
    AST_NODE_COMMON
    AstSpecifiers specs;
    AstDeclarator dec;
} *AstParameterDeclaration;

typedef struct astParameterTypelist {
    AST_NODE_COMMON
    AstNode param_decls;
    int ellipse;
} *AstParameterTypelist;

typedef struct astFunctionDeclarator {
    AST_DECLARATOR_COMMON
    Vector ids;
    AstParameterTypelist param_typelist;
    int part_of_def;
    Signature sig;
} *AstFunctionDeclarator;

typedef struct astArrayDeclarator {
    AST_DECLARATOR_COMMON
    AstExpression expr;
} *AstArrayDeclarator;

typedef struct astPointerDeclarator {
    AST_DECLARATOR_COMMON
    AstNode type_quals;
} *AstPointerDeclarator;

typedef struct astStructDeclarator {
    AST_NODE_COMMON
    AstDeclarator dec;
    AstExpression expr;
} *AstStructDeclarator;

typedef struct astStructDeclaration {
    AST_NODE_COMMON
    AstSpecifiers specs;
    AstNode st_decs;
} *AstStructDeclaration;

typedef struct astStructSpecifier {
    AST_NODE_COMMON
    char *id;
    AstNode st_decls;
} *AstStructSpecifer;

typedef struct astEnumerator {
    AST_NODE_COMMON
    char *id;
    AstExpression expr;
} *AstEnumerator;

typedef struct astEnumSpecifier {
    AST_NODE_COMMON
    char *id;
    AstNode enumers;
} *AstEnumSpecifier;

typedef struct astTypedefName {
    AST_NODE_COMMON
    char *id;
    Symbol sym;
} *AstTypedefName;

typedef struct astToken {
    AST_NODE_COMMON
    int token;
} *AstToken;

struct astSpecifiers {
    AST_NODE_COMMON
    AstNode stg_classes;
    AstNode type_quals;
    AstNode type_specs;
    int sclass;
    Type ty;
};

struct astTypeName {
    AST_NODE_COMMON
    AstSpecifiers specs;
    AstDeclarator dec;
};

struct astDeclaration {
    AST_NODE_COMMON
    AstSpecifiers specs;
    AstNode init_decs;
};

typedef struct astFunction {
    AST_NODE_COMMON
    AstSpecifiers specs;
    AstDeclarator dec;
    AstFunctionDeclarator fdec;
    AstNode decls;
    AstStatement stmt;
    FunctionSymbol fsym;
    Label labels;
    Vector loops;
    Vector switches;
    Vector breakable;
    int has_return;
} *AstFunction;

struct astTransUnit {
    AST_NODE_COMMON
    AstNode ext_decls;
};

void CheckLocalDeclaration(AstDeclaration, Vector);
Type CheckTypeName(AstTypeName);

extern AstFunction current_f;

#endif
