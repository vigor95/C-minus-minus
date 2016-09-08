#include "cic.h"
#include "ast.h"

int current_token;

void Expect(int tok) {
    if (current_token == tok) {
        NEXT_TOKEN;
        return;
    }

    if (tok == TK_SEMICOLON && token_coord.line - prev_coord.line == 1) {
        Error(&prev_coord, "Expect ';'");
    }
    else {
        Error(&token_coord, "Expect %s", token_strings[tok - 1]);
    }
}

int CurrentTokenIn(int toks[]) {
    int *p = toks;

    while (*p) {
        if (current_token == *p)
            return 1;
        p++;
    }

    return 0;
}

void SkipTo(int toks[], char *e_msg) {
    int *p = toks;
    struct coord cd;

    if (CurrentTokenIn(toks) || current_token == TK_END)
        return;

    cd = token_coord;
    while (current_token != TK_END) {
        p = toks;
        while (*p) {
            if (current_token == *p)
                goto sync;
            p++;
        }
        NEXT_TOKEN;
    }

sync:
    Error(&cd, "skip to %s\n", e_msg);
}
