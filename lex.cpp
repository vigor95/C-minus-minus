#include "cic.h"

#define errorp(p, ...) errorf(__FILE__ ":" STR(__LINE__), posString(&p), __VA_ARGS__)
#define warnp(p, ...) warnf(__FILE__ ":" STR(__LINE__), posString(&p), __VA_ARGS__)

struct Pos {
    int line, column;
};

static Pos pos;
static std::vector<std::vector<Token*>* > *buffers = new std::vector<std::vector<Token*> *>;

void lexInit(const char *filename) {
    buffers->push_back(new std::vector<Token*>);
    FILE *fp = fopen(filename, "r");
    if (!fp) error("Cannot open %s: %s", filename, strerror(errno));
    printf("Open %s successfully!\n", filename);
    streamPush(makeFile(fp, filename));
    printf("streamPush ok\n");
}

static const char* posString(Pos *p) {
    File *f = currentFile();
    return format("%s:%d:%d", f ? f->name : "(unknown)", p->line, p->column);
}

static Pos getPos(int delta) {
    File *f = currentFile();
    return (Pos){f->line, f->column + delta};
}

static void mark() {
    pos = getPos(0);
}

static Token* makeToken(Token *tmpl) {
    Token *r = new Token;
    *r = *tmpl;
    File *f = currentFile();
    r->file = f;
    r->line = pos.line;
    r->column = pos.column;
    r->cnt = f->ntok++;
    return r;
}

static Token* makeIdent(const char *p) {
    return makeToken(new Token(TIDENT, p));
}

static Token* makeStrtok(const char *s, int len, int enc) {
    return makeToken(new Token(TSTRING, s, len, enc));
}

static Token* makeKeyword(int id) {
    return makeToken(new Token(TKEYWORD, id));
}

static Token* makeNumber(const char *s) {
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

static Token* readChar(int enc) {
    int c = readc();
    int r = (c == '\\') ? readEscapedChar() : c;
    c = readc();
    if (c != '\'') errorp(pos, "unterminated char");
    if (enc == ENC_NONE) return makeChar((char)r, enc);
    return makeChar(r, enc);
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
        bufWrite(*b, c);
    }
    bufWrite(*b, '\0');
    return makeStrtok(bufBody(*b), bufLen(*b), enc);
}

static Token* readIdent(char c) {
    Buffer *b = makeBuffer();
    bufWrite(*b, c);
    while (1) {
        c = readc();
        if (isalnum(c) || c & 0x80 || c == '_' || c == '$') {
            bufWrite(*b, c);
            continue;
        }
        unreadc(c);
        bufWrite(*b, '\0');
        return makeIdent(bufBody(*b));
    }
}

static Token* readRep(char expect, int t1, int els) {
    return makeKeyword(next(expect) ? t1 : els);
}

static Token* readRep2(char expect1, int t1, char expect2, int t2, char els) {
    if (next(expect1)) return makeKeyword(t1);
    return makeKeyword(next(expect2) ? t2 : els);
}

static Token* doReadToken() {
    if (skipSpace()) return new Token(TSPACE);
    mark();
    int c = readc();
    switch (c) {
        case '\n': return new Token(TNEWLINE);
        case ':': return makeKeyword(next('<') ? ']' : ':');
        case '+': return readRep2('+', OP_INC, '=', OP_A_ADD, '+');
        case '*': return readRep('=', OP_A_MUL, '*');
        case '=': return readRep('=', OP_EQ, '=');
        case '!': return readRep('=', OP_NE, '!');
        case '&': return readRep2('&', OP_LOGAND, '=', OP_A_AND, '&');
        case '|': return readRep2('|', OP_LOGOR, '=', OP_A_OR, '|');
        case '^': return readRep('=', OP_A_XOR, '^');
        case '"': return readString(ENC_NONE);
        case '\'': return readChar(ENC_NONE);
        case '/': return makeKeyword(next('=') ? OP_A_DIV : '/');
        case 'a' ... 't': case 'v' ... 'z': case 'A' ... 'K':
        case 'M' ... 'T': case 'V' ... 'Z': case '_': case '$':
        case 0x80 ... 0xfd: return readIdent(c);
        case '0' ... '9': return readNumber(c);
        case '.':
            if (isdigit(peek())) return readNumber(c);
            if (next('.')) {
                if (next('.'))
                    return makeKeyword(KELLIPSIS);
            }
            return makeIdent("..");
        case '(': case ')': case ',': case ';': case '[':
        case ']': case '{': case '}': case '?': case '~':
            return makeKeyword(c);
        case '-':
            if (next('-')) return makeKeyword(OP_DEC);
            if (next('>')) return makeKeyword(OP_ARROW);
            if (next('=')) return makeKeyword(OP_A_SUB);
            return makeKeyword('-');
        case '<':
            if (next('<')) return readRep('=', OP_A_SAL, OP_SAL);
            if (next('=')) return makeKeyword(OP_LE);
            return makeKeyword('<');
        case '>':
            if (next('=')) return makeKeyword(OP_GE);
            if (next('>')) return readRep('=', OP_A_SAR, OP_SAR);
            return makeKeyword('>');
        case '%': return readRep('=', OP_A_MOD, '%');
        case EOF: return new Token(TEOF);
        default: return makeInvalid(c);
    }
}

static bool bufferEmpty() {
    return buffers->size() == 1 && (*buffers)[0]->size() == 0;
}

bool isKeyword(Token &tk, int c) {
    return tk.kind == TKEYWORD && tk.id == c;
}

void tokenBufferStash(std::vector<Token*> *buf) {
    buffers->push_back(buf);
}

void tokenBufferUnstash() {
    buffers->pop_back();
}

void ungetToken(Token *tk) {
    if (tk->kind == TEOF) return;
    std::vector<Token*> *buf = (*buffers)[buffers->size() - 1];
    buf->push_back(tk);
}

Token* lexString(char *s) {
    Token *r = doReadToken();
    next('\n');
    Pos p = getPos(0);
    if (peek() != EOF) errorp(p, "unconsumed input: %s", s);
    return r;
}

Token* lex() {
    //if (buffers == NULL) buffers = new std::vector<std::vector<Token*> >;
    std::vector<Token*> *buf = 
        (*buffers)[buffers->size() - 1];
    if (buf->size() > 0) {
        buf->pop_back();
        return (*buf)[buf->size()];
    }
    if(buffers->size() > 1) return new Token(TEOF);
    bool bol = (currentFile()->column == 1);
    Token *tk = doReadToken();
    while (tk->kind == TSPACE) {
        tk = doReadToken();
        tk->space = 1;
    }
    tk->bol = bol;
    return tk;
}
