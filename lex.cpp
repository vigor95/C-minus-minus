#include "cic.h"

struct Pos {
    int line, column;
};

static Pos pos;

static Pos getPos(int delta) {
    File *f = currentFile();
    return (Pos){f->line, f->column + delta};
}

static Token* makeToken(Token *tmpl) {
    Token *r = new Token;
    *r = *tmpl;
    r->line = pos.line;
    r->column = pos.column;
    return r;
}

static Token* makeIdent(char *p) {
    return makeToken(new Token(TIDENT, p));
}

static Token* makeStrtok(char *s, int len, int enc) {
    return makeToken(new Token(TSTRING, s, len, enc));
}

static Token* makeKeyword(int id) {
    return makeToken(new Token(TKEYWORD, id));
}

static Token* makeNumber(char *s) {
    return makeToken(new Token(TNUMBER, s));
}

static Token* makeInvalid(char c) {
    return makeToken(new Token(TINVALID, c));
}

static Token* makeChar(int c, int enc) {
    return makeToken(new Token(TCHAR, c, enc));
}

static bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\f' || c == '\v';
}

static int peek() {
    int r = readc();
    unreadc(r);
    return r;
}

static bool next(int expect) {
    int c = readc();
    if (c == expect) return 1;
    unreadc(c);
    return 0;
}

static void skipLine() {
    while (1) {
        int c = readc();
        if (c == EOF) return;
        if (c == '\n') {
            unreadc(c);
            return;
        }
    }
}

static void skipBlockComment() {
    Pos p = getPos(-2);
    bool maybe_end = 0;
    while (1) {
        int c = readc();
        if (c == EOF) errorp(p, "Premature end of block comment");
        if (c == '/' && maybe_end) return;
        maybe_end = (c == '*');
    }
}

static bool doSkipSpace() {
    int c = readc();
    if (c == EOF) return 0;
    if (isWhitespace(c)) return 1;
    if (c == '/') {
        if (next('*')) {
            skipBlockComment();
            return 1;
        }
        if (next('/')) {
            skipLine();
            return 1;
        }
    }
    unreadc(c);
    return 0;
}

static bool skipSpace() {
    if (!doSkipSpace()) return 0;
    while (doSkipSpace());
    return 1;
}

static void skipChar() {
    if (readc() == '\\') readc();
    int c = readc();
    while (c != EOF && c != '\'') c = readc();
}

static void skipString() {
    for (int c = readc(); c != EOF && c != '"'; c = readc())
        if (c == '\\')
            readc();
}

static Token* readNumber(char c) {
    Buffer *b = makeBuffer();
    bufWrite(*b, c);
    char last = c;
    while (1) {
        int c = readc();
        bool flonum = strchr("eEpP", last) && strchr("+-", c);
        if (!isdigit(c) && !isalpha(c) && c != '.' && !flonum) {
            unreadc(c);
            bufWrite(*b, '\0');
            return makeNumber(bufBody(*b));
        }
        bufWrite(*b, c);
        last = c;
    }
}

static bool nextOct() {
    int c = peek();
    return '0' <= c && c <= '7';
}

static int readOctalChar(int c) {
    int r = c - '0';
    if (!nextOct()) return r;
    r = (r << 3) | (readc() - '0');
    if (!nextOct()) return r;
    return (r << 3) | (readc() - '0');
}

static int readHexChar() {
    Pos p = getPos(-2);
    int c = readc();
    if (!isxdigit(c))
        errorp(p, 
            "\\x is not followed by a hexadecimal character: %c", c);
    int r = 0;
    for (;; c = readc()) {
        switch (c) {
            case '0' ... '9': r = (r << 4) | (c - '0'); continue;
            case 'a' ... 'f': r = (r << 4) | (c - 'a' + 10); continue;
            case 'A' ... 'F': r = (r << 4) | (c - 'A' + 10); continue;
            default: unreadc(c); return r;
        }
    }
}

static bool isValidUcn(unsigned c) {
    if (0xd800 <= c && c <= 0xdfff) return 0;
    return 0xa0 <= c || c == '$' || c == '@' || c == '`';
}

static int readUniversalChar(int len) {
    Pos p = getPos(-2);
    unsigned r = 0;
    for (int i = 0; i < len; i++) {
        char c = readc();
        switch (c) {
            case '0' ... '9': r = (r << 4) | (c - '0'); continue;
            case 'a' ... 'f': r = (r << 4) | (c - 'a' + 10); continue;
            case 'A' ... 'F': r = (r << 4) | (c - 'A' + 10); continue;
            default: errorp(p, "invalid universal character: %c", c);
        }            
    }
    if (!isValidUcn(r))
        errorp(p, 
        "invalid universal character: \\%c%0*x", (len == 4) ? 'u' : 'U', len, r);
    return r;
}

static int readEscapedChar() {
    Pos p = getPos(-1);
    int c = readc();
    switch (c) {
        case '\'': case '"': case '?': case '\\':
            return c;
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case 'e': return '\033';
        case 'x': return readHexChar();
        case 'u': return readUniversalChar(4);
        case 'U': return readUniversalChar(8);
        case '0' ... '7': return readOctalChar(c);
    }
    warnp(p, "unknown escaped character: \\%c", c);
    return c;
}

static Token* readString(int enc) {
    Buffer *b = makeBuffer();
    while (1) {
        int c = readc();
        if (c == EOF) errorp(pos, "unterminated string");
        if (c == '"') break;
        if (c != '\\') {
            bufWrite(*b, c);
            continue;
        }
        bool isucs = (peek() == 'u' || peek() == 'U');
        c = readEscapedChar();
        if (isucs) {
            writeUtf8(*b, c);
            continue;
        }
        bufWrite(*b, c);
    }
    bufWrite(*b, '\0');
    return makeStrtok(bufBody(*b), bufLen(*b), enc);
}
