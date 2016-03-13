#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stack>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>

#define MAX_CODE_LEN 1000000

#define null "null"
#define MAX_GRAM_NUM		230
#define MAX_GRAM_STMT_LEN	10
#define MAX_ALL_SYMBOL_NUM 		170
#define MAX_ITEM_NUM 		3000

typedef std::string chars;
typedef std::vector<chars> svec;
typedef std::vector<int> ivec;

static int OUT_OTHER_FILE = 1;

#ifdef __linux_system__
    #define MAX_DEPTH		100000
#else
    #define MAX_DEPTH		10000
#endif

struct Source {
    chars code;
    chars file;
    int line;
    Source() {}
    Source(chars c, chars f, int l): code(c), file(f), line(l) {}
};
extern std::vector<Source> source;

struct DefDetail {
    chars key, val;
    svec para;
    DefDetail() {}
    DefDetail(chars k, chars v): key(k), val(v) {}
    DefDetail(chars k, chars v, svec p): key(k), val(v), para(p) {}
};

extern chars filename;

struct Word {
   chars type,val;
   int line;
   Word() {}
   Word(chars t, chars v, int l): type(t), val(v), line(l) {}
};
extern std::vector<Word> word;

struct Type {
    chars name;
    chars type;
    int array;
    int pointer;
    ivec index;
    Type() {}
    Type(chars n, chars t, int a, int p, ivec i): name(n), type(t),
        array(a), pointer(p), index(i) {}
};

// 语法树
struct ParseTree {
    int stmt;
    Type type;
    chars val;
    std::vector<ParseTree*> tree;
    int line;
};

extern ParseTree *headSubtr;

struct IR {
    chars label, op, arg1, arg2, result;
    int out_struct_function;
    svec parameter;
    IR(){}
    IR(chars _op, chars _arg1, chars _arg2, chars _result) {
        op = _op; arg1 = _arg1; arg2 = _arg2; result = _result;
    }
    IR(chars _label,chars _op, chars _arg1, chars _arg2, chars _result) {
        label = _label; op = _op; arg1 = _arg1; arg2 = _arg2; result = _result;
    }
};
extern std::vector<IR> semantic_exp;			// +  i  j  k  ==>  k = i+j;

struct Func {
    Type type;
    chars name;
    svec flist;
    chars beg, mid, end; // like: start = .L1
    Func() {}
    Func(Type t, chars n, svec l): type(t), name(n), flist(l) {}
    Func(Type t, chars n, svec l, chars s, chars m, chars e):
    type(t), name(n), flist(l), beg(s), mid(m), end(e) {}
};
extern std::vector<Func> function_block;			// int lowbit(int x) { return x&(-x); }

inline chars iTs(int i) {      // int转string
    std::stringstream ss;
    ss << i;
    chars num = ss.str();
    return num;
}

inline int sTi(chars s) {    // string转int
     std::istringstream iss(s);
     int num;
     iss >> num;
     return num;
}

inline double sTd(chars s) { // string转double
    std::istringstream iss(s);
     double num;
     iss >> num;
     return num;
}

inline int _alpha(char ch) {
    if (ch >= 'a' && ch <= 'z') return 1;
    if (ch >= 'A' && ch <= 'Z') return 1;
    if (ch == '_') return 1;
    return 0;
}

inline int digit(int ch) {
    if (ch >= '0' && ch <= '9') return 1;
    return 0;
}

inline int xdigit(int ch) {
    if (ch >= '0' && ch <= '9') return 1;
    if (ch >= 'a' && ch <= 'f') return 1;
    if (ch >= 'A' && ch <= 'F') return 1;
    return 0;
}

inline int digit_val(int ch) {
    if (ch >= '0' && ch <= '9') return ch-'0';
    if (ch >= 'a' && ch <= 'f') return ch-'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch-'A' + 10;
    return 0;
}

extern int preprocess();
extern int lex();
extern int parse();
extern int semantic();
