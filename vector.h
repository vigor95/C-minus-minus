#ifndef __VECTOR_H_
#define __VECTOR_H_

typedef struct vector {
    void **data;
    int len;
    int size;
} *Vector;

Vector CreateVector(int);
void ExpandVector(Vector);

int Len(Vector v) {
    return v->len;
}

void InsertItem(Vector v, void **x) {
    if (v->len >= v->size)
        ExpandVector(v);
    v->data[v->len++] = x;
}

void* GetItem(Vector v, int i) {
    return v->data[i];
}

void* TopItem(Vector v) {
    return v->len == 0 ? NULL : v->data[v->len - 1];
}

#define FOR_EACH_ITEM(ty, x, v)     \
{                                   \
    int len = v->len;               \
    for (int i = 0; i < len; i++) { \
        x = (ty)v->data[i];         \

#define ENDFOR  \
    }           \
}

#endif
