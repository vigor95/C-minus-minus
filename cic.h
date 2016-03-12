#include <set>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <stack>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>
#include <climits>

#define null "null"
#define MAX_GRAM_NUM 230
#define MAX_GRAM_STMT_LEN 10
#define MAX_ALL_SYMBOL_NUM 170
#define MAX_ITEM_NUM 3000

typedef std::vector<std::string> svec;
typedef std::string chars;
typedef std::vector<int> ivec;

struct Source {
    std::string code;
    std::string file;
    int line;
    Source() {}
    Source(std::string c, std::string f, int l):
        code(c), file(f), line(l) {}
};
extern std::vector<Source> source;

struct DefDetail {
    std::string key, val;
    std::vector<std::string> para;
    DefDetail() = default;
    DefDetail(std::string k, std::string v): key(k), val(v) {}
};

extern std::string filename;

struct Word {
    chars type, val;
    int line;
    Word() = default;
    Word(chars t, chars v, int l): type(t), val(v), line(l) {}
};

extern std::vector<Word> word;

struct Type {
    chars  name, type;
    int array;
    int pointer;
    std::vector<int> index;
    Type() = default;
    Type(chars n, chars t, int a, int p, int i):
        name(n), type(t), array(a), pointer(p), index(i) {}
};

struct ParseTree {
    int stmt;
    Type type;
    chars val;
    std::vector<ParseTree*> tree;
    int line;
};
extern ParseTree *root;

struct IR {
    chars label, op, arg1, arg2, res;
    int out_struct_func;
    svec para;
    IR() = default;
    IR(chars o, chars a1, chars a2, chars r):
        op(o), arg1(a1), arg2(a2), res(r) {}
};
extern std::vector<IR> irs;

struct Func {
    Type type;
    chars name;
    svec flist;
    chars beg, mid, end;
    Func() = default;
    Func(Type t, chars n, svec l): type(t), name(n), flist(l) {}
    Func(Type t, chars n, svec l, chars b, chars m, chars e):
        type(t), name(n), flist(l), beg(b), mid(m), end(e) {}
};
extern std::vector<Func> funcs;

inline chars iTs(int i) {
    std::stringstream ss;
    ss << i;
    return ss.str();
}

inline int sTi(chars s) {
    std::istringstream iss(s);
    int num;
    iss >> num;
    return num;
}

inline double sTd(chars s) {
    std::istringstream iss(s);
    double num;
    iss >> num;
    return num;
}

inline bool _alpha(char ch) {
    return isalpha(ch) || ch == '_';
}

inline int digitVal(int ch) {
    if (isdigit(ch)) return ch - '0';
    if ('a' <= ch && ch <= 'f') return ch - 'a' + 10;
    if ('A' <= ch && ch <= 'F') return ch - 'A' + 10;
    return 0;
}

extern int preprocess();
extern int lex();
extern int parse();
extern int semantic();
