#include "cic.h"
#include "gram.init"

ParseTree *subtr;
ParseTree *root;

struct Item {
    int gid, pos, fwd;
    Item() = default;
    Item(int g, int p, int f): gid(g), pos(p), fwd(f) {}
    bool operator<(Item other) const {
        return gid < other.gid || gid == other.gid && pos < other.pos
            || gid == other.gid && pos == other.pos && fwd < other.fwd;
    }
};

struct Action {
    int to, sta;
    Action() = default;
    Action(int t, int s): to(t), sta(s) {}
};

struct Go {
    int from, symbol, to;
    Go() = default;
    Go(int f, int s, int t): from(f), symbol(s), to(t) {}
};

struct SStack {
    int state;
    chars val;
    svec slist;
    ParseTree *subtr;
    SStack() = default;
    SStack(int s, chars v): state(s), val(v) {}
};

struct SType_name {
    chars name;
    SType_name() = default;
    SType_name(chars n): name(n) {}
};

static int gram_num, item_num;

static svec gram[MAX_GRAM_NUM];
static svec terms, nterms;
static svec symbols;
static std::map<chars, int> sym_id;

static std::vector<int> first[MAX_ALL_SYMBOL_NUM];

static ivec vgram[MAX_GRAM_NUM];
static ivec vfirst[MAX_ALL_SYMBOL_NUM];
static ivec
    vexp_first[MAX_GRAM_NUM][MAX_GRAM_STMT_LEN][MAX_ALL_SYMBOL_NUM];
static ivec vsymbol[MAX_GRAM_NUM];

static std::vector<Item> items[MAX_ITEM_NUM];
static std::vector<Go> gos;
static ivec X[MAX_ALL_SYMBOL_NUM];
static int now,
    check[MAX_GRAM_NUM][MAX_GRAM_STMT_LEN][MAX_ALL_SYMBOL_NUM];
static int appear[MAX_GRAM_NUM][MAX_GRAM_STMT_LEN][MAX_ALL_SYMBOL_NUM];

static std::vector<Action> actions[MAX_ITEM_NUM];

static std::vector<SStack> stk;
static std::vector<SType_name> type_name;

static chars action_name, number, ident, strlit, parse_out;

static void inputGram() {
#include "./gram.init"
    std::istringstream is;
    chars tmp, str;
    int i, j, k, ok;
    gram_num = 1;
    for (i = 0;; i++) {
        str = gram_init[i];
        if (str.length() == 0) break;
        is.clear();
        is.str(str);
        ok = 0;
        while (is >> tmp) {
            gram[gram_num].push_back(tmp);
            ok = 1;
        }
        if (ok) gram_num++;
    }
    gram[0].clear();
    gram[0].push_back("<S>");
    gram[0].push_back("::=");
    gram[0].push_back(gram[1][0]);
    sym_id.clear();
    terms.clear();
    nterms.clear();
    symbols.clear();
    for (i = 0; i < gram_num; i++) {
        for (j = 0; j < gram[i].size(); j++) {
            k = gram[i][j].length();
            if (gram[i][j][0] == '<' && gram[i][j][k-1] == '>' && k > 2
                && gram[i][j] != "<ident>" && gram[i][j] != "<number>"
                && sym_id.find(gram[i][j]) == sym_id.end()) {
                        nterms.push_back(gram[i][j]);
                        sym_id[gram[i][j]] = 1;
            }
            else if (gram[i][j] != "::=" &&
                    sym_id.find(gram[i][j]) == sym_id.end()) {
                terms.push_back(gram[i][j]);
                sym_id[gram[i][j]] = 1;
            }
        }
    }
    k = 1;
    for (i = 0; i < terms.size(); i++) if (terms[i] == "null") k = 0;
    if (k) terms.push_back("null");
    terms.push_back("$");

    sym_id.clear();
    k = 0;
    for (i = 0; i < terms.size(); i++) {
        sym_id[terms[i]] = k++;
        symbols.push_back(terms[i]);
    }
    for (i = 0; i < nterms.size(); i++) {
        sym_id[nterms[i]] = k++;
        symbols.push_back(nterms[i]);
    }
}

static void makeFirst() {
    int i, j, k, ok;
    for (i = 0; i < symbols.size(); i++) {
        first[i].clear();
        for (j = 0; j < symbols.size(); j++) first[i].push_back(0);
    }
    for (i = 0; i < terms.size(); i++) first[i][i] = 1;
    ok = 1;
    while (ok) {
        ok = 0;
        for (i = 0; i < gram_num; i++) {
            if (gram[i].size() == 3 && gram[i][2] == "null") {
                if (first[sym_id[gram[i][0]]][sym_id["null"]] == 0) {
                    ok = 1;
                    first[sym_id[gram[i][0]]][sym_id["null"]] = 1;
                }
                continue;
            }
            k = 2;
            while (1) {
                for (j = 0; j < symbols.size(); j++) {
                    if (first[sym_id[gram[i][k]]][j] == 1) {
                        if (first[sym_id[gram[i][0]]][j] == 0) {
                            ok = 1;
                            first[sym_id[gram[i][0]]][j] = 1;
                        }
                    }
                }
                if (first[sym_id[gram[i][k]]][sym_id["null"]] == 0)
                    break;
                k++;
                if (k >= gram[i].size()) break;
            }
        }
    }
    for (i = 0; i < terms.size(); i++) first[i][i] = 1;
}

static void makeVinit() {
    int i, j;
    for (i = 0; i < symbols.size(); i++) {
        vfirst[i].clear();
        for (j = 0; j < symbols.size(); j++)
            if (first[i][j] == 1) vfirst[i].push_back(j);
    }
    for (i = 0; i < symbols.size(); i++) {
        vgram[sym_id[symbols[i]]].clear();
        for (j = 0; j < gram_num; j++)
            if (gram[j][0] == symbols[i])
                vgram[sym_id[symbols[i]]].push_back(j);
    }
    for (i = 0; i < gram_num; i++) {
        vsymbol[i].clear();
        for (j = 0; j < gram[i].size(); j++) {
            if (sym_id.find(gram[i][j]) == sym_id.end())
                vsymbol[i].push_back(-1);
            else
                vsymbol[i].push_back(sym_id[gram[i][j]]);
        }
    }
}

static int tmpInItems(std::vector<Item> tmp) {
    int i, j;
    for (i = 0; i < item_num; i++)
        if (items[i].size() == tmp.size()) {
            for (j = 0; j < items[i].size(); j++)
                if (items[i][j].gid != tmp[j].gid ||
                        items[i][j].pos != tmp[j].pos ||
                        items[i][j].fwd != tmp[j].fwd)
                    break;
            if (j == items[i].size()) return i;
        }
    return -1;
}

static void expandItem(Item tmp, ivec &exp_first) {
    int pos = tmp.pos, i, g;
    if (appear[tmp.gid][tmp.pos][tmp.fwd] == 1) {
        exp_first = vexp_first[tmp.gid][tmp.pos][tmp.fwd];
        return;
    }
    exp_first.clear();
    while (1) {
        if (pos + 1 >= gram[tmp.gid].size()) {
            exp_first.push_back(tmp.fwd);
            break;
        }
        g = vsymbol[tmp.gid][pos+1];
        if (g < terms.size()) {
            exp_first.push_back(g);
            break;
        }
        else {
            for (i = vfirst[g].size() - 1; i >= 0; i--)
                exp_first.push_back(vfirst[g][i]);
            if (first[vsymbol[tmp.gid][pos]][sym_id["null"]] == 1)
                pos++;
            else break;
        }
    }
    appear[tmp.gid][tmp.pos][tmp.fwd] = 1;
    vexp_first[tmp.gid][tmp.pos][tmp.fwd] = exp_first;
}

static void closure(std::vector<Item> &item) {
    int i, j, jj, k, g;
    ivec exp_first;
    for (i = 0; i < item.size(); i++)
        if (item[i].pos + 1 <= gram[item[i].gid].size()) {
            g = vsymbol[item[i].gid][item[i].pos];
            for (jj = vgram[g].size() - 1; jj >= 0; jj--) {
                j = vgram[g][jj];
                expandItem(item[i], exp_first);
                for (k = exp_first.size() - 1; k >= 0; k--)
                    if (check[j][2][exp_first[k]] != now) {
                        item.push_back(Item(j, 2, exp_first[k]));
                        check[j][2][exp_first[k]] = now;
                    }
            }
    }
}

static void Goto(int i, int j, std::vector<Item> &tmp) {
    int k, kk;
    tmp.clear();
    now++;
    for (kk = X[j].size() - 1; kk >= 0; kk--) {
        k = X[j][kk];
        tmp.push_back(items[i][k]);
        tmp[tmp.size() - 1].pos++;
        check[items[i][k].gid][items[i][k].pos+1][items[i][k].fwd] = now;
    }
}

static void makeItems() {
    int i, j, next_state;
    std::vector<Item> tmp;
    int len = 0;
    for (i = 0; i < gram_num; i++)
        len = std::max(len, (int)gram[i].size());
    for (i = 0; i < gram_num; i++) {
        for (j = 0; j <= len; j++)
            for (int k = 0; k < symbols.size(); k++) {
                check[i][j][k] = -1;
                appear[i][j][k] = 0;
            }
    }
    now = -1;
    item_num = 1;
    items[0].clear();
    now++;
    items[0].push_back(Item(0, 2, sym_id["$"]));
    check[0][2][sym_id["$"]] = now;
    closure(items[0]);
    std::sort(items[0].begin(), items[0].end());
    gos.clear();
    for (i = 0; i < item_num; i++) {
        for (j = symbols.size() - 1; j >= 0; j--) X[j].clear();
        for (j = items[i].size() - 1; j >= 0; j--)
            if (items[i][j].pos < gram[items[i][j].gid].size())
                X[vsymbol[items[i][j].gid][items[i][j].pos]].push_back(j);
        for (j = symbols.size() - 1; j >= 0; j--) if (X[j].size()) {
            Goto(i, j, tmp);
            if (tmp.size() == 0) continue;
            closure(tmp);
            std::sort(tmp.begin(), tmp.end());
            next_state = tmpInItems(tmp);
            if (next_state == -1) {
                items[item_num] = tmp;
                item_num++;
                next_state = item_num - 1;
            }
            gos.push_back(Go(i, j, next_state));
        }
    }
}

static void makeAction() {
    size_t i, j;
    for (i = 0; i < item_num; i++) {
        actions[i].clear();
        for (j = 0; j < symbols.size(); j++)
            actions[j].push_back(Action(-1, -1));
    }
    for (i = 0; i < item_num; i++) {
        for (j = 0; j < items[i].size(); j++)
            if (items[i][j].pos == gram[items[i][j].gid].size()
                || (items[i][j].pos + 1 == gram[items[i][j].gid].size()
                && gram[items[i][j].gid][items[i][j].pos] == "null")) {
                actions[i][items[i][j].fwd].sta = 1;
                actions[i][items[i][j].fwd].to = items[i][j].gid;
            }
    }
    for (i = 0; i < gos.size(); i++) {
        actions[gos[i].from][gos[i].symbol].to = gos[i].to;
        actions[gos[i].from][gos[i].symbol].sta = 0;
    }
    auto fp = fopen(action_name.c_str(), "w");
    fprintf(fp, "int action_row = %d, action_col = %d;\r\n",
            item_num, (int)(symbols.size()));
    fprintf(fp, "int action_init[%d][%d] = {\r\n", item_num,
            2 * (int)(symbols.size()));
    for (i = 0; i < item_num; i++) {
        fprintf(fp, "{");
        for (j = 0; j < symbols.size(); j++) {
            if (j > 0) fprintf(fp, ", ");
            fprintf(fp, "%d, %d", actions[i][j].sta, actions[i][j].to);
        }
        fprintf(fp, "}, \r\n");
    }
    fprintf(fp, "};\r\n");
    fclose(fp);
}

static void inputAction() {
#include "./action.init"
    int i, j, k, l, row, col;
    row = action_row;
    col = action_col;
    for (i = 0; i < row; i++) {
        actions[i].clear();
        for (j = 0; j < col; j++) {
            k = action_init[i][j+j];
            l = action_init[i][j+j+1];
            actions[i].push_back(Action(l, k));
        }
    }
}

static void inputCode() {
    word.push_back(Word("<keyword>", "$", source.size()));
    source.push_back(Source("", filename, source.size()));
}

static FILE *fp;

static chars makeSolve() {
    int i, j;
    int id, stmt, len, cnt = 1, top = 1, to;
    stk.clear();
    stk.push_back(SStack(0, ""));
    type_name.clear();
    root = NULL;

    for (i = 0; i < word.size(); i++) {
        while (stk.size() < top + 1) stk.push_back(SStack(0, ""));
        id = sym_id[word[i].val];
        for (j = 0; j < type_name.size(); j++)
            if (word[i].val == type_name[j].name)
                if (!i || word[i-1].val != "struct") {
                    id = sym_id["TYPE_NAME"];
                    word[i].type = "<keyword>";
                }
        if (word[i].type == "<ident>") id = sym_id[ident];
        if (word[i].type == "<number>") id = sym_id[number];
        if (word[i].type == "<string>") id = sym_id[strlit];

        to = actions[stk[top-1].state][id].to;
        switch (actions[stk[top-1].state][id].sta) {
            case 0:
                stk[top].state = to;
                stk[top].val = word[i].val;
                stk[top].slist.clear();
                stk[top].slist.push_back(stk[top].val);

                stk[top].subtr = new ParseTree();
                stk[top].subtr->stmt = -1;
                stk[top].subtr->val = stk[top].val;
                stk[top].subtr->line = word[i].line;
                top++;
                if (top > 3 && stk[top-3].val == "struct"
                        && stk[top-1].val == "{")
                    type_name.push_back(SType_name(stk[top-2].val));
                break;
            case 1:
                stmt = to;
                len = gram[stmt].size();
                if (gram[stmt][len-1] == "null") len--;
                subtr = new ParseTree();
                subtr->stmt = stmt;
                subtr->val = "";
                subtr->tree.clear();
                root->line = stk[top-len+2].subtr->line;
                for (j = top-len+2; j < top; j++)
                    subtr->tree.push_back(stk[j].subtr);
                top = top - len + 3;
                stk[top-1].state = sym_id[gram[stmt][0]];
                stk[top-1].subtr = subtr;
                if (stmt == 44)
                    type_name.push_back(SType_name(stk[top].val));
                if (stmt == 12 &&
                        stk[top-1].val.substr(0, 7) == "typedef")
                    for (j = 0; j < stk[top].slist.size(); j++)
                        type_name.push_back(SType_name(stk[top].slist[j]));
                if (stmt == 65)
                    stk[top-1].slist.push_back(stk[top+1].val);
                i--;
                if (stk[top-1].state == sym_id["<S>"]) {
                    root = stk[top-1].subtr;
                    return "parse accept!";
                }

                if (top > 1 && stk[top-1].state >= terms.size() &&
                actions[stk[top-2].state][stk[top-1].state].sta == 0) {
                    stk[top-1].state =
                        actions[stk[top-2].state][stk[top-1].state].to;
                }
                else {
                    return "parse error: " + source[word[i].line].file
                        + ": " + iTs(source[word[i].line].line)
                        + ": " + word[i].val;
                }
                break;
            case -1:
                return "parse error: " + source[word[i].line].file
                    + ": " + iTs(source[word[i].line].line) + ": "
                    + word[i].val;
                break;
        }
    }
    if (top == 1 && stk[0].state == sym_id["<S>"]) {
        root = stk[top-1].subtr;
        return "parse accept!";
    }
    else {
        return "parse error: " + filename + ": at end of file";
    }
}

FILE *out;

void printTree(ParseTree *r, int dep) {
    for (int i = 0; i < dep; i++) fprintf(out, " ");
    chars s;
    if (r->stmt >= 0) {
        for (int i = 0; gram_init[r->stmt][i] != '>'; i++)
            s += gram_init[r->stmt][i];
        s += '>';
    }
    else s = r->val;
    fprintf(out, "%s (%d)\n", s.c_str(), r->line);
    for (size_t i = 0; i < r->tree.size(); i++)
        printTree(r->tree[i], dep + 1);
}

int parse() {
    parse_out = "parse.out";
    action_name = "action.init";

    number = "constant";
    ident = "identifier";
    strlit = "string_literal";

    inputGram();
    //makeFirst();
    //makeVinit();
    //makeItems();
    //makeAction();

    inputAction();
    inputCode();
    fp = fopen(parse_out.c_str(), "w");
    chars ss = makeSolve();
    std::cout << "solve" << std::endl;
    int ok = 0;
    if (ss != "parse accept!") {
        std::cout << ss << std::endl;
        ok = 1;
    }
    /*out = fopen("2.out", "w");
    ParseTree *r = subtr;
    printTree(r, 0);*/
    return ok;
}
