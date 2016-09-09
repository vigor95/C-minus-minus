#ifndef __VECTOR_H_
#define __VECTOR_H_

typedef struct vector {
    void **data;
    int len;
    int size;
} *Vector;

Vector CreateVector(int);
void ExpandVector(Vector);

#define Len(v) (v->len)

#define InsertItem(v, x)    \
    if (v->len >= v->size)  \
        ExpandVector(v);    \
    v->data[v->len] = x; \
    v->len++;

#define GetItem(v, i) (v->data[i])

#define TopItem(v) (v->len == 0 ? NULL : v->data[v->len - 1])

#define FOR_EACH_ITEM(ty, x, v)     \
{                                   \
    int i, len = v->len;            \
    for (i = 0; i < len; i++) {     \
        x = (ty)v->data[i];         

#define ENDFOR  \
    }           \
}

#endif
