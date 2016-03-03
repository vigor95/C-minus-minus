#include "cic.h"

template <class T>
Dict<T>* makeDict() {
    Dict<T> *r = new Dict<T>;
    r->map = new std::map<char*, T*>;
    r->key = new std::vector<char*>;
    return r;
}

template <class T>
Type* dictGet(Dict<T> *dict, char *key) {
    return dict->map->find(key)->second;
}

template <class T>
void dictPut(Dict<T> *dict, char *key, T *val) {
    if (dict->map->find(key) != dict->map->end())
        dict->map->erase(key);
    dict->map->insert(std::pair<char*, T*>(key, (T*)val));
}

template <class T>
std::vector<char*>* dictKeys(Dict<T> *dict) {
    return dict->key;
}
