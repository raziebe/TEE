#pragma once

#include "vector.h"
#include "tp_types.h"

typedef PVector PIntervals;

typedef struct
{
	size_t begin;
	size_t end;
	void*  data;
} Interval;

PIntervals IntervalsInit(void);
void IntervalsFree(PIntervals ints);
BOOLEAN IntervalsAdd(PIntervals ints, Interval *interval);
BOOLEAN IntervalsAddNoAlloc(PIntervals ints, Interval* interval);
BOOLEAN IntervalsDelete(PIntervals ints, size_t value);
void IntervalsDeleteIntersections(PIntervals ints, Interval *interval);
Interval* IntervalsFind(PIntervals ints, size_t value);
size_t IntervalsCount(PIntervals ints);
Interval* IntervalsGet(PIntervals ints, size_t index);

Interval* IntervalsFindCmp(PIntervals ints, void* val, int (*cmp)(void*,void*));
