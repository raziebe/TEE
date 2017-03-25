#pragma once

#include "tp_types.h"

typedef struct Vector_ Vector, *PVector;

typedef int(*VectorGuidingCallback)(void *item, void *context);

PVector VectorInit(void);
void VectorFree(PVector v);
size_t VectorCount(PVector v);
void VectorInsert(PVector v, size_t index, void *value);
void VectorDelete(PVector v, size_t index);
void* VectorGet(PVector v, size_t index);

void* VectorFind(PVector v, void* val, int (*cmp)(void*,void*));
BOOLEAN VectorSortedFindIndex(PVector v, 
	                            VectorGuidingCallback callback,
															void *context,
															size_t *index);

