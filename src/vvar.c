
#include "vvar.h"
#include "vgc.h"
#include "vlist.h"
#include "vdict.h"
#include "vmisc.h"

const char* Type[] = {
	"Null",
	"Bool",
	"Int",
	"Double",
	"String",
	"List",
	"Dict",
	"Func",
	"CFunc",
};

VVar newvnull() { VVar var;	var.type = T_VNull;	return var; }
VVar newvbool(const int value) { VVar var; var.type = T_VBool; var.i = value; return var; }
VVar newvint(const int32_t value) { VVar var; var.type = T_VInt; var.i = value; return var; }
VVar newvfloat(const double value) { VVar var; var.type = T_VFloat; var.f = value; return var; }

int32_t vvar_hash(const VVar var) { // todo
	switch (var.type) {
	case T_VNull:
		return 0;
	case T_VBool: case T_VInt:
		return vhash((char*)(&var.i), sizeof(int32_t));
	case T_VFloat:
		return vhash((char*)(&var.f), sizeof(double));
	case T_VString:
		return var.gc->s->hash;
	case T_VList: case T_VDict: case T_VFunc:
		return vhash((char*)(&var.gc), sizeof(void*));
	default:
		return 0;
	}
}

int64_t vvar_cmp(const VVar lhs, const VVar rhs) {
	if (lhs.type == rhs.type) {
		switch (lhs.type) {
		case T_VNull:
			return 0;
		case T_VBool: case T_VInt:
			return lhs.i - rhs.i;
		case T_VFloat:
			if (lhs.f == rhs.f) return 0;
			else if (lhs.f < rhs.f) return -1;
			else return 1;
		case T_VString:
			if (lhs.gc->s == rhs.gc->s) return 0; // same entry in strpool
			else {
				char* lstr = lhs.gc->s->p;
				char* rstr = rhs.gc->s->p;
				return strcmp(lstr, rstr);
			}
		case T_VList: case T_VDict: case T_VFunc:
			return (size_t)lhs.gc - (size_t)rhs.gc;
		default:
			return 1;
		}
	}
	else {
		switch (lhs.type) {
		case T_VInt:
			switch (rhs.type) {
			case T_VFloat:
				if (lhs.i == rhs.f) return 0;
				else if (lhs.i < rhs.f) return -1;
				else return 1;
			default:
				break;
			}
		case T_VFloat:
			switch (rhs.type) {
			case T_VInt:
				if (lhs.f == rhs.i) return 0;
				else if (lhs.f < rhs.i) return -1;
				else return 1;
			default:
				break;
			}
		default:
			break;
		}
		return lhs.type - rhs.type;
	}
}

int vvar_fprint1(FILE* stream, const VVar var, const VVar root, int depth) {
#define fprntf(...)		fprintf(stream, __VA_ARGS__)
	switch (var.type) {
	case T_VNull:		return fprntf("null");
	case T_VBool:		return fprntf(var.i ? "true" : "false");
	case T_VInt:		return fprntf("%d", (int32_t)var.i);
	case T_VFloat:		return fprntf("%f", (double)var.f);
	case T_VString:		return fprntf("\"%s\"", var.gc->s->p);
	case T_VList:		return vlist_fprint(stream, var, root, depth + 1);
	case T_VDict:		return vdict_fprint(stream, var, root, depth + 1);
	case T_VFunc:		return fprntf("function"); // : %lld", (int64_t)var.gc->fn);
	case T_VCFunc:	return fprntf("nativefunction"); // : %lld", (int64_t)var.gc->fn);
	default:			return fprntf("<unknown type>");
	}
#undef fprntf
}

int vvar_fprint(FILE* stream, const VVar var) {
	return vvar_fprint1(stream, var, var, 0);
}

void vvar_print(const VVar var) {
	vvar_fprint1(stdout, var, var, 0);
}

char* vvar_tocstr(VVar* var) {
	return var->gc->s->p;
}
