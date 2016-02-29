
#include "vinyl.h"
#include "vgc.h"
#include "vdict.h"
#include "vstring.h"
#include "vmisc.h"

VVar strpool = { 0, {0} };

static VString* vstr_new(const char* s, size_t n, int32_t hash) {
	VString* str = (VString*)malloc(sizeof(VString));
	if (str == 0) {
		vfataloom("creating string");
		return 0;
	}
	str->p = strdup(s);
	str->n = strlen(s);
	str->hash = hash;
	str->rc = 0;
	return str;
}

static void vstr_delete(VString* str) {
	free(str->p);
	free(str);
}

static void strpool_addstr(VString** s) {
	VVar newstr, oldstr;
	gc_newgcobj_noreg(&newstr, T_VString);
	newstr.gc->s = *s;

	if (strpool.type == 0) {
		strpool = newvdict(0);
	}

	oldstr = vdict_get(strpool, newstr);

	if (oldstr.type != T_VNull) {
		*s = oldstr.gc->s;
		vstr_delete(newstr.gc->s);
		free(newstr.gc);
	}
	else {
		gc_reg(&newstr);
		vdict_put(strpool, newstr, newstr);
		(*s)->rc++;
	}
}

static int strpool_print_callback(const VVar k, VVar* v, void *obj) {
	printf("  \"%s\" n %ld rc %ld @0x%lx h %x ok %d\n",
		k.gc->s->p, k.gc->s->n, k.gc->s->rc, (size_t)k.gc->s, k.gc->s->hash, k.gc->s == v->gc->s);
	return 1;
}

void strpool_print() {
	printf("str pool count %d\n", vdict_count(strpool));
	vdict_iter(strpool, strpool_print_callback, 0);
}

static int strpool_gc_helper(const VVar Key, VVar* value) {
	if (Key.gc->s->rc == 1) {
		Key.gc->s->rc--;
		return 1;
	}
	else return 0;
}

void strpool_gc() {
	vdict_removeif(strpool, strpool_gc_helper);
	gc_markvvar(strpool);
}

static VString* vstr_fromutf8(const char* s, size_t n) {
	VString* str;
	str = vstr_new(s, n, vhash(s, n < VSTRLEN ? n : VSTRLEN));

	if (n < VSTRLEN) {
		strpool_addstr(&str);
	}
	str->rc++;
	return str;
}

VVar newvstrn(const char* s, const size_t n) {
	VVar var;
	gc_newgcobj(&var, T_VString);
	var.gc->s = vstr_fromutf8(s, n);
	return var;
}

VVar newvstr(const char* s) {
	return newvstrn(s, strlen(s));
}

VVar vstr_cat(VVar* var0, VVar* var1) {
	unsigned int n = var0->gc->s->n + var1->gc->s->n;
	char* s = (char*)malloc(n + 1);
	strcpy(s, var0->gc->s->p);
	strcat(s, var1->gc->s->p);
	VVar result = newvstrn(s, n);
	free(s);
	return result;
}

void vstr_finalize(VString* s) {
	s->rc--;
	if (s->rc == 0)
		vstr_delete(s);
#ifdef _DEBUG
	else if (s->rc < 0)
		vfatal("string ref count %d < 0!", s->rc);
#endif
}

#include "cmetrohash.h"

int32_t vhash(const char* s, size_t n) {
	int64_t hash;
	cmetrohash64((uint8_t*)s, n, 0, (uint8_t*)(&hash));
	return (int32_t)(hash - (hash >> 32));
}
