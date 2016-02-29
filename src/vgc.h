
#ifndef VGC_H
#define VGC_H

#include "vvar.h"
#include "vstring.h"
#include "vlist.h"
#include "vdict.h"
#include "vlex.h"

//#define VGC_GREY		0x0000
#define VGC_WHITE		0x0000
#define VGC_BLACK		0x0100
#define VGC_CMASK		0xff00
#define VGC_TMASK		0x00ff

typedef struct GCObject {
	unsigned int type;
	struct GCObject* next;
	union {
		struct VString* s;
		struct VDict* d;
		struct VList* l;
		struct Ast* fn;
	};
} GCObject;


void gc_newgcobj(VVar *obj, enum VVarType type);
void gc_newgcobj_noreg(VVar *obj, enum VVarType type);
void gc_reg(VVar *obj);

void gc_markb(VVar o);
void gc_mark();
void gc_markvvar(VVar obj);
void gc_sweep();

void gc_dump();

#endif // VGC_H
