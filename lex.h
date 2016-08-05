#ifndef __LEX_H_
#defien __LEX_H_

enum token {
    TK_BEGIN,
#define TOKEN(k, s) k,
#include "token.h"
#undef TOKEN
};

union value {
    int i[2];
    float f;
    double d;
    void *p;
};

void SetupLexer();
void BeginPeekToken();
void EndPeekToken();
int GetNextToken();

extern union value token_value;
extern struct Coord token_coord;
extern struct Corrd prev_coord;
extern char *token_strings[];

#endif
