#ifndef __GC_PRIV_FREMOVED_HXX__
#define __GC_PRIV_FREMOVED_HXX__

#include <cstdlib>

#include <string>

#include "../gc.hxx"
#include "removed.hxx"

namespace alf {

namespace gc {

struct head;

// in fpool we use fremoved rather than removed so that we can
// keep a list of removed objects.
class Fremoved : public removed {
public:

  // free list pointers.
  head * next_;
  head * prev_;

  Fremoved() : next_(0), prev_(0) { }
  Fremoved(head * prv, head * nxt) : next_(nxt), prev_(prv) { }
  virtual ~Fremoved();

}; // end of class Fremoved

}; // end of namespace gc

}; // end of namespace alf


#endif
