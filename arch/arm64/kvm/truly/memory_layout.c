#include "memory_layout.h"
#include "osal.h"
#include "tp_string.h"
#include "common.h"
#include "vector.h"
#include "osal.h"


// ACTIVE PROCESS FUNCTIONS

static void DeallocateIntervalsIfLast(PMemoryLayout ml, PActiveProcess self);

struct _ActiveProcess
{
	size_t pid;
	PIntervals modules;
};

#define PROC(X) ((PActiveProcess)VectorGet(ml, (X)))

static int GuideSearchForProcessByPid(void* p, void *ppid)
{
	PActiveProcess proc = (PActiveProcess)p;
	size_t pid = *(size_t*)ppid;
	if (pid < proc->pid)
		return -1;
	if (pid > proc->pid)
		return 1;
	return 0;
}

static PActiveProcess GetProcess(PMemoryLayout ml, size_t pid, BOOLEAN create)
{
	size_t index;
	PActiveProcess p;

	if (VectorSortedFindIndex(ml, GuideSearchForProcessByPid, &pid, &index))
		return PROC(index);

	if (!create)
		return NULL;

	p = TP_NEW(ActiveProcess);
	VectorInsert(ml, index, p);
	p->pid = pid;
	p->modules = IntervalsInit();
	return p;
}

BOOLEAN MemoryLayoutIsIntervalExist(PMemoryLayout ml, size_t pid, size_t va)
{
    PActiveProcess p;
    return (p = GetProcess(ml, pid, FALSE)) != NULL
        && IntervalsFind(p->modules, va) != NULL;
}

BOOLEAN MemoryLayoutIsClonedProcess(PMemoryLayout ml, size_t pid)
{
    size_t i, count;
    PActiveProcess self, p;

    if ((self = GetProcess(ml, pid, FALSE)) == NULL)
        return FALSE;

    count = VectorCount(ml);
    for (i = 0; i < count; ++i)
    {
        p = PROC(i);
        if (p != self && p->modules == self->modules)
            return TRUE;
    }
    return FALSE;
}

void MemoryLayoutPrintPid(PMemoryLayout ml, size_t pid)
{
	size_t i, count;
	PActiveProcess p = GetProcess(ml, pid, FALSE);
	if (!p)
		return;
	count = IntervalsCount(p->modules);
	for (i = 0; i < count; ++i)
	{
		Interval *interval = IntervalsGet(p->modules, i);
		KdPrint(("%p - %p\n", interval->begin, interval->end));
	}
}

PMemoryLayout MemoryLayoutInit(BOOLEAN create) 
{
    PActiveProcess p;
    PMemoryLayout ml;
    
    if ((ml = VectorInit()) == NULL)
        return NULL;

    if (!create)
        return ml;

    p = TP_NEW(ActiveProcess);
    p->pid = 0;
    p->modules = IntervalsInit();

    VectorInsert(ml, 0, p);
    return ml;
}

void MemoryLayoutFree(PMemoryLayout ml) 
{
	size_t i, count = VectorCount(ml);
	for (i = 0; i < count; ++i)
	{
		PActiveProcess p = PROC(i);

        DeallocateIntervalsIfLast(ml, p);
		//IntervalsFree(p->modules);
		tp_free(p);
	}
	VectorFree(ml);
}

void MemoryLayoutNotifyImageLoad(PMemoryLayout ml,
								 size_t pid,
								 void*  data,
								 size_t base,
								 size_t length) 
{
	BOOLEAN result;
	Interval interval;
	PActiveProcess p = GetProcess(ml, pid, TRUE);
	ASSERT(p);
	interval.begin = base;
	interval.end = base + length;
	interval.data = data;
	IntervalsDeleteIntersections(p->modules, &interval);
	result = IntervalsAdd(p->modules, &interval);
	ASSERT(result);
}

void MemoryLayoutNotifyImageLoadNoAlloc(PMemoryLayout ml,
                                        size_t pid,
                                        void* data,
                                        size_t base,
                                        size_t length,
                                        Interval* interval)
{

	BOOLEAN result;
	PActiveProcess p = GetProcess(ml, pid, TRUE);
	ASSERT(p);
	interval->begin = base;
	interval->end = base + length;
	interval->data = data;
	IntervalsDeleteIntersections(p->modules, interval);
    result = IntervalsAddNoAlloc(p->modules, interval);
	ASSERT(result);
}

BOOLEAN MemoryLayoutResolve(PMemoryLayout ml,
							size_t pid,
							size_t address,
							void **out_data,
							size_t *base) 
{
	Interval* interval;
	PActiveProcess p = GetProcess(ml, pid, FALSE);
	
	if (!p)
		return FALSE;

	interval = IntervalsFind(p->modules, address);

	if (interval == NULL)
		return FALSE;

	*out_data = interval->data;
	*base = interval->begin;

	return TRUE;
}

size_t MemoryLayoutCountProcesses(PMemoryLayout ml)
{
	return VectorCount(ml);
}

PActiveProcess MemoryLayoutGetProcess(PMemoryLayout ml, size_t pid)
{
	return GetProcess(ml, pid, FALSE);
}

size_t MemoryLayoutCountModules(PActiveProcess proc)
{
	ASSERT(proc);
	return IntervalsCount(proc->modules);
}

void MemoryLayoutGetModule(PActiveProcess proc,
                           size_t index,
						   void** data,
						   size_t *base,
						   size_t *length)
{
	Interval *interval;
	ASSERT(proc);
	interval = IntervalsGet(proc->modules, index);
	ASSERT(interval);
	*data = interval->data;
	*base = interval->begin;
	*length = interval->end - interval->begin;
}

BOOLEAN MemoryLayoutFindModule(PActiveProcess proc,
                           void   *module,
                           int    (*cmp)(void*,void*),
						   void   **data,
						   size_t *base,
						   size_t *length)
{
	Interval *interval;
	ASSERT(proc);
	interval = IntervalsFindCmp(proc->modules, module, cmp);
    if (interval == NULL)
        return FALSE;
	ASSERT(interval);
	*data = interval->data;
	*base = interval->begin;
	*length = interval->end - interval->begin;
    return TRUE;
}


BOOLEAN MemoryLayoutRemoveInterval(PMemoryLayout ml, size_t pid, size_t value)
{
	PActiveProcess p;
    if ((p = GetProcess(ml, pid, FALSE)) == NULL)
        return FALSE;

    return IntervalsDelete(p->modules, value);
}

static void DeallocateIntervalsIfLast(PMemoryLayout ml, PActiveProcess self)
{

	size_t i, count = VectorCount(ml);
	for (i = 0; i < count; ++i)
	{
		PActiveProcess p = PROC(i);
        if (p != self && p->modules == self->modules)
            return;
	}
    IntervalsFree(self->modules);
}

BOOLEAN MemoryLayoutRemoveProcess(PMemoryLayout ml, size_t pid)
{
    size_t index;
    PActiveProcess process;
	if (!VectorSortedFindIndex(ml, GuideSearchForProcessByPid, &pid, &index))
		return FALSE;

    process = PROC(index);
    DeallocateIntervalsIfLast(ml, process);
    //IntervalsFree(process->modules);
    VectorDelete(ml, index);
    tp_free(process);
    return TRUE;
}

BOOLEAN MemoryLayoutCopyProcess(PMemoryLayout ml, size_t pid_to_copy, size_t new_pid)
{
    size_t index;
    PActiveProcess process, new_process;
	if (!VectorSortedFindIndex(ml, GuideSearchForProcessByPid, &pid_to_copy, &index))
		return FALSE;

    process = PROC(index);

    if (VectorSortedFindIndex(ml, GuideSearchForProcessByPid, &new_pid, &index))
        return FALSE;

	new_process = TP_NEW(ActiveProcess);
	VectorInsert(ml, index, new_process);
	new_process->pid = new_pid;
	new_process->modules = process->modules;
    return TRUE;
}
