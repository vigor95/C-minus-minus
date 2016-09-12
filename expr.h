#ifndef __EXPR_H_
#define __EXPR_H_

enum OP {
#define OPINFO(op, prec, name, func, opcode) op,
#include "opinfo.h"
#undef OPINFO
};

struct tokenOp {
    int bop: 16;
    int uop: 16;
};

struct astExpression {
    AST_NODE_COMMON
    Type ty;
    int op: 16;
    int isarray: 1;
    int isfunc: 1;
    int lvalue: 1;
    int bitfld: 1;
    int inreg: 1;
    int unused: 11;
    struct astExpression *kids[2];
    union value val;
};

#define IsBinary(tok)   (TK_OR <= tok && tok <= TK_MOD)
#define Binary_OP       token_ops[current_token - TK_ASSIGN].bop
#define UNARY_OP        token_ops[current_token - TK_ASSIGN].uop

int CanAssign(Type, AstExpression);
AstExpression Constant(struct coord, Type, union value);
AstExpression Cast(Type, AstExpression);
AstExpression Adjust(AstExpression, int);
AstExpression DoIntegerPromotion(AstExpression);
AstExpression FoldConstant(AstExpression);
AstExpression FoldCast(Type, AstExpression);
AstExpr Not(AstExpression);
AstExpr CheckExpression(AstExpression);
AstExpr CheckConstantExpression(AstExpression);

void TranslateBranch(AstExpr, BBlock, BBlock);
Symbol TranslateExpression(AstExpression);

extern char *op_names[];

#endif
