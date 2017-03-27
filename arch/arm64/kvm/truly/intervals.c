#include "intervals.h"
#include "osal.h"

#define INT(X) ((Interval*)VectorGet(ints, X))

PIntervals IntervalsInit()
{
	return VectorInit();
}

void IntervalsFree(PIntervals ints)
{
	size_t count = VectorCount(ints), i;
	for (i = 0; i < count; ++i)
    {
        tp_free(INT(i)->data);
		tp_free(INT(i));
    }
	VectorFree(ints);
}

static BOOLEAN get_index(PIntervals ints, Interval* interval, size_t* index)
{
    size_t count = VectorCount(ints), i;

	// skip intervals preceding ``interval''
	for (i = 0; i < count && INT(i)->end <= interval->begin; ++i);

	// verify that INT(i) and interval do not intersect
	if (i < count)
	{
		Interval *c = INT(i);
		if (interval->begin >= c->begin && interval->begin < c->end)
			return FALSE;
		if (c->begin >= interval->begin && c->begin < interval->end)
			return FALSE;
	}

    *index = i;
    return TRUE;
}

BOOLEAN IntervalsAddNoAlloc(PIntervals ints, Interval* interval)
{
    size_t i;
    if (!get_index(ints, interval, &i))
        return FALSE;
    VectorInsert(ints, i, interval);
    return TRUE;
}

BOOLEAN IntervalsAdd(PIntervals ints, Interval *interval)
{
    size_t i;
    Interval* copy;

    if (!get_index(ints, interval, &i)) {
        return FALSE;
   }

   copy = TP_NEW(Interval);
   copy->begin = interval->begin;
   copy->end = interval->end;
   copy->data = interval->data;
   VectorInsert(ints, i, copy);

   return TRUE;
}

static BOOLEAN FindByValue(PIntervals ints, size_t value, size_t *i)
{
	size_t count = VectorCount(ints);
	for (*i = 0; *i < count && INT(*i)->end <= value; ++*i);
	return *i < count && INT(*i)->begin <= value;
}

BOOLEAN IntervalsDelete(PIntervals ints, size_t value)
{
	size_t i;
	if (!FindByValue(ints, value, &i))
		return FALSE;
    tp_free(INT(i)->data);
	tp_free(INT(i));
	VectorDelete(ints, i);
	return TRUE;
}

void IntervalsDeleteIntersections(PIntervals ints, Interval *interval)
{
	size_t count = VectorCount(ints), b, e;
	for (b = 0; b < count && INT(b)->end <= interval->begin; ++b);
	for (e = b; e < count && INT(e)->begin < interval->end; ++e);
	
	//  b     b+1     b+2         e-1     e 
	// ---|  |---|  |-----| ... |-----| |----    ints
	//  |--------------------------|             interval
	// so we need to delete b,b+1,...,e-1
	for (; b < e; --e)
    {
        tp_free(INT(b)->data);
        tp_free(INT(b));
		VectorDelete(ints, b);
    }
}


Interval* IntervalsFind(PIntervals ints, size_t value)
{
	size_t i;
	return FindByValue(ints, value, &i) ? INT(i) : NULL;
}

size_t IntervalsCount(PIntervals ints)
{
	return VectorCount(ints);
}

Interval* IntervalsGet(PIntervals ints, size_t index)
{
	return VectorGet(ints, index);
}

Interval* IntervalsFindCmp(PIntervals ints, void* val, int (*cmp)(void*,void*))
{
    return VectorFind(ints, val, cmp);
}
