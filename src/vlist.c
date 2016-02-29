
#include "vlist.h"
#include "vgc.h"
#include "vmisc.h"

static VList* vlist_new(unsigned int capacity) {
	capacity = capacity < VLISTSIZE ? VLISTSIZE : capacity;
	VList* l = (VList*)malloc(sizeof(VList));
	if (l == 0) {
		vfataloom("creating new list");
		return 0;
	}
	l->arr = (VVar*)malloc(capacity * sizeof(VVar));
	if (l->arr == 0) {
		free(l);
		vfataloom("creating new list");
		return 0;
	}
	l->count = 0;
	l->capacity = capacity;
	return l;
}

VVar newvlist(const int capacity) {
	VVar var;
	gc_newgcobj(&var, T_VList);
	var.gc->l = vlist_new(capacity);
	return var;
}

void vlist_delete(VList* l) {
	free(l->arr);
	free(l);
}

unsigned int vlist_count(const VVar list) {
	VList* l = list.gc->l;
	if (l == 0) return 0;
	return l->count;
}

unsigned int vlist_capacity(const VVar list) {
	VList* l = list.gc->l;
	if (l == 0) return 0;
	return l->capacity;
}

int vlist_contains(const VVar list, VVar value) {
	return vlist_indexof(list, value) >= 0;
}

int vlist_indexof(const VVar list, VVar value) {
	VList* l = list.gc->l;
	for (unsigned int i = 0; i < l->count; i++)
		if (vvar_cmp(l->arr[i], value) == 0)
			return (int)i;
	return -1;
}

VVar vlist_get(const VVar list, unsigned int index) {
	VList* l = list.gc->l;
    if (index >= l->count)
        verror("index out of range");
	return l->arr[index];
}

void vlist_set(const VVar list, unsigned int index, VVar value) {
	VList* l = list.gc->l;
    if (index >= l->count)
        verror("index out of range");
	l->arr[index] = value;
}

static void vlist_resize(VVar list, unsigned int newcapacity) {
	VList* l = list.gc->l;
	VVar* newarr = (VVar*)realloc(l->arr, newcapacity * sizeof(VVar));
	if (newarr == 0) {
		vfataloom("growing list");
		return;
	}
	l->arr = newarr;
	l->capacity = newcapacity;
}

static void vlist_ensure_capacity(VVar list, unsigned int mincap) {
	VList* l = list.gc->l;
	if (mincap > l->capacity) {
		vlist_resize(list, round_up_pow2((unsigned int)((float)mincap * 1.5f)));
	}
}

void vlist_add(VVar list, VVar value) {
	VList* l = list.gc->l;
	vlist_ensure_capacity(list, l->count + 1);
	l->arr[l->count++] = value;
}

void vlist_insert(VVar list, unsigned int index, VVar value) {
	VList* l = list.gc->l;
	vlist_ensure_capacity(list, l->count + 1);
	memmove(l->arr + index + 1, l->arr + index,
		(l->count - index) * sizeof(VVar));
	l->arr[index] = value;
	l->count++;
}

void vlist_remove(VVar list, unsigned int index) {
	VList* l = list.gc->l;
    if (index >= l->count)
        verror("list index out of range");
    memmove(l->arr + index, l->arr + index + 1,
		(l->count - index - 1) * sizeof(VVar));
	l->count--;
}

void vlist_clear(VVar list) {
	VList* l = list.gc->l;
	if (l == 0) return;
	l->count = 0;
}

int vlist_iter(const VVar list, vlist_iter_callback callback, void *obj) {
	if (callback == 0) return 0;
	VList* l = list.gc->l;
	for (unsigned int i = 0; i < l->count; ++i) {
		if (!callback(l->arr[i], obj)) {
			return 0; // callback fail
		}
	}
	return 1;
}

VVar vlist_filter(VVar list, vlist_filter_callback callback) {
	VList* l = list.gc->l;
	VVar newlist = newvlist(VLISTSIZE);
	for (unsigned int i = 0; i < l->count; i++)
		if (callback == 0 || callback(l->arr[i]))
			vlist_add(newlist, l->arr[i]);
	return newlist;
}

VVar vlist_clone(VVar list) {
	return vlist_filter(list, 0);
}

int vlist_bsearch(VVar list, const VVar value) {
	VList* l = list.gc->l;
	unsigned int lo = 0, hi = l->count - 1;
	while (lo <= hi) {
		unsigned int mid = lo + (hi - lo) / 2;
		int64_t tmp = vvar_cmp(value, l->arr[mid]);
		if		(tmp < 0) { hi = mid - 1; }
		else if (tmp > 0) { lo = mid + 1; }
		else return (int)mid;
	}
	return -((int)lo + 1);
}

static void swap(VVar list, unsigned int a, unsigned int b) {
	VList* l = list.gc->l;
	VVar temp = l->arr[a];
	l->arr[a] = l->arr[b];
	l->arr[b] = temp;
}

static unsigned int med3(VList* x, unsigned int a, unsigned int b, unsigned int c) {
	int ab = (int)vvar_cmp(x->arr[a], x->arr[b]);
	int bc = (int)vvar_cmp(x->arr[b], x->arr[c]);
	int ac = (int)vvar_cmp(x->arr[a], x->arr[c]);
	return (ab < 0 ?
		(bc < 0 ? b : ac < 0 ? c : a) :
		(bc > 0 ? b : ac > 0 ? c : a));
}

static void sort_helper(VVar list, unsigned int left, unsigned int right) {
	VList* l = list.gc->l;
	unsigned int pindex = med3(l, left, right, left + (right - left) / 2);
	VVar pivot = l->arr[pindex];

	// partition
	unsigned int i = left, j = right;
	while (i <= j) {
		while (vvar_cmp(l->arr[i], pivot) < 0)
			i++;
		while (vvar_cmp(l->arr[j], pivot) > 0)
			j--;
		if (i <= j) {
			swap(list, i, j);
			i++;
			j--;
		}
	};

	// recursion
	if (left < j)
		sort_helper(list, left, j);
	if (i < right)
		sort_helper(list, i, right);
}

void vlist_sort(VVar list) {
	VList* l = list.gc->l;
	if (l->count <= 1) return;
	sort_helper(list, 0, l->count - 1);
}

int vlist_fprint(FILE* stream, const VVar list, const VVar root, int depth) {
	VList* l = list.gc->l;
	int count = fprintf(stream, "[");
	if (depth > 1 && vvar_cmp(list, root) == 0) {
		count += fprintf(stream, "...");
	}
	else {
		for (unsigned int i = 0; i < l->count; i++) {
			if (i > 0)
				fprintf(stream, ", ");
			count += vvar_fprint1(stream, l->arr[i], root, depth);
		}
	}
	count += fprintf(stream, "]");
	return count;
}

void vlist_gcmark(VVar list) {
	gc_markb(list);
	VList* l = list.gc->l;
	for (unsigned int i = 0; i < l->count; i++)
		gc_markvvar(l->arr[i]);
}
