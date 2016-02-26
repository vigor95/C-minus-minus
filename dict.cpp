#include "cic.h"

Dict* makeDict() {
    Dict *r = new Dict;
    r->map = new std::map<char*, Type*>;
    r->key = new std::vector<char*>;
    return r;
}

Type* dictGet(Dict *dict, char *key) {
    return dict->map->find(key)->second;
}

void dictPut(Dict *dict, char *key, Type *val) {
    if (dict->map->find(key) != dict->map->end())
        dict->map->erase(key);
    dict->map->insert(std::pair<char*, Type*>(key, val));
}

std::vector<char*>* dictKeys(Dict *dict) {
    return dict->key;
}
