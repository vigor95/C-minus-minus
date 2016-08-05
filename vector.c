#include "cic.h"

Vector CreateVector(int sz) {
    Vector vec = NULL;

    ALLOC(vec);
    vec->data = (void**)HeapAllocate(current_heap, sz * sizeof(void*));
    vec->size = sz;
    vec->len = 0;
    return 0;
}

void ExpandVector(Vector v) {
    void *orig = v->data;
    v->data = (void**)HeapAllocate(current_heap,
            v->size * 2 * sizeof(void*));
    memcpy(v->data, orig, v->size * sizeof(void*));
    v->size *= 2;
}
