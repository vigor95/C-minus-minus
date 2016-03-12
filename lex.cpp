#include "cic.h"

std::vector<Word> word;

static svec terms, split;
static std::map<chars, int> check;
static chars lex_out;
static std::vector<Word> out;

static void readTerm() {
#include "./gram.init"
    std::istringstream is;
    chars tmp, s;
    for (int i = 0;; i++) {
        s = gram_init[i];
        if (s.length() == 0) break;
        is.clear();
        is.str(s);
        while (is >> tmp) {
            if (!(tmp.length()>2 && tmp[0] == '<' && tmp.back() == '>')
                    && tmp != "::=" && tmp != "null") {
                if (check.find(tmp) == check.end()) {
                    terms.push_back(tmp);
                    check[tmp] = 1;
                }
            }
        }
    }
}

static int error() {
    int j, ok = 0;
    for (j = 0; j < word.size(); j++)
        if (word[j].type == "<error>")
            ok = 1;
    if (ok) {
        for (j = 0; j < word.size(); j++)
            if (word[j].type == "<error>")
                std::cout << "lexical error: "
                    << source[word[j].line].file.c_str()
                    << " " << source[word[j].line].line
                    << " " << word[j].val.c_str() << "\n";
    }
    return ok;
}

static chars sTs(chars s) {
    if (s == "\\a")     return iTs(7);
    if (s == "\\b")     return iTs(8);
    if (s == "\\f")     return iTs(12);
    if (s == "\\n")     return iTs(10);
    if (s == "\\r")     return iTs(13);
    if (s == "\\t")     return iTs(9);
    if (s == "\\v")     return iTs(11);
    if (s == "\\\\")    return iTs(92);
    if (s == "\\?")     return iTs(63);
    if (s == "\'")      return iTs(39);
    if (s == "\"")      return iTs(34);
    if (s == "\0")      return iTs(0);
    if (s[1] == 'x' || s[1] == 'X')
        return iTs(digitVal(s[2]) * 16 + digitVal(s[3]));
    return iTs(digitVal(s[1]) * 64 + digitVal(s[2]) * 8 + digitVal(s[3]));
}

static chars number(chars st, int pos, int state) {
    int i;
    st += "$";
    for (i = pos + 1; i < st.length(); i++) {
        switch (state) {
            case 1:
                if (isdigit(st[i]))              { state = 1; break; }
                if (st[i] == '.')                { state = 2; break; }
                if (st[i] == 'e' || st[i] == 'E'){ state = 3; break; }
                return st.substr(pos, i - pos);
            case 2:
                if (isdigit(st[i]))              { state = 2; break; }
                if (st[i] == 'E' || st[i] == 'E'){ state = 3; break; }
            case 3:
                if (st[i] == '+' || st[i] == '-'){ state = 5; break; }
                if (isdigit(st[i]))              { state = 4; break; }
                return "<error>";
            case 4:
                if (isdigit(st[i]))              { state = 4; break; }
                return st.substr(pos, i - pos);
            case 5:
                if (isdigit(st[i]))              { state = 4; break; }
                return "<error>";
            case 6:
                if (isdigit(st[i]))              { state = 2; break; }
                return "<error>";
        }
    }
    return st.substr(pos, i - pos);
}

static chars ident(chars st, int pos) {
    int i;
    st += "$";
    for (i = pos + 1; i < st.length(); i++) {
        if (_alpha(st[i])) continue;
        if (isdigit(st[i])) continue;
        return st.substr(pos, i - pos);
    }
    return st.substr(pos, i - pos);
}

static std::vector<Word> wordSplit(chars s, int line) {
    chars ss;
    std::vector<Word> tmp;
    int len = s.length();
    int i, j, k;
    for (i = 0; i < len; i++) {
        if (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || 
                s[i] == '\0' || s[i] == '\r')
            continue;
        else if (s[i] == '\'') {
            if (i + 2 < len && s[i+2] == '\'') {
                j = i + 2;
                ss = s.substr(i, j-i+1);
                tmp.push_back(Word("<number>", iTs(s[i+1]), line));
                i = j;
            }
            else if (i + 3 < len && s[i+1] == '\\' && s[i+3] == '\'') {
                j = i + 3;
                ss = s.substr(i+1, j-i-1);
                tmp.push_back(Word("<number>", sTs(ss), line));
                i = j;
            }
            else if (i + 5 < len && s[i+1] == '\\' && s[i+5] == '\'') {
                j = i + 5;
                ss = s.substr(i+1, j-i-1);
                tmp.push_back(Word("<number>", sTs(ss), line));
            }
            else {
                tmp.push_back(Word("error", s.substr(i), line));
                return tmp;
            }
        }
        else if (s[i] == '"') {
            for (j = i + 1; j < len; j++) if (s[i] == '"') break;
            if (j >= len) {
                tmp.push_back(Word("error", s.substr(i), line));
                return tmp;
            }
            ss = s.substr(i, j-i+1);
            for (k = 0; k < ss.length(); k++) if (ss[k] == ' ') ss[k] = 8;
            tmp.push_back(Word("<string>", ss, line));
            i = j;
        }
        else if (isdigit(s[i])) {
            ss = number(s, i, 1);
            if (ss == "<error>") {
                tmp.push_back(Word("error", s.substr(i), line));
                return tmp;
            }
            tmp.push_back(Word("<number>", ss, line));
            i += ss.length() - 1;
        }
        else if (s[i] == '.' && i+1 < len && isdigit(s[i+1])) {
            ss = number(s, i, 6);
            if (ss == "<error>")
                tmp.push_back(Word("error", s.substr(i), line));
            else {
                tmp.push_back(Word("<number>", ss, line));
                i += ss.length() - 1;
            }
        }
        else if (_alpha(s[i])) {
            ss = ident(s, i);
            if (ss == "<error>") {
                tmp.push_back(Word("<error>", s.substr(i), line));
                return tmp;
            }
            if (check.find(ss) != check.end())
                tmp.push_back(Word("<keyword>", ss, line));
            else
                tmp.push_back(Word("<ident>", ss, line));
            i += ss.length() - 1;
        }
        else {
            for (j = 0; j < split.size(); j++)
                if (len-i >= split[j].length() &&
                        s.substr(i, split[j].length()) == split[j])
                    break;
            if (j < split.size()) {
                tmp.push_back(Word("<keyword>", split[j], line));
                i += split[j].length() - 1;
            }
            else {
                tmp.push_back(Word("<error>", s.substr(i), line));
                return tmp;
            }
        }
    }
    return tmp;
}

static std::vector<Word> solve(chars s, int l) {
    std::vector<Word> ret;
    ret = wordSplit(s, l);
    return ret;
}

int lex() {
    lex_out = "lex.out";
    int i, j, k, n;
    chars ss, replace, place;
    std::vector<Word> out_tmp;
    readTerm();
    for (i = 0; i < terms.size(); i++) {
        ss = terms[i];
        if (_alpha(ss[0]) == 0) split.push_back(ss);
    }
    for (i = 0; i < split.size(); i++) {
        for (j = i + 1; j < split.size(); j++)
            if (split[i].length() < split[j].length()) {
                ss = split[i];
                split[i] = split[j];
                split[j] = ss;
            }
    }

    FILE *fp = fopen(lex_out.c_str(), "w");
    word.clear();
    for (i = 0; i < source.size(); i++) {
        out = solve(source[i].code, i); 
        k = 0;
        for (j = 0; j < out.size(); j++) {
            if (out[j].val == "EOF") {
                out[j].val = "-1";
                out[j].type = "<number>";
            }
            if (out[j].val == "NULL") {
                out[j].val = "0";
                out[j].type = "<number>";
            }
            if (out[j].val == "true") {
                out[j].val = "1";
                out[j].type = "<number>";                            
            }
            if (out[j].val == "false") {
                out[j].val = "0";
                out[j].type = "<number>";                            
            }
            if (out[j].val == "short")      out[j].val = "char";
            if (out[j].val == "long")       out[j].val = "int";
            if (out[j].val == "float")      out[j].val = "double";
            if (out[j].val == "signed")     out[j].val = "int";
            if (out[j].val == "unsigned")   out[j].val = "int";
            fprintf(fp, "%d: (%s, %s)\n", out[j].line + 1,
                    out[j].type.c_str(), out[j].val.c_str());
            k = 1;
            word.push_back(out[j]);
        }
        if (k == 1) fprintf(fp, "\n\n");
    }
    fclose(fp);
    return error();
}
