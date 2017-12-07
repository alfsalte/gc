
#include <sys/time.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>

#include <iostream>
#include <string>
#include <new>
#include <exception>

#include "../gc.hxx"

#include "moved.hxx"
#include "removed.hxx"
#include "fremoved.hxx"
#include "tail.hxx"
#include "minipool.hxx"
#include "head.hxx"
#include "pool.hxx"
#include "gcpool.hxx"
#include "fpool.hxx"
#include "lpool.hxx"
#include "ptrpool.hxx"
#include "fptrpool.hxx"
#include "wptrpool.hxx"
#include "gcstat.hxx"

namespace alf {
namespace gc {

bool deallocate_(void * ptr);

};
};

#include "moved.cxx"
#include "removed.cxx"
#include "fremoved.cxx"
#include "head.cxx"
#include "tail.cxx"
#include "minipool.cxx"
#include "pool.cxx"
#include "gcpool.cxx"
#include "fpool.cxx"
#include "lpool.cxx"
#include "ptrpool.cxx"
#include "fptrpool.cxx"
#include "wptrpool.cxx"
#include "gcstat.cxx"
#include "gcerror.cxx"
#include "dangling_pointer.cxx"
#include "gc_allocation_error.cxx"
#include "gcobj.cxx"
#include "gcdataobj.cxx"
#include "gcpriv.cxx"
