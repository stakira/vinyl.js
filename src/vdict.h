
#ifndef VDICT_H
#define VDICT_H

#include "vinyl.h"
#include "vvar.h"

typedef struct VDict VDict;

VVar newvdict(const int capacity);
void vdict_delete(VDict* d);

unsigned int vdict_count(const VVar dict);
unsigned int vdict_capacity(const VVar dict);

int vdict_containskey(const VVar dict, const VVar key);
struct VVar vdict_get(const VVar dict, const VVar key);
void vdict_put(VVar dict, const VVar key, const VVar value);
int vdict_remove(VVar dict, const VVar key);
void vdict_clear(VVar dict);

typedef int(*vdict_iter_callback)(const VVar key, VVar* value, void *obj);
int vdict_iter(const VVar dict, vdict_iter_callback callback, void *obj);

typedef int(*vdict_removeif_callback)(const VVar Key, VVar* value);
void vdict_removeif(VVar dict, vdict_removeif_callback callback);

int vdict_fprint(FILE* stream, const VVar dict, const VVar root, int depth);

void vdict_gcmark(VVar dict);

#endif // VDICT_H
