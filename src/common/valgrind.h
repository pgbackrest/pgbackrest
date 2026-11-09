/***********************************************************************************************************************************
Valgrind Memcheck Client Requests

Memcheck tracks the state of memory using the requests made to the allocator, which is not enough when part of an allocation is
invalidated without being freed, e.g. when a buffer shrinks. In this case memory that should be undefined still looks valid to
memcheck and errors go undetected. The client requests below update the state directly so detection remains accurate.

The requests are compiled in for debug builds when the Valgrind header is available, which includes the unit tests, and are no-ops
when not run under Valgrind. When the header is not available they compile to nothing.
***********************************************************************************************************************************/
#ifndef COMMON_VALGRIND_H
#define COMMON_VALGRIND_H

#ifdef WITH_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, size)
#endif

#endif
