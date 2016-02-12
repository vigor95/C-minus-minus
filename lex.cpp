#include "cic.h"

struct Pos {
    int line, column;
} pos;

static Token makeToken(Token *tmpl) {
    Token r = *tmpl;
    r.line = pos.line;
    r.column = pos.column;
    return r;
}

static Token makeIdent(char *p) {
    return makeToken(new Token(TIDENT, p));
}

static Token makeStrtok(char *s, int len, int enc) {
    return makeToken(new Token(TSTRING, s, len, enc));
}

static Token makeKeyword(int id) {
    return makeToken(new Token(TKEYWORD, id));
}

static Token makeNumber(char *s) {
    return makeToken(new Token(TNUMBER, s));
}

static Token makeInvalid(char c) {
    return makeToken(new Token(TINVALID, c));
}

static Token makeChar(int c, int enc) {
    return makeToken(new Token(TCHAR, c, enc));
}

static bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\f' || c == '\v';
}

static peek() {
    int r = readc();
    return r;
}
