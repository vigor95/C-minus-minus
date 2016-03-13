#include "cic.h"
#include "gram.init"

ParseTree *subtr;

ParseTree *headSubtr;

struct Item {
    int gid, pos, fwd;
    Item() {}
    Item(int g, int p, int f): gid(g), pos(p), fwd(f) {}
    bool operator < (Item other) const {
        return gid < other.gid || gid == other.gid && pos <
            other.pos || gid == other.gid && pos == other.pos &&
            fwd < other.fwd;
    }
} ;

struct Action {
    int to, sta;
    Action() {}
    Action(int t, int s): to(t), sta(s) {}
};

struct Go {
    int from, symbol, to;
    Go() {}
    Go(int f, int s, int t): from(f), symbol(s), to(t) {}
};

struct SStack {
    int state;
    chars val;
    svec slist;
    ParseTree *subtr;
    SStack() {}
    SStack(int s, chars v): state(s), val(v) {}
};

struct SType_name {
    chars name;
    SType_name() {}
    SType_name(chars n): name(n) {}
};


static int gram_num, item_num;

static svec gram[MAX_GRAM_NUM];
// gram[i] = stmt(<program> ::= <block> .)
static svec terms, nterms;					
// terms = int < ; ...  nterms = <program> <block> ...
static svec symbols;	// symbols = terms + nterms
static std::map<chars, int> sid;	// sid[ all_symbol[i] ] = i;

// init in MAKE_FIRST
static ivec first[MAX_ALL_SYMBOL_NUM];				
// if (symbols[j] in symbols[i]'s first) first[i][j] = 1

// init in MAKE_VINIT
static ivec vgram[MAX_GRAM_NUM];
// vgram[i] = list(j) where sid[gram[j][0]] = i;
static ivec vfirst[MAX_ALL_SYMBOL_NUM];	
// vfirst[i] = list(j) where fitst[i][j] == 1
static ivec vexp_first[MAX_GRAM_NUM][MAX_GRAM_STMT_LEN][MAX_ALL_SYMBOL_NUM];	// if (expand_item(Item)) vexp_first[Item] = exp_first;
static ivec vsymbol[MAX_GRAM_NUM];		// vsymbol[i][j] = sid[gram[i][j]]

// init in MAKE_ITEMS
static std::vector<Item> items[MAX_ITEM_NUM];					// items[i] = a item
static std::vector<Go> go;											// Action => go[i] = (symbol==end_symbol?push:goto)
static ivec X[MAX_ALL_SYMBOL_NUM];					// i = all_symbol; X[i] = list(j) where sid[ gram[ items[][j].gid ][ items[][j].pos ] ] = i;
static int now, check[MAX_GRAM_NUM][MAX_GRAM_STMT_LEN][MAX_ALL_SYMBOL_NUM];	// now = making items[id];  check[i][j][k] = (Item(i,j,k)==appear in items[id])?(=now):(!=now)
static int appear[MAX_GRAM_NUM][MAX_GRAM_STMT_LEN][MAX_ALL_SYMBOL_NUM];		// boo[i][j][k] = (Item(i,j,k)==appear before)1:0

// init in MAKE_ACTION
static std::vector<Action> action[MAX_ITEM_NUM];				// action[i][j].sta = 0 => push; action[i][j].sta = 1 && symbol==end_symbol => pop; action[i][j].sta = 1 && symbol==exp_symbol => goto;

// used in MAKE_SOLVE
static std::vector<SStack> sstack;									// sstack[top++] = push  sstack[top-len+3 -1] = pop; sstack[top-1] = goto
static std::vector<SType_name> type_name;

// init in main   used to choose grammar
static chars action_name, number, ident, strlit, parse_out;

static void inputGram() {
    std::istringstream is;

    chars tmp, s;
    int i, j, k, ok;

    gram_num = 1;

    for (i = 0; ; i++) {
        s = gram_init[i];
        if (s.length() == 0) break;
        is.clear();
        is.str(s);
        ok = 0;
        while (is >> tmp) {
            gram[gram_num].push_back(tmp);
            ok = 1;
        }
        if (ok == 1) gram_num++;
    }

    gram[0].clear();
    gram[0].push_back("<S>");
    gram[0].push_back("::=");
    gram[0].push_back(gram[1][0]);

    sid.clear();
    terms.clear();
    nterms.clear();
    symbols.clear();

    for (i = 0; i < gram_num; i++) {
        for (j = 0; j < gram[i].size(); j++) {
            k = gram[i][j].length();
            if ((gram[i][j][0] == '<' && gram[i][j][k-1] == '>' && k > 2)
                    && gram[i][j] != "<ident>" && gram[i][j]
                    != "<number>" && sid.find(gram[i][j]) == sid.end()) {
                nterms.push_back(gram[i][j]);
                sid[gram[i][j]] = 1;
            }
            else if (gram[i][j] != "::=" &&
                    sid.find(gram[i][j]) == sid.end()) {
                terms.push_back(gram[i][j]);
                sid[gram[i][j]] = 1;
            }
        }
    }

    k = 1;
    for (i = 0; i < terms.size(); i++) if (terms[i] == null) k = 0;
    if (k == 1) terms.push_back(null);
    terms.push_back("$");

    sid.clear();
    k = 0;
    for (i = 0; i < terms.size(); i++)
    { sid[terms[i]] = k++; symbols.push_back(terms[i]); }
    for (i = 0; i < nterms.size(); i++)
    { sid[nterms[i]] = k++; symbols.push_back(nterms[i]); }
}

static void makeFirst() {
    int i, j, k, ok;
    for (i = 0; i < symbols.size(); i++) {
        first[i].clear();
        for (j = 0; j< symbols.size(); j++) first[i].push_back(0);
    }

    // 终结符的first集等于本身
    for (i = 0; i < terms.size(); i++) first[i][i] = 1;

    ok = 1;
    while (ok == 1) {
        ok = 0;
        for (i = 0; i < gram_num; i++) {
            if (gram[i].size() == 3 && gram[i][2] == null) {
                if (first[sid[gram[i][0]]][sid[null]] == 0) {
                    ok = 1;
                    first[sid[gram[i][0]]][sid[null]] = 1;
                }
                continue;
            }

            k = 2;
            while (true) {
                for (j = 0; j < symbols.size(); j++) {
                    if (first[sid[gram[i][k]]][j] == 1) {
                        // 若b是<B>的头符号，那么b也是<A>的头符号
                        if (first[ sid[gram[i][0]] ][j] == 0) {
                            ok = 1;
                            first[ sid[gram[i][0]] ][j] = 1;
                        }
                    }
                }
                if (first[sid[gram[i][k]]][sid[null]] == 0) break;
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
        vgram[sid[symbols[i]]].clear();
        for (j = 0; j < gram_num; j++) {
            if (gram[j][0] == symbols[i])
                vgram[sid[symbols[i]]].push_back(j);
        }
    }
    for (i = 0; i < gram_num; i++) {
        vsymbol[i].clear();
        for (j = 0; j < gram[i].size(); j++) {
            if (sid.find(gram[i][j]) == sid.end()) {
                vsymbol[i].push_back(-1);
            } else {
                vsymbol[i].push_back(sid[gram[i][j]]);
            }
        }
    }
}

static int tmpInItems(std::vector<Item> tmp) {
    int i, j;
    for (i = 0; i < item_num; i++) if (items[i].size() == tmp.size()) {
        for (j = 0; j < items[i].size(); j++) {
            if (items[i][j].gid != tmp[j].gid ||
                    items[i][j].pos != tmp[j].pos ||
                    items[i][j].fwd != tmp[j].fwd)
                break;
        }
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

    while (true) {
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
            for (i = vfirst[g].size() - 1; i >= 0; i--) {
                exp_first.push_back(vfirst[g][i]);
            }
            if (first[ vsymbol[tmp.gid][pos] ][sid[null]] == 1) {
                pos++;
            }
            else {
                break;
            }
        }
    }
    appear[tmp.gid][tmp.pos][tmp.fwd] = 1;
    vexp_first[tmp.gid][tmp.pos][tmp.fwd] = exp_first;
}

static void closure(std::vector<Item> &item) {
    int i, j, jj, k, g;
    ivec exp_first;
    for (i = 0; i < item.size(); i++)
    if (item[i].pos+1 <= gram[ item[i].gid ].size()) {
        g = vsymbol[ item[i].gid ][ item[i].pos ];

        for (jj = vgram[g].size() - 1; jj >= 0; jj--) {
            j = vgram[g][jj];
            expandItem(item[i], exp_first);

            for (k = exp_first.size() - 1; k >= 0; k--) {
                if (check[j][2][exp_first[k]] != now) {
                    item.push_back( Item(j, 2, exp_first[k]) );
                    check[j][2][exp_first[k]] = now;
                }
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
        if (gram[i].size() > len) len = gram[i].size();
    for (i = 0;i < gram_num; i++) {
        for (j = 0; j <= len; j++)
            for (int k = 0; k < symbols.size(); k++) {
                check[i][j][k] = -1; appear[i][j][k] = 0;
            }
    }
    now = -1;

    item_num = 1;
    items[0].clear();
    now++;
    items[0].push_back( Item(0, 2, sid["$"]) );
    check[0][2][sid["$"]] = now;
    closure(items[0]);

    std::sort(items[0].begin(), items[0].end());

    go.clear();
    for (i = 0; i < item_num; i++) {
        for (j = symbols.size() - 1; j >= 0; j--) X[j].clear();
        for (j = items[i].size()-1; j >= 0; j--)
            if (items[i][j].pos < gram[items[i][j].gid].size())
                X[vsymbol[items[i][j].gid][items[i][j].pos]].push_back(j);

        for (j = symbols.size() - 1; j >= 0; j--) if (X[j].size() > 0) {
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

            go.push_back(Go(i, j, next_state));
        }
    }
}

static void makeAction() {
    int i, j;

    for (i = 0; i < item_num; i++) {
        action[i].clear();
        for (j = 0; j < symbols.size(); j++)
            action[i].push_back(Action(-1, -1));
    }

    for (i = 0; i < item_num; i++) {
        for (j = 0; j<items[i].size(); j++) {
            if (items[i][j].pos == gram[items[i][j].gid].size() ||
               (items[i][j].pos+1 == gram[items[i][j].gid].size() &&
                gram[items[i][j].gid][items[i][j].pos] == null)) // 是规约
            {
                action[i][items[i][j].fwd].sta = 1;
                action[i][items[i][j].fwd].to = items[i][j].gid;
            }
        }
    }
    for (i = 0; i < go.size(); i++) {// 是移入
        action[go[i].from][go[i].symbol].to = go[i].to;
        action[go[i].from][go[i].symbol].sta = 0;
    }

    FILE *fp = fopen(action_name.c_str(), "w");

    fprintf(fp, "int action_row = %d, action_col = %d;\r\n", item_num,
            (int)(symbols.size()));
    fprintf(fp, "int action_init[%d][%d]={\r\n", item_num,
            2 * (int)(symbols.size()));
    for (i = 0; i < item_num; i++) {
        fprintf(fp, "{");
        for (j = 0; j < symbols.size(); j++) {
            if (j > 0) {
                fprintf(fp, ", ");
            }
            fprintf(fp, "%d,%d", action[i][j].sta, action[i][j].to);
        }
        fprintf(fp, "},\r\n");
    }
    fprintf(fp, "};\r\n");
    fclose(fp);
}

static void inputAction() {
#include "action.init"
    int i, j, k, l, row, col;
    row = action_row;
    col = action_col;

    for (i = 0; i < row; i++) {
        action[i].clear();
        for (j = 0; j < col; j++) {
            k = action_init[i][j+j];
            l = action_init[i][j+j+1];
            action[i].push_back(Action(l, k));
        }
    }
}

static void inputCode() {
    word.push_back(Word("<keyword>", "$", source.size()) );
    source.push_back( Source("", filename, source.size())) ;
}

static FILE *fp;

static chars makeSolve() {
    int i, j, symbol_id, stmt, len, count, top, to;

    top = 1;
    count = 1;
    sstack.clear();
    sstack.push_back(SStack(0, ""));
    type_name.clear();
    headSubtr = NULL;

    for (i = 0; i < word.size(); i++) {
        while (sstack.size() < top + 1) sstack.push_back(SStack(0, ""));

        symbol_id = sid[word[i].val];
        for (j = 0;j < type_name.size(); j++) {
            if (word[i].val == type_name[j].name)
                if (!i || word[i-1].val != "struct") {
                    symbol_id = sid["TYPE_NAME"];
                    word[i].type = "<keyword>";
                }
        }
        if (word[i].type == "<ident>")  symbol_id = sid[ident];
        if (word[i].type == "<number>") symbol_id = sid[number];
        if (word[i].type == "<string>") symbol_id = sid[strlit];

        to = action[sstack[top-1].state][symbol_id].to;
        switch (action[sstack[top-1].state][symbol_id].sta) {
            case 0:			// PUSH 移入
                sstack[top].state = to;
                sstack[top].val = word[i].val;
                sstack[top].slist.clear();
                sstack[top].slist.push_back(sstack[top].val);

                sstack[top].subtr = new ParseTree();
                sstack[top].subtr->stmt = -1;
                sstack[top].subtr->val = sstack[top].val;
                sstack[top].subtr->line = word[i].line;
                top++;

                if (top > 3 && sstack[top-3].val == "struct" &&
                        sstack[top-1].val == "{") {
                    type_name.push_back( SType_name(sstack[top-2].val) );
                }

                break;
            case 1:			// POP 规约
                stmt = to;

                len = gram[stmt].size();
                if (gram[stmt][len-1] == null) len--;

                //ParseTree subtr;
                subtr = new ParseTree();
                subtr->stmt = stmt;
                subtr->val = "";
                subtr->tree.clear();
                subtr->line = sstack[top-len+2].subtr->line;
                for (j = top-len+2; j < top; j++) {
                    subtr->tree.push_back(sstack[j].subtr);
                }

                top = top - len + 3;
                sstack[top-1].state = sid[gram[stmt][0]];
                sstack[top-1].subtr = subtr;
                if (stmt == 44)
                    type_name.push_back(SType_name(sstack[top].val));
                if (stmt == 12 && sstack[top-1].val.substr(0, 7)
                        == "typedef") {
                    for (j = 0; j < sstack[top].slist.size(); j++) {
                        type_name.push_back(SType_name(sstack[top].slist[j]));
                    }
                }
                if (stmt == 65) {
                    sstack[top-1].slist.push_back(sstack[top+1].val);
                }

                i--;

                if (sstack[top-1].state == sid["<S>"]) {
                    headSubtr = sstack[top-1].subtr;
                    return "parse accept!";
                }

                // GOTO 跳转
                if (top > 1 && sstack[top-1].state >= terms.size() &&
                    action[sstack[top-2].state][sstack[top-1].state].sta == 0) {
                    sstack[top-1].state =
                        action[sstack[top-2].state][sstack[top-1].state].to;

                }
                else {
                    return "parse error: " + source[word[i].line].file +
                        ": " + iTs(source[word[i].line].line)
                        + ": " + word[i].val;
                }
                break;
            case -1:		// error
                return "parse error: " + source[word[i].line].file + ": " +
                    iTs(source[word[i].line].line) + ": " + word[i].val;
                break;
        }
    }
    if (top == 1 && sstack[0].state == sid["<S>"]) {
        headSubtr = sstack[top-1].subtr;
        return "parse accept!";
    } else {
        return "parse error: " + filename + ": at end of file";
    }
}

FILE *out;

void printTree(ParseTree *root, int dep) {
    for (int i = 0; i < dep; i++) fprintf(out, " ");
    chars s;
    if (root->stmt >= 0) {
        for (int i = 0; gram_init[root->stmt][i] != '>'; i++)
            s += gram_init[root->stmt][i];
        s += '>';
    }
    else s = root->val;
    fprintf(out, "%s (%d)\n", s.c_str(), root->line);
    //cout << s << " (" << root->line << ")" << endl;
    for (unsigned i = 0; i < root->tree.size(); i++)
        printTree(root->tree[i], dep + 1);
}

int parse() {
    parse_out		= "parse.out";
    action_name		= "action.init";

    number	= "constant";
    ident	= "identifier";
    strlit	= "string_literal";

    inputGram();

    inputAction();
    inputCode();
    chars ss = makeSolve();
    int success = 0;
    if (ss == "parse accept!") {
        success = 0;
    } else {
        printf("%s\n", ss.c_str());
        success = 1;
    }

    out = fopen(parse_out.c_str(), "w");
    ParseTree *root = subtr;
    printTree(root, 0);
    return success;
}
