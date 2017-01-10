#include "main.h"

size_t total_newlines = 1;

extern file_struct file;

int main(int argc, char *argv[]) {
    char *outfile;
    char *infile;
    size_t out_name_len;
    
    strcpy(file.calling_prog, argv[0]);
    strcpy(file.default_out, DEFAULT_OUTPUT);
    file.cur_line = 0;
    file.cur_column = 0;

    switch (argc) {
        case 1:
            printf("no input file\n");
            break;
        case 2:
            strcpy(file.filename, argv[1]);
            break;
        case 3:
            fprintf(stderr, "File input error\n");
            exit(EXIT_FAILURE);
            break;
    }
    return EXIT_SUCCESS;
}
