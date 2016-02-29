
#ifndef VVAR_H
#define VVAR_H

#include "vinyl.h"
#include "vio.h"

extern const char* Type[];

enum VVarType {
	T_VNull,
	T_VBool,
	T_VInt,
	T_VFloat,
	T_VString,
	T_VList,
	T_VDict,
	T_VFunc,
	T_VCFunc,
};

typedef struct GCObject GCObject;

typedef struct VVar VVar;

struct VVar {
	enum VVarType type;
	union {
		int32_t i;
		double f;
		GCObject* gc;
		void* cfunc;
	};
};

int32_t vvar_hash(const VVar var);
int64_t vvar_cmp(const VVar lhs, const VVar rhs);

int vvar_fprint1(FILE* stream, const VVar var, const VVar root, int depth);
int vvar_fprint(FILE* stream, const VVar var);
void vvar_print(const VVar var);

VVar newvnull();
VVar newvbool(const int value);
VVar newvint(const int32_t value);
VVar newvfloat(const double value);

char* vvar_tocstr(VVar* var);

#endif // VVAR_H
