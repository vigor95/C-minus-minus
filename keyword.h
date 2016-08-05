struct keyword {
    const char *name;
    int len;
    int tk;
};

static struct keyword *keywords[] = {
    {//_
        {"__int64", 0, TK_INT64},
        {NULL,      0, TK_ID}
    },
    {//a
        {"auto",    4, TK_AUTO},
        {NULL,      0, TK_ID}
    },
    {//b
        {"break",   5, TK_BREAK},
        {NULL,      0, TK_ID}
    },
    {//c
        {"case",    4, TK_CASE},
        {"char",    4, TK_CHAR},
        {"const",   5, TK_CONST},
        {"continue",8, TK_CONTINUE},
        {NULL,      0, TK_ID}
    },
    {//d
        {"default", 7, TK_DEFAULT},
        {"do",      2, TK_DO},
        {"double",  6, TK_DOUBLE},
        {NULL,      0, TK_ID}
    },
    {//e
        {"else",    4, TK_ELSE},
        {"enum",    4, TK_ENUM},
        {"extern",  6, TKEXTERN},
        {NULL,      0, TK_ID}
    },
    {//f
        {"float",   5, TK_FLOAT},
        {"for",     3, TK_FOR},
        {NULL,      0, TK_ID}
    },
    {//g
        {"goto",    4, TK_GOTO},
        {NULL,      0, TK_ID}
    },
    {//h
        {NULL,      0, TK_ID}
    },
    {//i
        {"if",  2, TK_IF},
        {"int", 3, TK_INT},
        {NULL,  0, TK_ID}
    },
    {//j
        {NULL,  0,  TK_ID}
    },
    {//k
        {NULL,  0,  TK_ID}
    },
    {//l
        {"long",4,  TK_LONG},
        {NULL,  0,  TK_ID}
    },
    {//m
        {NULL,  0,  TK_ID}
    },
    {//n
        {NULL,  0,  TK_ID}
    },
    {//o
        {NULL,  0,  TK_ID}
    },
    {//p
        {NULL,  0,  TK_ID}
    },
    {//q
        {NULL,  0,  TK_ID}
    },
    {//r
        {"register",8,  TK_REGISTER},
        {"return",  6,  TK_RETURN},
        {NULL,      0,  TK_ID}
    },
    {//s
        {"short",   5,  TK_SHORT},
        {"signed",  6,  TK_SIGNED},
        {"sizeof",  6,  TK_SIZEOF},
        {"static",  6,  TK_STATIC},
        {"struct",  6,  TK_STRUCT},
        {"switch",  6,  TK_SWITCH},
        {NULL,      0,  TK_ID}
    },
    {//t
        {"typedef", 7,  TK_TYPEDEF},
        {NULL,      0,  TK_ID}
    },
    {//u
        {"union",   5,  TK_UNION},
        {"unsigned",8,  TK_UNSIGNED},
        {NULL,      0,  TK_ID}
    },
    {//v
        {"void",    4,  TK_VOID},
        {"volatile",8,  TK_VOLATILE},
        {NULL,      0,  TK_ID}
    },
    {//w
        {"while",   5,  TK_WHILE},
        {NULL,      0,  TK_ID}
    },
    {//x
        {NULL,  0,  TK_ID}
    },
    {//y
        {NULL,  0,  TK_ID}
    },
    {//z
        {NULL,  0,  TK_ID}
    }
};
