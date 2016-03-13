#include "cic.h"

std::vector<Source> source;

static chars preprocessor_out;
static chars preprocessor_in;

static int error_line;
static chars error_file;

// =======================================

static void readCode(chars codefile, svec &code) {
    code.clear();
    std::fstream fin(codefile.c_str());
    if (!fin.is_open()) {
        std::cout << "open error!" << std::endl;
        exit(0);
    }
    std::cout << "readcode" << std::endl;
    chars tmp;
    while (getline(fin, tmp)) {
        code.push_back(tmp);
    }
}

static void solve(svec &code)
{
    unsigned ok, x, y;
    for (unsigned i = 0; i < code.size(); i++) {
        for (unsigned j = 0; j + 1 < code[i].length(); j++)
            if (code[i][j] == '/' && code[i][j+1] == '/') {
                code[i] = code[i].substr(0, j);
                break;
            }
    }
    for (unsigned i = 0; i < code.size(); i++) {
        for (unsigned j = 0; j + 1 < code[i].length(); j++)
            if (code[i][j] == '/' && code[i][j+1] == '*') {
                ok = 0;
                for (unsigned k = i; k < code.size(); k++) {
                    for (unsigned l = 0; l + 1 < code[k].length(); l++) {
                        if (k == i && l <= j + 1) continue;
                        if (code[k][l] == '*' && code[k][l+1] == '/') {
                            for (unsigned x = i; x <= k; x++) {
                                for (unsigned y = 0; y < code[x].length(); y++) {
                                    if (x == i && y < j) continue;
                                    if (x == k && y > l+1) break;
                                    code[x][y] = '\0';
                                }
                            }
                            ok = 1; x = k; y = l;
                            break;
                        }
                    }
                    if (ok == 1) break;
                }
                if (ok == 1) {
                    i = x; j = y;
                } else if (ok == 0) {
                    i = code.size();
                }
        }
    }
}

static chars readSource(chars codefile) {
    svec code;

    readCode(codefile, code);
    solve(code);

    for (unsigned i = 0; i < code.size(); i++) {
        source.push_back( Source(code[i], codefile, i + 1) );
    }
    return "";
}

int preprocess() {
    preprocessor_out = "preprocessor.out";

    preprocessor_in = filename;

    int success = 0;

    source.clear();

    // 输入
    chars ret = readSource(preprocessor_in);

    if (ret.length() > 0) {
        printf("preprocessor error: %s: %d: %s\n", error_file.c_str(), error_line+1, ret.c_str());
        success = 1;
    }

    return success;
}
