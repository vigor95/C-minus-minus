#include "cic.h"
#include "output.h"
#include "type.h"
#include "config.h"

struct type types[VOID - CHAR + 1];
Type default_function_type;
Type wchar_type;

Type ArgumentPromote(Type ty) {
    return ty->categ < INT ? T(INT) :
        (ty->categ == FLOAT ? T(DOUBLE) : ty);
}

static int IsCompatibleFunction(FunctionType fty1, FunctionType fty2) {
    Signature sig1 = fty1->sig, sig2 = fty2->sig;
    Parameter param1, param2;
    int para_len1, para_len2;
    int i;

    if (!IsCompatibleType(fty1->bty, fty2->bty))    return 0;
    if (!sig1->has_proto && !sig2->has_proto)       return 1;
    else if (sig1->has_proto && sig2->has_proto) {
        para_len1 = Len(sig1->params);
        para_len2 = Len(sig2->params);

        if ((sig1->has_ellipse ^ sig2->has_ellipse) ||
                para_len1 != para_len2)
            return 0;

        for (i = 0; i < para_len1; i++) {
            param1 = (Parameter)GetItem(sig1->params, i);
            param2 = (Parameter)GetItem(sig2->params, i);

            if (!IsCompatibleType(param1->ty, param2->ty))
                return 0;
        }

        return 1;
    }
    else if (!sig1->has_proto && sig2->has_proto) {
        sig1 = fty1->sig;
        sig2 = fty2->sig;
        goto mix_proto;
    }
    else {
mix_proto:
        para_len1 = Len(sig1->params);
        para_len2 = Len(sig2->params);

        if (sig1->has_ellipse)
            return 0;

        if (para_len2 == 0) {
            FOR_EACH_ITEM(Parameter, param1, sig1->params)
                if (!IsCompatibleType(Promote(param1->ty), param1->ty))
                    return 0;
            ENDFOR

            return 1;
        }
        else if (para_len1 != para_len2) {
            return 0;
        }
        else {
            for (i = 0; i < para_len1; i++) {
                param1 = (Parameter)GetItem(sig1->params, i);
                param2 = (Parameter)GetItem(sig2->params, i);

                if (!IsCompatibleType(param1->ty, Promote(param2->ty)))
                    return 0;
            }
            return 1;
        }
    }
}

int IsCompatibleType(Type ty1, Type ty2) {
    if (ty1 == ty2)
        return 1;

    if (ty1->qual != ty2->qual)
        return 0;

    ty1 = Unqual(ty1);
    ty2 = Unqual(ty2);

    if (ty1->categ == ENUM && ty2 == ty1->bty ||
        ty2->categ == ENUM && ty1 == ty2->bty)
        return 1;

    if (ty1->categ != ty2->categ)
        return 0;

    switch (ty1->categ) {
        case POINTER:
            return IsCompatibleType(ty1->bty, ty2->bty);
        case ARRAY:
            return IsCompatibleType(ty1->bty, ty2->bty) &&
                (ty1->size == ty2->size || !ty1->size|| !ty2->size);
        case FUNCTION:
            return IsCompatibleFunction((FunctionType)ty1,
                    (FunctionType)ty2);
        default:
            return ty1 == ty2;
    }
}

Type Enum(char *id) {
    EnumType ety;
    ALLOC(ety);
    ety->categ = ENUM;
    ety->id = id;

    ety->bty = T(INT);
    ety->size = ety->bty->size;
    ety->align = ety->bty->align;
    ety->qual = 0;

    return (Type)ety;
}

Type Qualify(int qual, Type ty) {
    Type qty;
    if (qual == 0 || qual == ty->qual)
        return ty;

    ALLOC(qty);
    *qty = *ty;
    qty->qual |= qual;
    if (ty->qual)
        qty->bty = ty->bty;
    else
        qty->bty = ty;

    return qty;
}

Type Unqual(Type ty) {
    if (ty->qual)
        ty = ty->bty;
    return ty;
}

Type ArrayOf(int len, Type ty) {
    Type aty;
    ALLOC(aty);
    aty->categ = ARRAY;
    aty->qual = 0;
    aty->size = len * ty->size;
    aty->align = ty->align;
    aty->bty = ty;

    return (Type)aty;
}

Type PointerTo(Type ty) {
    Type pty;
    ALLOC(pty);
    pty->categ = POINTER;
    pty->qual = 0;
    pty->align = T(POINTER)->align;
    pty->size = T(POINTER)->size;
    pty->bty = ty;

    return pty;
}

Type FunctionReturn(Type ty, Signature sig) {
    FunctionType fty;
    ALLOC(fty);
    fty->categ = FUNCTION;
    fty->qual = 0;
    fty->size = T(POINTER)->size;
    fty->align = T(POINTER)->align;
    fty->sig = sig;
    fty->bty = ty;

    return (Type)fty;
}

Type StartRecord(char *id, int categ) {
    RecordType rty;
    ALLOC(rty);
    memset(rty, 0, sizeof(*rty));
    rty->categ = categ;
    rty->id = id;
    rty->tail = &rty->flds;

    return (Type)rty;
}

Field AddField(Type ty, char *id, Type fty, int bits) {
    RecordType rty = (RecordType)ty;
    Field fld;

    if (fty->size == 0) {
        assert(fty->categ == ARRAY);
        rty->has_flex_arr = 1;
    }
    if (fty->qual & CONST)
        rty->has_const_fld = 1;

    ALLOC(fld);
    fld->id = id;
    fld->ty = fty;
    fld->bits = bits;
    fld->pos = fld->offset = 0;
    fld->next = NULL;

    *rty->tail = fld;
    rty->tail = &(fld->next);

    return fld;
}

Field LookupField(Type ty, char *id) {
    RecordType rty = (RecordType)ty;
    Field fld = rty->flds;

    while (fld) {
        if (fld->id == NULL && IsRecordType(fld->ty)) {
            Field p = LookupField(fld->ty, id);
            if (p)
                return p;
        }
        else if (fld->id == id)
            return fld;

        fld = fld->next;
    }

    return NULL;
}

void AddOffset(RecordType rty, int offset) {
    Field fld = rty->flds;

    while (fld) {
        fld->offset += offset;
        if (fld->id == NULL && IsRecordType(fld->ty))
            AddOffset((RecordType)fld->ty, fld->offset);
        fld = fld->next;
    }
}

void EndRecord(Type ty) {
    RecordType rty = (RecordType)ty;
    Field fld = rty->flds;
    int bits = 0;
    int int_bits = T(INT)->size * 8;

    if (rty->categ == STRUCT) {
        while (fld) {
            fld->offset = rty->size = ALIGN(rty->size, fld->ty->align);
            if (fld->id == NULL && IsRecordType(fld->ty))
                AddOffset((RecordType)fld->ty, fld->offset);
            if (fld->bits == 0) {
                if (bits) {
                    fld->offset = rty->size =
                        ALIGN(rty->size + T(INT)->size, fld->ty->align);
                }
                bits = 0;
                rty->size = rty->size + fld->ty->size;
            }
            else if (bits + fld->bits <= int_bits) {
                fld->pos = LITTLE_ENDIAN ? bits : int_bits - bits;
                bits += fld->bits;
                if (bits == int_bits) {
                    rty->size += T(INT)->size;
                    bits = 0;
                }
            }
            else {
                rty->size += T(INT)->size;
                fld->offset += T(INT)->size;
                fld->pos = LITTLE_ENDIAN ? 0 : int_bits - fld->bits;
                bits = fld->bits;
            }
            if (fld->ty->align > rty->align)
                rty->align = fld->ty->align;

            fld = fld->next;
        }
        if (bits)
            rty->size += T(INT)->size;

        rty->size = ALIGN(rty->size, rty->align);
    }
    else {
        while (fld) {
            if (fld->ty->align > rty->align)
                rty->align = fld->ty->align;
            if (fld->ty->size > rty->size)
                rty->size = fld->ty->size;
            
            fld = fld->next;
        }
    }
}

Type CompositeType(Type ty1, Type ty2) {
    assert(IsCompatibleType(ty1, ty2));

    if (ty1->categ == ENUM)
        return ty1;
    if (ty2->categ == ENUM)
        return ty2;

    switch (ty1->categ) {
        case POINTER:
            return Qualify(ty1->qual,
                PointerTo(CompositeType(ty1->bty, ty2->bty)));
        case ARRAY:
            return ty1->size != 0 ? ty1 : ty2;
        case FUNCTION: {
            FunctionType fty1 = (FunctionType)ty1;
            FunctionType fty2 = (FunctionType)ty2;

            fty1->bty = CompositeType(fty1->bty, fty2->bty);
            if (fty1->sig->has_proto && fty2->sig->has_proto) {
                Parameter p1, p2;
                int i, len = Len(fty1->sig->params);
                for (i = 0; i < len; i++) {
                    p1 = (Parameter)GetItem(fty1->sig->params, i);
                    p2 = (Parameter)GetItem(fty2->sig->params, i);
                    p1->ty = CompositeType(p1->ty, p2->ty);
                }

                return ty1;
            }

            return fty1->sig->has_proto ? ty1 : ty2;
        }
        default:
            return ty1;
    }
}

int TypeCode(Type ty) {
    static int optypes[] = {I1, U1, I2, U2, I4, U4, I4, U4, I4, U4, I4,
    F4, F8, F8, U4, V, B, B, B};

    assert(ty->categ != FUNCTION);
    return optypes[ty->categ];
}

const char* TypeToString(Type ty) {
    int qual;
    const char *str;
    const char *names[] = {
        "char", "unsigned char", "short", "unsigned short", "int",
        "unsigned int", "long", "unsigned long", "long long",
        "unsigned long long", "enum", "float", "double", "long double"
    };

    if (ty->qual != 0) {
        qual = ty->qual;
        ty = Unqual(ty);

        if (qual == CONST)
            str = "const";
        else if (qual == VOLATILE)
            str = "volatile";
        else
            str = "const volatile";

        return FormatName("%s %s", str, TypeToString(ty));
    }

    if (CHAR <= ty->categ && ty->categ <= LONGDOUBLE && ty->categ != ENUM)
        return names[ty->categ];

    switch (ty->categ) {
        case ENUM:
            return FormatName("enum %s", ((EnumType)ty)->id);
        case POINTER:
            return FormatName("%s *", TypeToString(ty->bty));
        case UNION:
            return FormatName("union %s", ((RecordType)ty)->id);
        case STRUCT:
            return FormatName("struct %s", ((RecordType)ty)->id);
        case ARRAY:
            return FormatName("%s[%d]",
                TypeToString(ty->bty), ty->size / ty->bty->size);
        case VOID:
            return "void";
        case FUNCTION: {
            FunctionType fty = (FunctionType)ty;
            return FormatName("%s ()", TypeToString(fty->bty));
        }
        default:
            assert(0);
            return NULL;
    }
}

void SetupTypeSystem() {
    int i;
    FunctionType fty;

    T(CHAR)->size = T(UCHAR)->size = CHAR_SIZE;
    T(SHORT)->size = T(USHORT)->size = SHORT_SIZE;
    T(INT)->size = T(UINT)->size = INT_SIZE;
    T(LONG)->size = T(ULONG)->size = LONG_SIZE;
    T(LONGLONG)->size = T(ULONGLONG)->size = LONG_LONG_SIZE;
    T(FLOAT)->size = FLOAT_SIZE;
    T(DOUBLE)->size = DOUBLE_SIZE;
    T(LONGDOUBLE)->size = LONG_DOUBLE_SIZE;
    T(POINTER)->size = INT_SIZE;

    for (i = CHAR; i <= VOID; i++) {
        T(i)->categ = i;
        T(i)->align = T(i)->size;
    }

    ALLOC(fty);
    fty->categ = FUNCTION;
    fty->qual = 0;
    fty->align = fty->size = T(POINTER)->size;
    fty->bty = T(INT);
    ALLOC(fty->sig);
    CALLOC(fty->sig->params);
    fty->sig->has_proto = 0;
    fty->sig->has_ellipse = 0;

    default_function_type = (Type)fty;
    wchar_type = T(WCHAR);
}
