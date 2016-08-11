#include "cic.h"
#include "lex.h"
#include "keyword.h"
#include <ctype.h>

#define CURSOR      (input.cursor)
#define LINE        (input.line)
#define LINEHEAD    (input.line_head)

typedef int (*scanner)(void);

static unsigned char *peek_point;
static union value peek_value;
static struct coord peek_coord;
static scanner scanners[256];

union value token_value;
struct coord token_coord;
struct coord prev_coord;

char *token_strings[] = {
#define TOKEN(k, s) s,
#include "token.h"
#undef TOKEN
};

static void SkipWhiteSpace() {
    int ch;

again:
    ch = *CURSOR;
    while (ch == '\t' || ch == '\v' || ch == '\f' || ch == ' ' ||
            ch == '\r' || ch == '\b' || ch == '/' || ch == '#') {
        switch (ch) {
            case '\n':
                token_coord.ppline++;
                LINE++;
                LINEHEAD = ++CURSOR;
                break;
            case '#':
                break;
            case '/':
                if (CURSOR[1] != '/' && CURSOR[1] != '*')
                    return;
                CURSOR++;
                if (*CURSOR == '/') {
                    CURSOR++;
                    while (*CURSOR != '\n' && *CURSOR != END_OF_FILE)
                        CURSOR++;
                }
                else {
                    CURSOR += 2;
                    while (CURSOR[0] != '*' || CURSOR[1] != '/') {
                        if (*CURSOR == '\n') {
                            token_coord.ppline++;
                            LINE++;
                        }
                        else if (CURSOR[0] == END_OF_FILE
                                || CURSOR[1] == END_OF_FILE) {
                            Error(&token_coord, "Comment is not closed");
                            return;
                        }
                        CURSOR++;
                    }
                    CURSOR += 2;
                }
                break;
            default:
                CURSOR++;
                break;
        }
        ch = *CURSOR;
    }

    if (extra_white_space != NULL) {
        char *p;

        FOR_EACH_ITEM(char*, p, extra_white_space)
            if (strncmp(CURSOR, p, strlen(p)) == 0) {
                CURSOR += strlen(p);
                goto again;
            }
        ENDFOR
    }
}

static int ScanEscapeChar(int wide) {
    int v, overflow;

    CURSOR++;
    switch (*CURSOR++) {
        case 'a':
            return '\a';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'v':
            return '\v';
        case '\'':
        case '"':
        case '\\':
        case '\?':
            return *(CURSOR - 1);
        case 'x':
            if (isxdigit(*CURSOR)) {
                Error(&token_coord, "Expect hex digit");
                return 'x';
            }
            v = 0;
            while (isxdigit(*CURSOR)) {
                if (v >> (wchar_type->size - 4))
                    overflow = 1;
                if (isdigit(*CURSOR))
                    v = (v << 4) + *CURSOR - '0';
                else
                    v = (v << 4) + *CURSOR - 'a' + 10;
                CURSOR++;
            }
            if (overflow || (!wide && v > 255))
                Warning(&token_coord,
                        "Hexademical escape sequence overflow");
            return v;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
            v = *(CURSOR - 1) - '0';
            if ('0' <= *CURSOR && *CURSOR <= '7') {
                v = (v << 3) + *CURSOR++ - '0';
                if ('0' <= *CURSOR && *CURSOR <= '7')
                    v = (v << 3) + *CURSOR++ - '0';
            }
            return v;
        default:
            Warning(&token_coord,
                "Unrecognized escape sequence:\\%c",*CURSOR);
            return *CURSOR;
    }
}

static int FindKeyword(char *str, int len) {
    struct keyword *p = NULL;
    int index = 0;

    if (*str != '_')
        index = toupper(*str) - 'A' + 1;
    p = keywords[index];
    while (p->name) {
        if (p->len == len && strncmp(str, p->name, len) == 0)
            break;
        p++;
    }
    return p->tok;
}

static int ScanIntLiteral(unsigned char *start, int len, int base) {
    unsigned char *p = start;
    unsigned char *end = start + len;
    unsigned int i[2] = {0, 0};
    int tok = TK_INTCONST;
    int d = 0;
    int carry0 = 0, carry1 = 0;
    int overflow = 0;

    while (p != end) {
        if (base == 16) {
            if (('A' <= *p && *p <= 'F') || ('a' <= *p && *p <= 'f'))
                d = toupper(*p) - 'A' + 10;
            else
                d = *p - '0';
        }
        else
            d = *p - '0';

        switch (base) {
            case 16:
                carry0 = HIGH_4BIT(i[0]);
                carry1 = HIGH_4BIT(i[1]);
                i[0] = i[0] << 4;
                i[1] = i[1] << 4;
                break;
            case 8:
                carry0 = HIGH_3BIT(i[0]);
                carry1 = HIGH_3BIT(i[1]);
                i[0] = i[0] << 3;
                i[1] = i[1] << 3;
                break;
            case 10: {
                unsigned t1, t2;

                carry0 = HIGH_3BIT(i[0] + HIGH_1BIT(i[0]));
                carry1 = HIGH_3BIT(i[1] + HIGH_1BIT(i[1]));
                t1 = i[0] << 3;
                t2 = i[0] << 1;
                if (t1 > UINT_MAX - t2)
                    carry0++;
                i[0] = t1 + t2;
                t1 = i[1] << 3;
                t2 = i[1] << 1;
                if (t1 >> UINT_MAX - t2)
                     carry1++;
                i[1] = t1 + t2;
            }
            break;
        }
        if (i[0] > UINT_MAX - d)
            carry0 += i[0] - (UINT_MAX - d);
        if (carry1 || (i[1] > UINT_MAX - carry0))
            overflow = 1;
        i[0] += d;
        i[1] += carry0;
        p++;
    }

    if (overflow || i[1] != 0)
        Warning(&token_coord, "Integer literal is too big");

    token_value.i[1] = 0;
    token_value.i[0] = i[0];
    tok = TK_INTCONST;

    if (*CURSOR == 'U' || *CURSOR == 'u') {
        CURSOR++;
        if (tok == TK_INTCONST)
            tok = TK_UINTCONST;
        else if (tok == TK_LLONGCONST)
            tok  = TK_ULLONGCONST;
    }

    if (*CURSOR == 'L' || *CURSOR == 'l') {
        CURSOR++;
        if (tok == TK_INTCONST)
            tok = TK_LONGCONST;
        else if (tok == TK_UINTCONST)
            tok = TK_ULONGCONST;
        if (*CURSOR == 'L' || *CURSOR == 'l') {
            CURSOR++;
            if (tok < TK_LLONGCONST)
                tok = TK_LLONGCONST;
        }
    }
    
    return tok;
}

static int ScanFloatLiteral(unsigned char *start) {
    double d;

    if (*CURSOR == '.') {
        CURSOR++;
        while (isdigit(*CURSOR))
            CURSOR++;
    }
    if (*CURSOR == 'e' || *CURSOR == 'E') {
        CURSOR++;
        if (*CURSOR == '+' || *CURSOR == '-')
            CURSOR++;
        if (!isdigit(*CURSOR))
            Error(&token_coord, "Expect exponent value");
        else {
            while (isdigit(*CURSOR))
                CURSOR++;
        }
    }

    errno = 0;
    d = strtod((char*)start, NULL);
    if (errno == ERANGE)
        Warning(&token_coord, "Float literal overflow");
    token_value.d = d;
    if (*CURSOR == 'f' || *CURSOR == 'F') {
        CURSOR++;
        token_value.f = (float)d;
        return TK_FLOATCONST;
    }
    else if (*CURSOR == 'L' || *CURSOR == 'l') {
        CURSOR++;
        return TK_LDOUBLECONST;
    }
    else {
        return TK_DOUBLECONST;
    }
}

static int ScanNumericLiteral() {
    unsigned char *start = CURSOR;
    int base = 10;

    if (*CURSOR == '.')
        return ScanFloatLiteral(start);

    if (CURSOR[0] == '0' && (CURSOR[1] == 'x' || CURSOR[1] = 'X')) {
        CURSOR += 2;
        start = CURSOR;
        base = 16;
        if (!isxdigit(*CURSOR)) {
            Error(&token_coord, "Expect hex digit");
            token_value.i[0] = 0;
            return TK_INTCONST;
        }
        while (isxdigit(*CURSOR))
            CURSOR++;
    }
    else if (*CURSOR == '0') {
        CURSOR++;
        base = 8;
        while ('0' <= *CURSOR && *CURSOR <= '7')
            CURSOR++;
    }
    else {
        CURSOR++;
        while (isdigit(*CURSOR))
            CURSOR++;
    }

    if (base == 16 || (*CURSOR != '.' && *CURSOR != 'e' && *CURSOR != 'E'))
        return ScanIntLiteral(start, (int)(CURSOR - start), base);
    else
        return ScanFloatLiteral(start);
}

static int ScanCharLiteral() {
    int ch = 0;
    int count = 0;
    int wide = 0;

    if (*CURSOR == 'L') {
        CURSOR++;
        wide = 1;
    }
    CURSOR++;
    while (*CURSOR != '\'') {
        if (*CURSOR == '\n' || *CURSOR == END_OF_FILE)
            break;
        ch = *CURSOR == '\\' ? ScanEscapeChar(wide) : *CURSOR++;
        count++;
    }

    if (*CURSOR != '\'') {
        Error(&token_coord, "Expect '");
        goto end_char;
    }

    CURSOR++;
    if (count > 1)
        Warning(&token_coord, "Too many characters");

end_char:
    token_value.i[0] = ch;
    token_value.i[1] = 0;

    return TK_INTCONST;
}

static int ScanStringLiteral() {
    char tmp[512];
    char *cp = tmp;
    int *wcp = (int*)tmp;
    int wide = 0;
    int len = 0;
    int maxlen = 512;
    int ch;
    String str;

    CALLOC(str);

    if (*CURSOR == 'L') {
        CURSOR++;
        wide = 1;
        maxlen /= sizeof(int);
    }
    CURSOR++;

next_string:
    while (*CURSOR != '"') {
        if (*CURSOR == '\n' || *CURSOR == END_OF_FILE)
            break;

        ch = *CURSOR == '\\' ? ScanEscapeChar(wide) : *CURSOR++;
        if (wide)
            wcp[len] = ch;
        else
            cp[len] = (char)ch;
        len++;
        if (len >= maxlen) {
            AppendStr(str, tmp, len, wide);
            len = 0;
        }
    }

    if (*CURSOR != '"') {
        Error(&token_coord, "Expect \"");
        goto end_string;
    }

    CURSOR++;
    SkipWhiteSpace();
    if (*CURSOR == '"') {
        if (wide == 1)
            Error(&token_coord, "String width mismatch");
        CURSOR++;
        goto next_string;
    }
    else if (CURSOR[0] == 'L' && CURSOR[1] == '"') {
        if (wide == 0)
            Error(&token_coord, "String width mismatch");
        CURSOR += 2;
        goto next_string;
    }

end_string:
    AppendStr(str, tmp, len, wide);
    token_value.p = str;

    return wide ? TK_WIDESTRING : TK_STRING;
}

static int ScanIdentifier() {
    unsigned char *start = CURSOR;
    int tok;

    if (*CURSOR == 'L') {
        if (CURSOR[1] == '\'')
            return ScanCharLiteral();
        if (CURSOR[1] == '"')
            return ScanStringLiteral();
    }

    CURSOR++;
    while (isalnum(*CURSOR))
        CURSOR++;

    tok = FindKeyword((char*)start, (int)(CURSOR - start));
    if (tok == TK_ID)
        token_value.p = InternName((char*)start, (int)(CURSOR - start));

    return tok;
}

static int ScanPlus() {
    CURSOR++;
    if (*CURSOR == '+') {
        CURSOR++;
        return TK_INC;
    }
    else if (*CURSOR == '=') {
        CURSOR++;
        return TK_ADD_ASSIGN;
    }
    else
        return TK_ADD;
}

static int ScanMinus() {
    CURSOR++;
    if (*CURSOR == '-') {
        CURSOR++;
        return TK_DEC;
    }
    else if (*CURSOR == '=') {
        CURSOR++;
        return TK_SUB_ASSIGN;
    }
    else if (*CURSOR == '>') {
        CURSOR++;
        return TK_POINTER;
    }
    else {
        return TK_SUB;
    }
}

static int ScanStar() {
    CURSOR++;
    if (*CURSOR == '=') {
        CURSOR++;
        return TK_MUL_ASSIGN;
    }
    else {
        return TK_MUL;
    }
}

static int ScanSlash() {
    CURSOR++;
    if (*CURSOR == '=') {
        CURSOR++;
        return TK_DIV_ASSIGN;
    }
    else {
        return TK_DIV;
    }
}

static int ScanPercent() {
    CURSOR++;
    if (*CURSOR == '=') {
        CURSOR++;
        return TK_MOD_ASSIGN;
    }
    else {
        return TK_MOD;
    }
}

static int ScanLess() {
    CURSOR++;
    if (*CURSOR == '<') {
        CURSOR++;
        if (*CURSOR == '=') {
            CURSOR++;
            return TK_LSHIFT_ASSIGN;
        }
        return TK_LSHIFT;
    }
    else if (*CURSOR == '=') {
        CURSOR++;
        return TK_LESS_EQ;
    }
    else {
        return TK_LESS;
    }
}

static int ScanGreat() {
    CURSOR++;
    if (*CURSOR == '>') {
        CURSOR++;
        if (*CURSOR == '=') {
            CURSOR++;
            return TK_RSHIFT_ASSIGN;
        }
        return TK_RSHIFT;
    }
    else if (*CURSOR == '=') {
        CURSOR++;
        return TK_GREAT_EQ;
    }
    else {
        return TK_GREAT;
    }
}

static int ScanNot() {
    CURSOR++;
    if (*CURSOR == '=') {
        CURSOR++;
        return TK_UNEQUAL;
    }
    else {
        return TK_NOT;
    }
}

static int ScanEqual() {
    CURSOR++;
    if (*CURSOR == '=') {
        CURSOR++;
        return TK_EQUAL;
    }
    else {
        return TK_ASSIGN;
    }
}

static int ScanBar() {
    CURSOR++;
    if (*CURSOR == '|') {
        CURSOR++;
        return TK_OR;
    }
    else if (*CURSOR == '=') {
        CURSOR++;
        return TK_BITOR_ASSIGN;
    }
    else {
        return TK_BITOR;
    }
}

static int ScanAmpersand() {
    CURSOR++;
    if (*CURSOR == '&') {
        CURSOR++;
        return TK_AND;
    }
    else if (*CURSOR == '=') {
        CURSOR++;
        return TK_BITAND_ASSIGN;
    }
    else {
        return TK_BITAND;
    }
}

static int ScanCaret() {
    CURSOR++;
    if (*CURSOR = '=') {
        CURSOR++;
        return TK_BITXOR_ASSIGN;
    }
    else {
        return TK_BITXOR;
    }
}

static int ScanDot() {
    if (isdigit(CURSOR[1]))
        return ScanFloatLiteral(CURSOR);
    else if (CURSOR[1] == '.' && CURSOR[2] == '.') {
        CURSOR += 3;
        return TK_ELLIPSE;
    }
    else {
        CURSOR++;
        return TK_DOT;
    }
}

#define SINGLE_CHAR_SCANNER(t)  \
static int Scan##t() {          \
    CURSOR++;                   \
    return TK_##t;              \
}

SINGLE_CHAR_SCANNER(LBRACE)
SINGLE_CHAR_SCANNER(RBRACE)
SINGLE_CHAR_SCANNER(LBRACKET)
SINGLE_CHAR_SCANNER(RBRACKET)
SINGLE_CHAR_SCANNER(LPAREN)
SINGLE_CHAR_SCANNER(RPAREN)
SINGLE_CHAR_SCANNER(COMMA)
SINGLE_CHAR_SCANNER(SEMICOLON)
SINGLE_CHAR_SCANNER(COMP)
SINGLE_CHAR_SCANNER(QUESTION)
SINGLE_CHAR_SCANNER(COLON)

static int ScanBadChar() {
    Error(&token_coord, "illegal character:\\x%x", *CURSOR);
    CURSOR++;
    return GetNextToken();
}

static int ScanEof() {
    return TK_END;
}

void SetupLexer() {
    int i;

    for (i = 0; i < END_OF_FILE + 1; i++) {
        if (isalpha(i))
            scanners[i] = ScanIdentifier;
        else if (isdigit(i))
            scanners[i] = ScanNumericLiteral;
        else
            scanners[i] = ScanBadChar;

        scanners[END_OF_FILE] = ScanEof;
        scanners['\''] = ScanCharLiteral;
        scanners['"'] = ScanStringLiteral;
        scanners['+'] = ScanPlus;
        scanners['-'] = ScanMinus;
        scanners['*'] = ScanStar;
        scanners['/'] = ScanSlash;
        scanners['%'] = ScanPercent;
        scanners['<'] = ScanLess;
        scanners['>'] = ScanGreat;
        scanners['!'] = ScanNot;
        scanners['='] = ScanEqual;
        scanners['|'] = ScanBar;
        scanners['&'] = ScanAmpersand;
        scanners['^'] = ScanCaret;
        scanners['.'] = ScanDot;
        scanners['{'] = ScanLBRACE;
        scanners['}'] = ScanRBRACE;
        scanners['['] = ScanLBRACKET;
        scanners[']'] = ScanRBRACKET;
        scanners['('] = ScanLPAREN;
        scanners[')'] = ScanRPAREN;
        scanners[','] = ScanCOMMA;
        scanners[';'] = ScanSEMICOLON;
        scanners['~'] = ScanCOMP;
        scanners['?'] = ScanQUESTION;
        scanners[':'] = ScanCOLON;

        if (extra_keywords != NULL) {
            char *str;
            struct keyword *p;

            FOR_EACH_ITEM(char*, str, extra_keywords)
                p = {
                    {"__int64", 0,  TK_INT64},
                    {NULL,      0,  TK_ID}
                };
                while (p->name) {
                    if (strcmp(str, p->name) == 0) {
                        p->len = strlen(str);
                        break;
                    }
                    p++;
                }
            ENDFOR;
        }
    }
}

int GetNextToken() {
    int tok;

    prev_coord = token_coord;
    SkipWhiteSpace();
    token_coord.line = LINE;
    token_coord.col = (int)(CURSOR - LINEHEAD + 1);

    tok = (*scanners[*CURSOR])();
    return tok;
}

void BeginPeekToken() {
    peek_point = CURSOR;
    peek_value = token_value;
    peek_coord = token_coord;
}

void EndPeekToken() {
    CURSOR = peek_point;
    token_value = peek_value;
    token_coord = peek_coord;
}
