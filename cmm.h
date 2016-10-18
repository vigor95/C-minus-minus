#ifndef __CMM_H__

#define __CMM_H__

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <stack>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>

using namespace std;

#define __CMM__ 1

#include VERSION "CMM 1.0"

#include <sys/wait.h>

extern int MAKE_ACTION_INIT;

#define MAX_CODE_LEN 100000

#define null "null"
#define MAX_GRAMMAR_NUMBER              230
#define MAX_GRAMMAR_STATEMENT_LENGTH    10
#define MAX_ALL_SYMBOL_NUMBER           170
#define MAX_ITEMS_NUMBER                3000

#define MAX_DEPTH   100000

struct Source {
    string code_;
    string file_;
    int line_;
    Source() {}
    Source(string code, string file, int line):
        code_(code), file_(file), line_(line) {}
};

extern vector<Source> source;

struct DefineDetail {
    string key_, val_;
    vector<string> para_;
    DefineDetail() {}
    DefineDetail(string key, string val): key_(key), val_(val) {}
    DefineDetail(string key, string val, vector<string> para):
        key_(key), val_(val), para_(para) {}
};

extern int OUT_OTHER_FILE;

extern string filename;

extern string assemblyname;

struct Token {
    string type_, val_;
    int line_;
    Token() {}
    Token(string type, string val, int line):
        type_(type), val_(val), line_(line) {}
};

extern vector<Token> tokens;

struct Type {
    string name_, type_;
    int array_, pointer_;
    vector<int> index_;
    Type() {}
    Type(string name, string type, int arr, int ptr, vector<int> index):
        name_(name), type_(type), array_(arr),
        pointer_(ptr), index_(index) {}
};

struct ParseTree {
    int statement_;
    Type type_;
    string val_;
    vector<ParseTree*> tree_;
    int line_;
};

extern ParseTree *root_of_parsetree;

struct Expression {
    string label_, op_, arg1_, arg2_, result_;
    int out_struct_function_;
    vector<string> parameter_;
    Expression() {}

    Expression(string op, string arg1, string arg2, string result):
        op_(op), arg1_(arg1), arg2_(arg2), result_(result) {}

    Expression(string label, string op, string arg1, string arg2,
        string result): label_(label), op_(op),
        arg1_(arg1), arg2_(arg2), result_(result) {}
};

extern vector<Expression> semantic_expression;

struct Function {
    Type type_;
    string name_;
    vector<string> list_;
    string start_, mid_, end_;
    Function() {}

    Function(Type t, string n, vector<string> l):
        type_(t), name_(n), list_(l) {}

    Function(Type t, string n, vector<string> l, string s,
        string m, string e):
        type_(t), name_(n), list_(l), start_(s), mid_(m), end_(e) {}
};

extern vector<Function> function_block;

inline string IntToString(int i) {
    stringstream ss;
    ss << i;
    return ss.str();
}

inline int StringToInt(string s) {
    istringstream iss(s);
    int num;
    iss >> num;
    return num;
}

inline double StringToDouble(string s) {
    istringstream iss(s);
    double num;
    iss >> num;
    return num;
}

extern int preprocess_main();
extern int lexical_main();
extern int parse_main();
extern int semantic_main();
extern int assembly_main();

#endif
