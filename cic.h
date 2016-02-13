#include <set>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>

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

struct Buffer {
    char *body;
    int nalloc;
    int len;
};

struct File {
    FILE *file;
    char *p;
    std::string name;
    int line, column;
    int ntok;
    int last;
    int buf[3];
    int buflen;
};

struct Token {
    int kind;
    int line, column;
    bool space, bol;
    int cnt;
    std::set<std::string> hideset;
    union {
        int id;
        struct {
            char *sval;
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
    Token(int k, char *p) {
        kind = k;
        sval = p;
    }
    Token(int k, char *s, int len, int _enc) {
        kind = k;
        sval = s;
        slen = len;
        enc = _enc;
    }
    Token& operator=(const Token &other) {
        kind = other.kind;
        return *this;
    }
};

// buffer.cpp
Buffer* makeBuffer();
char* bufBody(Buffer &b);
int bufLen(Buffer &b);
void bufWrite(Buffer &b, char c);
void buf_Append(Buffer &b, char *s, int len);
void bufPrintf(Buffer &b, char *fmt, ...);
char *vformat(char *fmt, va_list ap);
char *format(char *fmt, ...);
char *quoteCstring(char *p);
char *quoteCstringLen(char *p, int len);
char *quoteChar(char c);

//file.cpp
File *makeFile(FILE &file, char *name);
File *makeFileString(char *s);
int readc(void);
void unreadc(int c);
File *currentFile(void);
void streamPush(File &file);
int streamDepth(void);
char *inputPosition(void);
void streamStash(File &f);
void streamUnstash(void);
