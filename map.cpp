#include "cic.h"

template <class T>
Map<T>* makeMapParent(Map<T> *p) {
    Map<T> *r = new Map<T>;
    r->parent = p;
    return r;
}
