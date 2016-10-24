#include "cmm.h"

static string kIncludeFile[] = {
    "./",
    "./include/"
};

static int kIncludeSize = 2;
vector<Source> source;

static map<string, int> defines;
static int define_line;
static struct DefineDetail define_details[100000];

static string preprocess_out;
static string preprocess_in;

static int m;
static int error_line;
static string error_file;

struct PStack {
    int status_, pre_show_, now_show_;
    PStack() = default;
    PStack(int status, int pre_show, int now_show):
        status_(status), pre_show_(pre_show), now_show_(now_show) {}
};

static int SpeChar(char c) {
    if (c == ' ' || c == '\0' || c == '\n' || c == '\r' || c == 't')
        return 1;
    return 0;
}

static string ButSpeChar(string s) {
    int k = 0, l = s.length() - 1;
    while (l >= 0 && SpeChar(s[l])) l--;
    while (k <= l && SpeChar(s[k])) k++;
    if (k > l) return "";
    return s.substr(k, l - k + 1);
}

static int IfDef(string s) {
    s = ButSpeChar(s);
    if (defines.find(s) == defines.end()) return 0;
    return 1;
}

static string ButDefined(string s) {
    int i = 0, j = 0, k = 0, l = 0, u = 0, v = 0, len = s.length();
    for (i = 0; i < len; i++) {
        if ((i == 0 || SpeChar(s[i-1]) || s[i-1] == '(' || s[i-1] == '!')
                && (s.substr(i, 7) == "defined")) {
            u = i;
            i += 7;
            k = 0;
            while (i < len && (SpeChar(s[i]) || s[i] == '(')) {
                i++;
                if (s[i] == '(') k++;
            }
            j = i;
            while (i < len && !SpeChar(s[i])) i++;
            l = i - 1;
            if (j == l) return "0";
            while (i < len && (SpeChar(s[i]) || (k > 0 && s[i] == ')'))) {
                i++;
                if (s[i] == ')') k--;
            }

            if (IfDef(s.substr(j, l - j))) k = 1;
            else k = 0;
            for (v = u; v < i; v++) s[v] = ' ';
            s[u] = k + 48;
        }
    }
    
    return s;
}

static string StrReplaceNow(string tmp, vector<string> func_list) {
    string ret, val = define_details[defines[tmp]].val_;
    vector<string> para = define_details[defines[tmp]].para_;
    int i = 0, j = 0, k = 0, len = val.length();
    for (i = 0; i < len; i++) {
        if (isalpha(val[i]) || val[i] == '_') {
            j = i;
            while (i < len && (val[i] == '_' || isalnum(val[i]))) i++;
            auto str1 = val.substr(j, i - j);
            for (k = 0; k < (int)para.size(); k++) {
                if (para[k] == str1) {
                    ret += func_list[k];
                    break;
                }
            }
            if (k >= (int)para.size()) {
                ret += str1;
            }
            i--;
        }
        else {
            ret += val[i];
        }
    }

    return ret;
}

static string RepDefineFunction(string s, int len, int &i, string tmp) {
    while (i < len && SpeChar(s[i])) i++;

    if (s[i] != '(') {
        return tmp;
    }
    string str1 = "(";
    int k = i;
    int cnt = 1;
    while (i + 1 < len) {
        str1 += s[i];
        if (s[i] == '(') {
            cnt++;
        }
        else if (s[i] == ')') {
            cnt--;
            if (cnt == 0) {
                i++;
                break;
            }
        }
        i++;
    }
    if (cnt != 0) {
        return tmp;
    }

    vector<string> func_list;
    int j = 0;
    k = 1;
    for (j = 1; j < (int)str1.length(); j++) {
        if (str1[j] == ',' || j == (int)str1.length() - 1) {
            if (k == j) {
                return tmp;
            }
            func_list.push_back(str1.substr(k, j - k));
            j++;
            k = j;
        }
    }

    if (func_list.size() != define_details[defines[tmp]].para_.size()) {
        return tmp;
    }
    else {
        return StrReplaceNow(tmp, func_list);
    }
}

static string RepDefine(string s) {
    int i = 0, j = 0, len = s.length();
    string ret;
    for (i = 0; i < len; i++) {
        if (isalpha(s[i]) || s[i] == '_') {
            j = i;
            while (i < len && (isalnum(s[i]) || s[i] == '_')) i++;
            string tmp = s.substr(j, i - j);
            if (defines.find(tmp) == defines.end()) {
                ret += tmp;
            }
            else {
                if (define_details[defines[tmp]].para_.size() == 0) {
                    ret += define_details[defines[tmp]].val_;
                }
                else {
                    ret += RepDefineFunction(s, len, i, tmp);
                }
            }
            i--;
        }
        else {
            ret += s[i];
        }
    }
    if (ret != s) {
        return RepDefine(ret);
    }
    else {
        return ret;
    }
}

static int IsRightChar(string s, int i, int len) {
    if (i + 2 <= len) {
        string tmp = s.substr(i, 2);
        if (tmp == "&&" || tmp == "||" || tmp == ">=" || tmp == "<="
            || tmp == "==" || tmp == "!=" || tmp == "<<" || tmp == ">>")
            return 2;
    }
    if (i + 1 <= len) {
        if (s[i] == '!' || s[i] == '>' || s[i] == '<' || s[i] == '+'
            || s[i] == '-' || s[i] == '*' || s[i] == '/' || s[i] == '%'
            || s[i] == '~' || s[i] == '&' || s[i] == '^' || s[i] == '|') {
            return 1;
        }
    }
    return 0;
}

static int ExplodeCalStr(string s, string v[], int &sz) {
    int i = 0, k = 0, l = 0, len = s.length();
    sz = 0;

    for (i = 0; i < len; i++) {
        if (s[i] == '(' || s[i] == ')') {
            v[sz++] = s.substr(i, 1);
        }
        else if ((l = IsRightChar(s, i, len))) {
            v[sz++] = s.substr(i, l);
            i += l - 1;
        }
        else if (isdigit(s[i])) {
            k = i;
            while (i < len && isdigit(s[i])) i++;
            v[sz++] = s.substr(k, i - k);
            i--;
        }
        else if (SpeChar(s[i])) {
            
        }
        else {
            return 0;
        }
    }
    return 1;
}

static int GetLevel(string op) {
    if (op == "!" || op == "~") return 1;
    if (op == "*" || op == "/" || op == "%") return 2;
    if (op == "+" || op == "-") return 3;
    if (op == "<<" || op == ">>") return 4;
    if (op == ">" || op == ">=" || op == "<" || op == "<=") return 5;
    if (op == "==" || op == "!=") return 6;
    if (op == "&") return 7;
    if (op == "^") return 8;
    if (op == "|") return 9;
    if (op == "&&") return 10;
    if (op == "||") return 11;
    return 0;
}

static int Arg1OpArg2(string st[], int &szs, int level) {
    if (szs < 2) return 0;
    string arg1, op, arg2;
    int val1, val2, ret, get_level;

    op = st[szs - 2];
    get_level = GetLevel(op);
    if (get_level == 0) return 0;
    if (get_level > level) return 0;

    arg2 = st[szs - 1];
    if (!(isdigit(arg2[0]) || (arg2[0] == '-' && isdigit(arg2[1])))) {
        return 0;
    }
    val2 = StringToInt(arg2);

    if (op == "-") {
        ret = -val2;
        st[szs - 2] = IntToString(ret);
        szs--;
        return 1;
    }
    if (op == "!") {
        ret = !val2;
        st[szs - 2] = IntToString(ret);
        szs--;
        return 1;
    }
    if (op == "~") {
        ret = ~val2;
        st[szs - 2] = IntToString(ret);
        szs--;
        return 1;
    }

    if (szs < 3) return 0;
    arg1 = st[szs - 3];
    val1 = StringToInt(arg1);
    if (!(isdigit(arg1[0]) || (arg1[0] == '-' && isdigit(arg1[1])))) {
        return 0;
    }
    if (op == "+") {
        ret = StringToInt(arg1) + StringToInt(arg2);
    }
    else if (op == "-") {
        ret = StringToInt(arg1) - StringToInt(arg2);
    }
    else if (op == "*") {
        ret = StringToInt(arg1) * StringToInt(arg2);
    }
    else if (op == "/") {
        ret = StringToInt(arg1) / StringToInt(arg2);
    }
    else if (op == "%") {
        ret = StringToInt(arg1) % StringToInt(arg2);
    }
    else if (op == "<<") {
        ret = StringToInt(arg1) << StringToInt(arg2);
    }
    else if (op == ">>") {
        ret = StringToInt(arg1) >> StringToInt(arg2);
    }
    else if (op == "&") {
        ret = StringToInt(arg1) & StringToInt(arg2);
    }
    else if (op == "^") {
        ret = StringToInt(arg1) ^ StringToInt(arg2);
    }
    else if (op == "|") {
        ret = StringToInt(arg1) | StringToInt(arg2);
    }
    else if (op == ">") {
        ret = StringToInt(arg1) > StringToInt(arg2);
    }
    else if (op == ">=") {
        ret = StringToInt(arg1) >= StringToInt(arg2);
    }
    else if (op == "<") {
        ret = StringToInt(arg1) < StringToInt(arg2);
    }
    else if (op == "<=") {
        ret = StringToInt(arg1) <= StringToInt(arg2);
    }
    else if (op == "==") {
        ret = StringToInt(arg1) == StringToInt(arg2);
    }
    else if (op == "&&") {
        ret = StringToInt(arg1) && StringToInt(arg2);
    }
    else if (op == "||") {
        ret = StringToInt(arg1) || StringToInt(arg2);
    }
    st[szs - 3] = IntToString(ret);
    szs -= 2;
    return 1;
}

static int CalTop(string st[], int &szs, int level) {
    if (level > 1) {
        while (CalTop(st, szs, level - 1)) ;
    }
    return Arg1OpArg2(st, szs, level);
}

static int CalStack(string v[], int sz) {
    int i = 0, l = 0, szs = 0;
    string st[100];
    for (i = 0; i < sz; i++) {
        if (v[i] == "(" || isdigit(v[i][0])) {
            st[szs++] = v[i];
        }
        else if (v[i] == ")") {
            CalTop(st, szs, 11);
            if (szs >= 2 && st[szs - 2] != "(") return 0;
            if (szs >= 2) {
                st[szs - 2] = st[szs - 1];
                szs--;
            }
        }
        else {
            l = GetLevel(v[i]);
            CalTop(st, szs, l);
            st[szs++] = v[i];
        }
    }

    CalTop(st, szs, 11);
    if (szs == 1 && StringToInt(st[szs - 1]) != 0) return 1;
    return 0;
}

static int CalIfExpr(string expr) {
    string v[100];
    int ret, sz;
    expr = ButDefined(expr);
    expr = RepDefine(expr);
    ret = ExplodeCalStr(expr, v, sz);
    if (ret == 0) return ret;
    ret = CalStack(v, sz);
    return ret;
} 

static int ExplodeDefStr(string s, string v[], int e[], int &sz) {
    int i = 0, k = 0, l = 0, len = s.length();
    sz = 0;

    for (i = 0; i < len; i++) {
        if ((l = IsRightChar(s, i, len))) {
            e[sz] = i + l;
            v[sz++] = s.substr(i, l);
            i += l - 1;
        }
        else if (isdigit(s[i])) {
            k = i;
            while (i < len && isdigit(s[i])) i++;
            if (sz > 0 && v[sz - 1] == "-") {
                e[sz - 1] = i;
                v[sz - 1] += s.substr(k, i - k);
            }
            else {
                e[sz] = i;
                v[sz++] = s.substr(k, i - k);
            }
            i--;
        }
        else if (isalpha(s[i]) || s[i] == '_') {
            v[sz] = "";
            while (i < len && (isalnum(s[i]) || s[i] == '_')) {
                v[sz] += s[i];
                i++;
            }
            e[sz++] = i;
            i--;
        }
        else if (i + 3 <= len && s.substr(i, 3) == "...") {
            e[sz] = i + 3;
            v[sz++] = s.substr(i, 3);
            i += 2;
        }
        else if (s[i] == '(' || s[i] == ')' || s[i] == ',' || s[i] == '='
                || s[i] == '.' || s[i] == '&' || s[i] == '|'
                || s[i] == '[' || s[i] == ']' || s[i] == '?'
                || s[i] == ':') {
            e[sz] = i + 1;
            v[sz++] = s.substr(i, 1);
        }
        else if (s[i] == '{' || s[i] == '}' || s[i] == '#'
                || s[i] == '"' || s[i] == '\\' || s[i] == ';') {
            e[sz] = i + 1;
            v[sz++] = s.substr(i, 1);
        }
        else {
            return 0;
        }
    }
    return 1;
}
