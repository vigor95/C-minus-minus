#include <set>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

enum {
    TIDENT,
    TKEYWORD,
    TNUMBER,
    TCHAR,
    TSTRING,
    TEOF,
    TINVALID,
                                // Only in CPP
    MIN_CPP_TOKEN,
    TNEWLINE,
    TSPACE,
    TMACRO_PARAM
};

enum {
    ENC_NONE,
    ENC_CHAR16,
    ENC_CHAR32,
    ENC_UTF8,
    ENC_WCHAR
};

struct Buffer {
    char *body;
    int nalloc;
    int len;
};

struct File {
    FILE *file;
    const char *p;
    const char *name;
    int line, column;
    int ntok;
    int last;
    int buf[3];
    int buflen;
};

struct Token {
    int kind;
    File *file;
    int line, column;
    bool space, bol;
    int cnt;
    std::set<std::string> hideset;
    union {
        int id;
        struct {
            const char *sval;
            int slen;
            int c;
            int enc;
        };
        struct {
            bool is_vararg;
            int pos;
        };
    };
    Token() {}
    Token(int k): kind(k) {}
    Token(int k, const char *p): kind(k), sval(p) {}
    Token(int k, const char *s, int len, int _enc):
    kind(k), sval(s), slen(len), enc(_enc) {}
    Token(int k, int i): kind(k), id(i) {}
    Token(int k, char _c): kind(k), c(_c) {}
    Token(int k, int _c, int _enc): kind(k), c(_c), enc(_enc) {}
    Token& operator=(const Token &other) {
        kind = other.kind;
        return *this;
    }
};

enum {
    AST_LITERAL = 256,
    AST_LVAR,
    AST_GVAR,
    AST_TYPEDEF,
    AST_FUNCALL,
    AST_FUNCPTR_CALL,
    AST_FUNCDESG,
    AST_FUNC,
    AST_DECL,
    AST_INIT,
    AST_CONV,
    AST_ADDR,
    AST_DEREF,
    AST_IF,
    AST_TERNARY,
    AST_DEFAULT,
    AST_RETURN,
    AST_COMPOUND_STMT,
    AST_STRUCT_REF,
    AST_GOTO,
    AST_COMPUTED_GOTO,
    AST_LABEL,
    OP_SIZEOF,
    OP_CAST,
    OP_SHR,
    OP_SHL,
    OP_A_SHR,
    OP_A_SHL,
    OP_PRE_INC,
    OP_PRE_DEC,
    OP_POST_INC,
    OP_POST_DEC,
    OP_LABEL_ADDR,
#define op(name, _) name,
#define keyword(name, x, y) name,
#include "keyword.inc"
#undef keyword
#undef op
};

// buffer.cpp
Buffer* makeBuffer();
char* bufBody(Buffer &b);
int bufLen(Buffer &b);
void bufWrite(Buffer &b, char c);
void buf_Append(Buffer &b, char *s, int len);
void bufPrintf(Buffer &b, char *fmt, ...);
char *vformat(const char *fmt, va_list ap);
char *format(const char *fmt, ...);
char *quoteCstring(char *p);
char *quoteCstringLen(char *p, int len);
char *quoteChar(char c);

//file.cpp
File *makeFile(FILE *file, const char *name);
File *makeFileString(const char *s);
int readc(void);
void unreadc(int c);
File *currentFile(void);
void streamPush(File *file);
int streamDepth(void);
char *inputPosition(void);
void streamStash(File *f);
void streamUnstash(void);

//lex.cpp
void lexInit(const char *filename);
bool isKeyword(Token &tk, int c);
void tokenBufferStash(std::vector<Token*> *buf);
void tokenBufferUnstash();
void ungetToken(Token *tk);
Token* lexString(char *s);
Token* lex();

//error.cpp
extern bool enable_warning;
extern bool dumpstack;
extern bool dumpsource;
extern bool w_e;

#define STR2(x) #x
#define STR(x) STR2(x)
#define error(...)       errorf(__FILE__ ":" STR(__LINE__), NULL, __VA_ARGS__)
#define errort(tok, ...) errorf(__FILE__ ":" STR(__LINE__), token_pos(tok), __VA_ARGS__)
#define warn(...)        warnf(__FILE__ ":" STR(__LINE__), NULL, __VA_ARGS__)
#define warnt(tok, ...)  warnf(__FILE__ ":" STR(__LINE__), token_pos(tok), __VA_ARGS__)

[[noreturn]] void errorf(const char *line, const char *pos, const char *fmt, ...);
void warnf(const char *line, const char *pos, const char *fmt, ...);
const char* tokenPos(Token *tk);
