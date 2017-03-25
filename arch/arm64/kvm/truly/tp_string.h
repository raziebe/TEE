#pragma once

#include "tp_types.h"
#include "common.h"

typedef struct
{
	UINT16 length;
	char *buffer;
} String, *PString;

#define STRINGS_ARE_EQUAL(s1, s2) (((s1)->length == (s2)->length) && \
                                   (0 == TPmemcmp((s1)->buffer, \
								                  (s2)->buffer, \
								                  (s1)->length)))
#define ASSIGN_STR(S, X) do { \
							(S)->buffer = (char*)(X); \
                            (S)->length = sizeof(X) - 2; \
						 } while (0, 0)
