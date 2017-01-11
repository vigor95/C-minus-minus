#include "lexer.h"

symbol_table *string_table;
symbol_table *id_table;

void run_lexer(file_struct *file) {
    char *word;
    int u = 0;
    string_table = create_stab("String literals", MAX_SLOTS);
    id_table = create_stab("ID symbol table", MAX_SLOTS);

    void (*dispatcher[255])(FILE*, char*);

    for (u = 0; u < 256; u++)
        dispatcher[u] = &scan_default;

    dispatcher['a' - 0] = &scan_a;
    dispatcher['b' - 0] = &scan_b;
    dispatcher['c' - 0] = &scan_c;
    dispatcher['d' - 0] = &scan_d;
    dispatcher['e' - 0] = &scan_e;               
    dispatcher['f' - 0] = &scan_f;
    dispatcher['g' - 0] = &scan_g;
    dispatcher['i' - 0] = &scan_i;
    dispatcher['l' - 0] = &scan_l;
    dispatcher['r' - 0] = &scan_r;
    dispatcher['s' - 0] = &scan_s;
    dispatcher['t' - 0] = &scan_t;
    dispatcher['u' - 0] = &scan_u;
    dispatcher['v' - 0] = &scan_v;
    dispatcher['w' - 0] = &scan_w;
    dispatcher['p' - 0] = &scan_p;
    dispatcher['{' - 0] = &scan_leftbracket;
    dispatcher['}' - 0] = &scan_rightbracket;
    dispatcher['(' - 0] = &scan_leftparen;
    dispatcher[')' - 0] = &scan_rightparen;
    dispatcher['[' - 0] = &scan_left_sqr_bracket;
    dispatcher[']' - 0] = &scan_right_sqr_bracket;
    dispatcher['.' - 0] = &scan_dot;
    dispatcher[',' - 0] = &scan_comma;
    dispatcher['!' - 0] = &scan_exclamation;
    dispatcher['~' - 0] = &scan_tilda;
    dispatcher['+' - 0] = &scan_plus;
    dispatcher['-' - 0] = &scan_minus;
    dispatcher['*' - 0] = &scan_unary_star;
    dispatcher['&' - 0] = &scan_ampersand;       
    dispatcher['/' - 0] = &scan_div;
    dispatcher['%' - 0] = &scan_mod;
    dispatcher['<' - 0] = &scan_less;        
    dispatcher['>' - 0] = &scan_greater;
    dispatcher['=' - 0] = &scan_equal;
    dispatcher['|' - 0] = &scan_or;
    dispatcher['^' - 0] = &scan_xor;
    dispatcher['?' - 0] = &scan_question;
    dispatcher[':' - 0] = &scan_colon;
    dispatcher[';' - 0] = &scan_semicolon;

    FILE *in = fopen(INTERIM_FILENAME, "r");
    FILE *out = fopen("tmp", "w");

    if (in == NULL) {
        error(file->filename, 0, 0, "Could not open file!");
    }

    while ((word = getword(in)) != NULL) {
        dispatcher[word[0] - 0](out, word);
        free(word);
    }
    fclose(in);
    fclose(out);
    remove(INTERIM_FILENAME);
    rename("tmp", INTERIM_FILENAME);
}

void put_lexeme(FILE *f, char *tk_name, char *tk_value) {
    size_t tk_name_size = strlen(tk_name);
    size_t tk_value_size = strlen(tk_value);

    putc('<', f);
    fwrite(tk_name, sizeof(char), tk_name_size, f);
    putc(',', f);
    fwrite(tk_value, sizeof(char), tk_value_size, f);
    putc('>', f);
}

void put_ulexeme(FILE *f, char *tk_name) {
    size_t tk_name_size = strlen(tk_name);

    putc('<', f);
    fwrite(tk_name, sizeof(char), tk_name_size, f);
    putc('>', f);
}

char* extract_token(char *word) {
    switch (word[0]) {
        char tmp[3];
        case '{': case '}': case '(': case ')':
        case '[': case ']': case '.': case ',':
            tmp[0] = word[0];
            return copy_alloced(tmp);
        case '!': case '~': case '+': case '-':
        case '*': case '/': case '=': case '%': case '^':
            tmp[0] = word[0];
            tmp[1] = (word[1] == '=' ? '=' : '\0');
            return copy_alloced(tmp);
        case '&':
            if (word[1] == '=')
                return copy_alloced("&=");
            else if (word[1] == '&')
                return copy_alloced("&&");
            else
                return copy_alloced("&");
        case '|':
            if (word[1] == '|')
                return copy_alloced("||");
            else if (word[1] == '=')
                return copy_alloced("|=");
            else
                return copy_alloced("=");
        case '<':
            if (word[1] == '<') {
                if (word[2] == '=')
                    return copy_alloced("<<=");
                return copy_alloced("<<");
            }
            else if (word[1] == '=')
                return copy_alloced("<=");
            else
                return copy_alloced("<");
        case '>':
            if (word[1] == '>') {
                if (word[2] == '=')
                    return copy_alloced(">>=");
                return copy_alloced(">>");
            }
            else if (word[1] == '=')
                return copy_alloced(">=");
            else
                return copy_alloced(">");
        case '?': case ':': case ';':
            tmp[0] = word[0];
            return copy_alloced(tmp);
        case '\'':
            return return_char(word);
        case '"':
            return return_string(word);
            break;
        default:
            if (isdigit(word[0])) {
                return return_integral(word);
            }
            else {
                return return_keyword(word);
            }
            break;
    }
    return NULL;
}

char* return_char(char *word) {
    int i, j;
    size_t size = strlen(word);
    char *tmp;
    for (i = 1; i < size; i++) {
        if (word[i] == '\'') {
            i++;
            break;
        }
    }

    tmp = (char*)xmalloc(sizeof(char) * i + 1);
    for (j = 0; j < i; j++)
        tmp[j] = word[j];
    tmp[i] = '\0';
    if (strlen(tmp) > 3)
        error("unknown-file", total_newlines, 0, "Bad character literal\n");
    return tmp;
}

char* return_integral(char *word) {
    int i, j;
    size_t size = strlen(word);

    for (i = 0; i < size; i++) {
        if (!isdigit(word[i]) && word[i] != '.') {
            break;
        }
    }

    char *integral = (char*)xmalloc(sizeof(char) * i + 1);

    for (j = 0; j < i; j++) {
        integral[j] = word[j];
    }
    integral[i] = '\0';
    return integral;
}

char* return_string(char *word) {
    int i, j;
    size_t size = strlen(word);
    char *tmp;

    for (i = 1; i < size; i++) {
        if (word[i] == '"') {
            i++;
            break;
        }
    }
    tmp = (char*)xmalloc(sizeof(char) * i + 1);
    for (j = 0; j < i; j++) {
        tmp[j] = word[j];
    }
    tmp[i] = '\0';

    return tmp;
}

char* return_keyword(char *word) {
    if (word == NULL)
        return NULL;
    size_t size = strlen(word);
    int i;
    for (i = 0; i < size; i++) {
        if (!isdigit(word[i]) && !isalpha(word[i]) && word[i] != '_')
            break;
    }
    char *keyword = (char*)xmalloc(sizeof(char) * i + 1);
    strcpy(keyword, word);
    keyword[i] = '\0';

    return keyword;
}

int is_valid_id(char *word) {
    size_t size = strlen(word);
    int j;
    for (j = 0; j < size; j++) {
        if (!isdigit(word[j]) && !isalpha(word[j]) && word[j] != '_')
            return FALSE;
    }
    return TRUE;
}

int parse_tokens(FILE *f, char *word) {
    int index;
    size_t n, m;
    char *token = extract_token(word);
    char *tmp;
    size_t tk_size = strlen(token);
    size_t word_size = strlen(word);
    size_t diff = word_size - tk_size;
    size_t upto = tk_size;
    token_package tk;

    while (tk_size > 0) {
        tmp = (char*)xmalloc(sizeof(char) * diff + 1);
        for (n = 0, m = upto; n < diff; n++, m++) {
            tmp[n] = word[m];
        }
        tmp[diff] = '\n';

        tk = get_sval(token);
        if (tk.type == -1) {
            sprintf(tk_buffer0, "%d", tk.val);
            put_ulexeme(f, tk_buffer0);
        }
        else if (tk.type == TK_KEYWORD) {
            sprintf(tk_buffer0, "%d", tk.val);
            put_ulexeme(f, tk_buffer0);
        }
        else if (tk.type == TK_IDENTIFIER) {
            record *rec = (record*)get_record(token, "NULL", 'V', 0, "\0");
            rec->addr = -1;
            index = hash(token, id_table->size);
            if (id_table->table[index].slot != EMPTY_SLOT) {
                free(rec);
            }
            else {
                stab_insert("symbol", rec, id_table);
            }
            sprintf(tk_buffer0, "%d", TK_IDENTIFIER);
            sprintf(tk_buffer1, "%d", index);
            put_lexeme(f, tk_buffer0, tk_buffer1);
        }
        else if (tk.type == TK_STRINGLIT) {
            record *rec = (record*)get_record(token, "NULL", 'S', 0, "\0");
            stab_insert("c-string-literal", rec, string_table);
            index = hash(rec->name, string_table->size);

            sprintf(tk_buffer0, "%d", TK_STRINGLIT);
            sprintf(tk_buffer1, "%d", index);

            put_lexeme(f, tk_buffer0, tk_buffer1);
        }
        else if (tk.type == TK_INTLIT) {
            sprintf(tk_buffer0, "%d", TK_INTLIT);
            sprintf(tk_buffer1, "%d", tk.val);
            put_lexeme(f, tk_buffer0, tk_buffer1);
        }
        else if (tk.type == TK_FLOATLIT) {
            sprintf(tk_buffer0, "%d", TK_FLOATLIT);
            sprintf(tk_buffer1, "%f", tk.fval);
            put_lexeme(f, tk_buffer0, tk_buffer1);
        }
        if (token != NULL) {
            free(token);
        }
        token = extract_token(tmp);
        tk_size = strlen(token);
        upto += tk_size;
        diff = word_size - upto;
        free(tmp);
    }
    return 0;
}
