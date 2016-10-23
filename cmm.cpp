#include "cmm.h"

int OUT_OTHER_FILE;

string filename;
string assemblyname;
string objectname;
string outfilename;

int MAKE_ACTION_INIT = 0;

int main(int argc, char **argv) {
    OUT_OTHER_FILE = 1;
    assemblyname = "./data/assembly.s";
    objectname = "./data/assembly.o";
    
    for (auto i = 1; i < argc; i++) {
        string argv_i = argv[i];
        int l = argv_i.length();
        auto argv_i_post1 = argv_i.substr(max(0, l - 2));
        auto argv_i_post2 = argv_i.substr(max(0, l - 4));
        for (auto &ch: argv_i_post1)
            if ('A' <= ch && ch <= 'Z') ch += 32;
        for (auto &ch: argv_i_post2)
            if ('A' <= ch && ch <= 'Z') ch += 32;

        if (argv_i_post2 == ".c" || argv_i_post2 == ".cpp")
            filename = argv[i];
    }

    if (filename == "") {
        printf("Error: no input file\n");
        return 10;
    }

    if (outfilename == "") {
        int i = filename.size() - 1;
        while (i > 0 && filename[i] != '.') i--;
        int j = i - 1;
        while (j >= 0 && filename[j] != '.' && filename[j] != '/'
                && filename[i] != '\\') j--;
        for (j++; j < i; j++) outfilename += filename[j];
    }

    if (preprocess_main()) return 1;
} 
