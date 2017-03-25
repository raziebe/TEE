#pragma once

#include "tp_types.h"
#include "tp_string.h"
#include "vector.h"
#include "intervals.h"

typedef PVector PMemoryLayout;
typedef struct _ActiveProcess ActiveProcess, *PActiveProcess;
PMemoryLayout MemoryLayoutInit(BOOLEAN create);

void MemoryLayoutFree(PMemoryLayout ml);

void MemoryLayoutNotifyImageLoad(PMemoryLayout ml,
								 size_t pid,
								 void* data,
								 size_t base,
								 size_t length);

void MemoryLayoutNotifyImageLoadNoAlloc(PMemoryLayout ml,
                                        size_t pid,
                                        void* data,
                                        size_t base,
                                        size_t length,
                                        Interval* interval);
BOOLEAN MemoryLayoutResolve(PMemoryLayout ml,
                            size_t pid,
							size_t address, 
							void **module_id,
							size_t *base);

void MemoryLayoutPrintPid(PMemoryLayout ml, size_t pid);

size_t MemoryLayoutCountProcesses(PMemoryLayout ml);

PActiveProcess MemoryLayoutGetProcess(PMemoryLayout ml, size_t pid);

BOOLEAN MemoryLayoutCopyProcess(PMemoryLayout ml, size_t pid_to_copy, size_t new_pid);

BOOLEAN MemoryLayoutIsClonedProcess(PMemoryLayout ml, size_t pid);

size_t MemoryLayoutCountModules(PActiveProcess proc);

void MemoryLayoutGetModule(PActiveProcess proc,
                           size_t index,
						   void** data,
						   size_t *base,
						   size_t *length);

BOOLEAN MemoryLayoutFindModule (PActiveProcess proc,
                             void   *module,
                             int    (*cmp)(void*,void*),
						     void   **data,
						     size_t *base,
						     size_t *length);

BOOLEAN MemoryLayoutRemoveInterval(PMemoryLayout ml, size_t pid, size_t value);
BOOLEAN MemoryLayoutRemoveProcess(PMemoryLayout ml, size_t pid);
BOOLEAN MemoryLayoutIsIntervalExist(PMemoryLayout ml, size_t pid, size_t va);
