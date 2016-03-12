#include "cic.h"

std::vector<Source> source;

static chars pre_out;
static chars pre_in;

static void readCode(chars file, svec &code) {
    std::fstream fin(file.c_str());
    chars tmp;
    while (getline(fin, tmp)) {
        code.push_back(tmp);
    }
}

static chars readSource(chars file) {
    svec code;
    readCode(file, code);
    for (int i = 0; i < code.size(); i++) {
        source.push_back(Source(code[i], file, i + 1));
    }
    return "";
} 

int preprocess() {
    int ok = 0;
    pre_out = "preprocess.out";
    pre_in = filename;

    chars ret = readSource(pre_in);
    if (ret.length() > 0) {
        std::cout << "preprocess err: ";
        ok = 1;
    }
    FILE *fp = fopen(pre_out.c_str(), "w");
    for (int i = 0; i < source.size(); i++) {
        if (strlen(source[i].code.c_str()))
            fprintf(fp, "%s\n", source[i].code.c_str());
    }
    fclose(fp);
    return ok;
}
