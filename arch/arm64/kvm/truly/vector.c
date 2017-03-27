#include "vector.h"
#include "osal.h"

struct Vector_ {
	void **arr;
	size_t count;
	size_t size;
};

PVector VectorInit() {
	PVector v = TP_NEW(Vector);
	v->count = 0;
	v->size = PAGE_SIZE / sizeof(void*);
	v->arr = tp_alloc(PAGE_SIZE);
	return v;
}

void VectorFree(PVector v) {
	tp_free(v->arr);
	tp_free(v);
}

size_t VectorCount(PVector v) {
	return v->count;
}

void VectorInsert(PVector v, size_t index, void *value) {
	size_t i;

	if (v->count == v->size) {
		void **new_arr = tp_alloc(v->size * sizeof(void*) * 2);
		for (i = 0; i < v->size; ++i)
			new_arr[i] = v->arr[i];
		tp_free(v->arr);
		v->arr = new_arr;
		v->size *= 2;
	}
	
	for (i = v->count; i > index; --i)
		v->arr[i] = v->arr[i - 1];
	
	v->arr[index] = value;

	++v->count;
}

void VectorDelete(PVector v, size_t index) {
	size_t i;

	for (i = index + 1; i < v->count; ++i)
		v->arr[i - 1] = v->arr[i];
	
	--v->count;
}

void* VectorGet(PVector v, size_t index) {
	return v->arr[index];
}

void* VectorFind(PVector v, void* val, int (*cmp)(void*,void*))
{
    size_t i;
    for (i = 0; i < v->count && cmp(v->arr[i], val) ; ++i);

    return i >= v->count ? NULL : v->arr[i];
}

BOOLEAN VectorSortedFindIndex(  PVector v,
                                VectorGuidingCallback func,
								void *context,
								size_t *index) 
{
	size_t left = 0, right = v->count;
	while (left < right) {
		size_t m = (left + right) / 2;
		int result = func(v->arr[m], context);
		if (result < 0) {
			right = m;
		} else if (result > 0) {
			left = m + 1;
		} else {
			*index = m;
			return TRUE;
		}
	}
	*index = left;
	return FALSE;
}
