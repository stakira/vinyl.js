
#ifndef VSTRING_H
#define VSTRING_H

#include "vinyl.h"
#include "vvar.h"

typedef struct VString {
	char* p;
	size_t n;
	int32_t hash;
	size_t rc;
} VString;

VVar newvstrn(const char* s, const size_t n);
VVar newvstr(const char* s);
VVar vstr_cat(VVar* var0, VVar* var1);
void vstr_finalize(VString* s);

void strpool_print();
void strpool_gc();

int32_t vhash(const char* s, size_t n);

#endif // VSTRING_H
