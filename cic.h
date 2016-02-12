#include <set>
#include <string>
#include <vector>
#include <cassert>

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
