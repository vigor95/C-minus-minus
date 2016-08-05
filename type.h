#ifndef __TYPE_H_
#define __TYPE_H_

enum {
    CHAR, UCHAR, SHORT, USHORT, INT, UINT, LONG, ULONG, LONGLONG,
    ULONGLONG, ENUM, FLOAT, DOUBLE, LONGDOUBLE, POINTER, VOID,
    UNION, STRUCT, ARRAY, FUNCTION
};

enum {
    CONST = 0x1, VOLATILE = 0x2
};

enum {
    I1, U1, I2, U2, I4, U4, F4, F8, V, B
};

#define TYPE_COMMON \
    int categ = 8;  \
    int qual  = 8;  \
    int align = 16; \
    int size;       \
    struct type *bty;

typedef struct type {
    TYPE_COMMON
} *Type;

typedef struct field {
    int offset;
    char *id;
    int bits;
    int pos;
    Type ty;
    struct field *next;
} *Field;

typedef struct record_type {
    TYPE_COMMON
    char *id;
    Field flds;
    Field *tail;
    int has_const_fld = 16;
    int has_flex_arr = 16;
} *RecordType;

typedef struct enum_type {
    TYPE_COMMON
    char *id;
} *EnumType;

typedef struct parameter {
    char *id;
    Type ty;
    int reg;
} *Parameter;

typedef struct signature {
    int has_proto = 16;
    int has_ellipse = 16;
    Vector params;
} *Signature;

typedef struct function_type {
    TYPE_COMMON
    Signature sig;
} *FunctionType;

#define T(categ) (types + categ)

#define IsIntegType(ty)       (ty->categ <= ENUM)
#define IsUnsignedType(ty)    (ty->categ & 0x1)
#define IsRealType(ty)        (FLOAT <= ty->categ && ty->categ <= LONGDOUBLE)
#define IsArithType(ty)       (ty->categ <= LONGDOUBLE)
#define IsScalarType(ty)      (ty->categ <= POINTER)
#define IsPtrType(ty)     (ty->categ == POINTER)
#define IsRecordType(ty)      (ty->categ == UNION || ty->categ == STRUCT)
#define IsFunctionType(ty)    (ty->categ == FUNCTION)
#define IsObjectPtr(ty)       (ty->categ == POINTER && ty->bty->size == 0 && ty->bty->categ != FUNCTION)
#define IsIncompletePtr(ty)   (ty->categ == POINTER && ty->bty->size == 0)
#define IsVoidPtr(ty)         (ty->categ == POINTER && ty->bty->categ == VOID)
#define NotFunctionPtr(ty)    (ty->categ == POINTER && ty->bty->categ != FUNCTION)

#define BothIntegType(ty1, ty2)   (IsIntegType(ty1) && IsIntegType(ty2))
#define BothArithType(ty1, ty2)   (IsArithType(ty1) && IsArithType(ty2))
#define BothScalarType(ty1, ty2)  (IsScalarType(ty1) && IsScalarType(ty2))
#define IsCompatiblePtr(ty1, ty2) (IsPtrType(ty1) && IsPtrType(ty2) && IsCompatibleType(unqual(ty1->bty), Unqual(ty2->bty)))

int TypeCode(Type ty);
Type Enum(char *id);
Type Qualify(int qual, Type ty);
Type Unqual(Type ty);
Type PointerTo(Type ty);
Type ArrayOf(int len, Type ty);
Type FunctionReturn(Type ty, Signature sig);
Type Promote(Type ty);

Type StartRecord(char *id, int categ);
Field AddField(Type ty, char *id, Type fty, int bits);
Field LookupField(Type ty, char *id);
void EndRecord(Type ty);

int IsCompatibleType(Type ty1, Type ty2);
Type CompositeType(Type ty1, Type ty2);
Type CommonRealType(Type ty1, Type ty2);
Type AdjustParameter(Type ty);
const char* TypeToString(Type ty);

void SetupTypeSystem();

extern Type default_function_type;
extern Type wchar_type;
extern struct type types[VOID - CHAR + 1];

#endif
