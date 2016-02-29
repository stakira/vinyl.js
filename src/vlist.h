
#ifndef VLIST_H
#define VLIST_H

#include "vinyl.h"
#include "vvar.h"

typedef struct VList VList;

struct VList {
	unsigned int count;
	unsigned int capacity;
	VVar* arr;
};

VVar newvlist(const int capacity);
void vlist_delete(VList* l);

unsigned int vlist_count(const VVar list);
unsigned int vlist_capacity(const VVar list);

int vlist_contains(const VVar list, VVar value);
int vlist_indexof(const VVar list, VVar value);
VVar vlist_get(const VVar list, unsigned int index);
void vlist_set(const VVar list, unsigned int index, VVar value);
void vlist_add(VVar list, VVar value);
void vlist_insert(VVar list, unsigned int index, VVar value);
void vlist_remove(VVar list, unsigned int index);
void vlist_clear(VVar list);

typedef int(*vlist_iter_callback)(VVar value, void* obj);
int vlist_iter(const VVar list, vlist_iter_callback callback, void* obj);

typedef int(*vlist_filter_callback)(VVar value);
VVar vlist_filter(VVar list, vlist_filter_callback callback);

VVar vlist_clone(VVar list);

int vlist_bsearch(VVar list, const VVar value);
void vlist_sort(VVar list);

int vlist_fprint(FILE* stream, const VVar list, const VVar root, int depth);

void vlist_gcmark(VVar list);

#endif // VLIST_H
