
#include "vdict.h"
#include "vgc.h"
#include "vmisc.h"

typedef struct Pair {
	struct VVar key;
	struct VVar value;
} Pair;

typedef struct Bucket {
	unsigned int capacity;
	unsigned int count;
	Pair *pairs;
} Bucket;

struct VDict {
	unsigned int capacity;
	unsigned int count;
	Bucket *buckets;
};

static Pair* getpair(Bucket *bucket, const VVar key);
static void sizeup(VVar key);
static void sizedown(VVar key);

static VDict* vdict_new(unsigned int capacity) {
	capacity = capacity < VDICTSIZE ? VDICTSIZE : capacity;
	VDict* d = (VDict*)malloc(sizeof(VDict));
	if (d == 0) {
		vfataloom("creating dict");
		return 0;
	}
	d->buckets = (Bucket*)malloc(capacity * sizeof(Bucket));
	if (d->buckets == 0) {
		free(d);
		vfataloom("creating dict");
		return 0;
	}
	d->count = 0;
	d->capacity = capacity;
	memset(d->buckets, 0, d->capacity * sizeof(Bucket));
	return d;
}

VVar newvdict(const int capacity) {
	VVar var;
	gc_newgcobj(&var, T_VDict);
	var.gc->d = vdict_new(capacity);
	return var;
}

void vdict_delete(VDict *d) {
	if (d == 0) return;
	for (unsigned int i = 0; i < d->capacity; ++i) {
		free(d->buckets[i].pairs);
	}
	free(d->buckets);
	free(d);
}

unsigned int vdict_count(const VVar dict) {
	VDict* d = dict.gc->d;
	return d->count;
}

unsigned int vdict_capacity(const VVar dict) {
	VDict* d = dict.gc->d;
	return d->capacity;
}

int vdict_containskey(const VVar dict, const VVar key) {
	VDict* d = dict.gc->d;
	unsigned int index = vvar_hash(key) % d->capacity;
	Bucket* bucket = &(d->buckets[index]);
	Pair* pair = getpair(bucket, key);
	if (pair == 0) return 0;
	else return 1;
}

VVar vdict_get(const VVar dict, const VVar key) {
	VDict* d = dict.gc->d;
	unsigned int index = vvar_hash(key) % d->capacity;
	Bucket* bucket = &(d->buckets[index]);
	Pair* pair = getpair(bucket, key);
	return pair == 0 ? newvnull() : pair->value;
}

static void put(VDict* d, const VVar key, const VVar value) {
	unsigned int index = vvar_hash(key) % d->capacity;
	Bucket* bucket = &(d->buckets[index]);
	Pair* pair = getpair(bucket, key);
	if (pair != 0) {
		pair->value = value;
	}
	else {
		if (bucket->capacity == bucket->count) {
			unsigned int newcapacity =
				bucket->capacity == 0 ? 1 : bucket->capacity * 2;
			Pair* tmp_pairs =
				(Pair*)realloc(bucket->pairs, sizeof(Pair) * newcapacity);
			if (tmp_pairs == 0) {
				vfataloom("resizeing dict");
				return;
			}
			bucket->pairs = tmp_pairs;
			bucket->capacity = newcapacity;
		}

		bucket->pairs[bucket->count].key = key;
		bucket->pairs[bucket->count].value = value;
		bucket->count++;
		d->count++;
	}
}

void vdict_put(VVar dict, const VVar key, const VVar value) {
	sizeup(dict);
	put(dict.gc->d, key, value);
}

int vdict_remove(VVar dict, const VVar key) {
	sizedown(dict);
	VDict* d = dict.gc->d;

	unsigned int index = vvar_hash(key) % d->capacity;
	Bucket* bucket = &(d->buckets[index]);
	for (unsigned int i = 0; i < bucket->count; ++i) {
		Pair* pair = &(bucket->pairs[i]);
		if (vvar_cmp(pair->key, key) == 0) {
			bucket->count--;
			d->count--;
			bucket->pairs[i] = bucket->pairs[bucket->count];
			return 1;
		}
	}
	return 0;
}

void vdict_clear(VVar dict) {
	VDict* d = dict.gc->d;
	for (unsigned int i = 0; i < d->capacity; ++i) {
		free(d->buckets[i].pairs);
	}
	memset(d->buckets, 0, d->capacity * sizeof(Bucket));
}

int vdict_iter(const VVar dict, vdict_iter_callback callback, void *obj) {
	VDict* d = dict.gc->d;
	if (callback == 0) return 0;
	for (unsigned int i = 0; i < d->capacity; ++i) {
		Bucket *bucket = &(d->buckets[i]);
		for (unsigned int j = 0; j < bucket->count; ++j) {
			if (!callback(bucket->pairs[j].key, &(bucket->pairs[j].value), obj)) {
				return 0; // callback fail
			}
		}
	}
	return 1;
}

static int rehash_helper(const VVar key, VVar* value, void *obj) {
	VDict* other = (VDict*)obj;
	put(other, key, *value);
	return 1;
}

static void rehash(VVar dict, unsigned int capacity) {
	VDict* d = dict.gc->d;
	// skip
	if (d->capacity == capacity) return;

	// copy to new buckets
	VDict* newd = vdict_new(capacity);
	vdict_iter(dict, rehash_helper, (void*)newd);

	// free old buckets
	for (unsigned int i = 0; i < d->capacity; ++i) {
		free(d->buckets[i].pairs);
	}
	free(d->buckets);

	// use new buckets
	d->capacity = capacity;
	d->buckets = newd->buckets;
	free(newd);
}

static unsigned int getpropersize(VVar dict) {
	VDict* d = dict.gc->d;
	unsigned int ns = d->count * 4;
	ns = round_up_pow2(ns);
	ns = ns < VDICTSIZE ? VDICTSIZE : ns;
	return ns;
}

static void sizeup(VVar dict) {
	VDict* d = dict.gc->d;
	if ((float)d->capacity * 0.75f <= d->count) {
		rehash(dict, d->capacity << 1);
	}
}

static void sizedown(VVar dict) {
	VDict* d = dict.gc->d;
	if ((float)d->capacity * 0.125f > d->count) {
		rehash(dict, d->capacity >> 1);
	}
}

static Pair* getpair(Bucket *bucket, const VVar key) {
	for (unsigned int i = 0; i < bucket->count; ++i) {
		Pair* pair = &(bucket->pairs[i]);
		if (vvar_cmp(pair->key, key) == 0) {
			return pair;
		}
	}
	return 0;
}

void vdict_removeif(VVar dict, vdict_removeif_callback callback) {
	VDict* d = dict.gc->d;
	if (callback == 0) return;
	for (unsigned int i = 0; i < d->capacity; ++i) {
		Bucket *bucket = &(d->buckets[i]);
		for (unsigned int j = 0; j < bucket->count; ++j) {
			if (callback(bucket->pairs[j].key, &(bucket->pairs[j].value))) {
				bucket->count--;
				d->count--;
				bucket->pairs[j] = bucket->pairs[bucket->count];
			}
		}
	}
	rehash(dict, getpropersize(dict));
}

typedef struct VDictPrinter {
	int count;
	FILE* stream;
	VVar root;
	int depth;
} VDictPrinter;

static int fprint_helper(const VVar key, VVar* value, void *obj) {
	VDictPrinter* vdp = (VDictPrinter*)obj;
	VVar root = vdp->root;
	if (vdp->count > 1)
		vdp->count = fprintf(vdp->stream, ", ");
	vdp->count += vvar_fprint1(vdp->stream, key, root, vdp->depth);
	vdp->count += fprintf(vdp->stream, " : ");
	vdp->count += vvar_fprint1(vdp->stream, *value, root, vdp->depth);
	return 1;
}

int vdict_fprint(FILE* stream, const VVar dict, const VVar root, int depth) {
	if (depth > 1 && vvar_cmp(dict, root) == 0)
		return fprintf(stream, "{...}");
	VDictPrinter vdp;
	vdp.stream = stream;
	vdp.root = root;
	vdp.depth = depth;
	vdp.count = fprintf(stream, "{");
	vdict_iter(dict, fprint_helper, (void*)&vdp);
	vdp.count += fprintf(stream, "}");
	return vdp.count;
}

static int vdict_gcmark_helper(const VVar key, VVar* value, void *obj) {
	gc_markvvar(key);
	gc_markvvar(*value);
	return 1;
}

void vdict_gcmark(VVar dict) {
	gc_markb(dict);
	vdict_iter(dict, vdict_gcmark_helper, 0);
}
