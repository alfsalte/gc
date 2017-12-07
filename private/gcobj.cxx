
#include "../gc.hxx"

#include "head.hxx"

#include "gcstat.hxx"

// virtual
alf::gc::gcobj::~gcobj()
{
  // replace ourselves with removed so that we do not
  // trigger destructor again on same obj.
  // If we are in frozen pool Fpool will overwrite this
  // with an Fremoved object to link into free list.
  new(this) removed;
}
