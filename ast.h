#ifndef __AST_H_
#define __AST_H_

enum nodeKind {
    NK_TranslationUnit,     NK_Function,           NK_Declaration,
    NK_TypeName,            NK_Specifiers,         NK_Token,            
    NK_TypedefName,         NK_EnumSpecifier,      NK_Enumerator,       
    NK_StructSpecifier,     NK_UnionSpecifier,     NK_StructDeclaration,
    NK_StructDeclarator,    NK_PointerDeclarator,  NK_ArrayDeclarator,  
    NK_FunctionDeclarator,  NK_ParameterTypeList,  NK_ParameterDeclaration,
    NK_NameDeclarator,      NK_InitDeclarator,     NK_Initializer,
    NK_Expression,          NK_ExpressionStatement, NK_LabelStatement,
    NK_CaseStatement,       NK_DefaultStatement,    NK_IfStatement, 
    NK_SwitchStatement,     NK_WhileStatement,      NK_DoStatement,     
    NK_ForStatement,        NK_GotoStatement,       NK_BreakStatement,
    NK_ContinueStatement,   NK_ReturnStatement,     NK_CompoundStatement
};

#define AST_NODE_COMMON     \
    int kind;               \
    struct astNode *next;   \
    struct coord cd;

typedef struct astNode {
    AST_NODE_COMMON
} *AstNode;

typedef struct label {
    struct coord cd;
    char *id;
    int ref;
    int defined;
    BBlock respBB;
    struct label *next;
} *Label;

typedef struct astExpression        *AstExpression;
typedef struct astStatement         *AstStatement;
typedef struct astDeclaration       *AstDeclaration;
typedef struct astTypeName          *AstTypeName;
typedef struct astTranslationUnit   *AstTransUnit;

struct initData {
    int offset;
    AstExpression expr;
    InitData next;
};

#define CREATE_AST_NODE(p, k)   \
    CALLOC(p);                  \
    p->kind = NK_##k;           \
    p->cd = token_coord;

#define NEXT_TOKEN  current_token = GetNextToken();

AstExpression ParseExpr();
AstExpression ParseConstExpr();
AstExpression ParseAssignExpr();
AstStatement ParseCompoundStmt();
AstTypeName ParseTypeName();
AstDeclaration ParseDecl();
AstTransUnit ParseTransUnit(char*);

void PostCheckTypedef();
void CheckTransUnit(AstTransUnit);
void Translate(AstTransUnit);
void EmitTransUnit(AstTransUnit);

void Expect(int);
void SkipTo(int[], char*);
int CurrentTokenIn(int[]);
int IsTypeName(int);

void DumpTransUnit(AstTransUnit);
void DAssemTransUnit(AstTransUnit);

extern int current_token;
extern int first_declaration[];

#endif
