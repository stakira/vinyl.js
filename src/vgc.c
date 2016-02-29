
#include "vgc.h"
#include "vmisc.h"

static GCObject head = { 0, 0 };

void gc_newgcobj(VVar *obj, enum VVarType type) {
	gc_newgcobj_noreg(obj, type);
	gc_reg(obj);
}

void gc_newgcobj_noreg(VVar *obj, enum VVarType type) {
	obj->type = type;
	obj->gc = (GCObject*)malloc(sizeof(GCObject));
	obj->gc->type = type | VGC_WHITE;
}

void gc_reg(VVar *obj) {
	obj->gc->next = head.next;
	head.next = obj->gc;
}

void gc_dump() {
	VVar obj;
	GCObject* ptr = head.next;
	printf("GC dump:");
	int count = 0;
	while (ptr != 0) {
		obj.type = ptr->type & VGC_TMASK;
		obj.gc = ptr;
		printf("\n    <%s> ", Type[obj.type]);
		vvar_print(obj);
		count++;
		ptr = ptr->next;
	}
	printf("\n%d objects in total\n", count);
}

void gc_markb(VVar o) {
	o.gc->type = (o.gc->type & VGC_TMASK) | VGC_BLACK;
}

void gc_mark() {
	VVar obj;
	GCObject* ptr = head.next;
	while (ptr != 0) {
		obj.type = ptr->type & VGC_TMASK;
		obj.gc = ptr;
	}
}

#define skipw()		if ((var.gc->type & VGC_CMASK) == VGC_BLACK) break

void gc_markvvar(VVar var) {
	switch (var.type) {
	case T_VString:
		gc_markb(var);
		break;
	case T_VList:
		skipw();
		vlist_gcmark(var);
		break;
	case T_VDict:
		skipw();
		vdict_gcmark(var);
		break;
	case T_VFunc:
		gc_mark();
		break;
	default:
		break;
	}
}

#undef skipw

static void vvar_gcsweep(GCObject* gc) {
	switch (gc->type & VGC_TMASK) {
	case T_VString:
		vstr_finalize(gc->s);
		free(gc);
		break;
	case T_VList:
		vlist_delete(gc->l);
		free(gc);
		break;
	case T_VDict:
		vdict_delete(gc->d);
		free(gc);
		break;
	case T_VFunc:
		free(gc);
		break;
	default:
		vfatal("unknown type in GC chain");
		break;
	}
}

void gc_sweep() {
	GCObject* ptr = &head;
	GCObject* nptr;
	while (ptr->next != 0) {
		nptr = ptr->next;
		if ((nptr->type & VGC_CMASK) == VGC_BLACK) { // mark white
			nptr->type = (nptr->type & VGC_TMASK) | VGC_WHITE;
			ptr = ptr->next;
		}
		else { // free
			ptr->next = nptr->next;
			vvar_gcsweep(nptr);
		}
	}
}
