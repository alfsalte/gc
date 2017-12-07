#ifndef __GC_PRIV_WPTRPOOL_HXX__
#define __GC_PRIV_WPTRPOOL_HXX__

#include <cstdlib>
#include <cstring>

#include <list>
#include <string>

#include "../gc.hxx"
#include "moved.hxx"
#include "removed.hxx"
#include "Fremoved.hxx"
#include "tail.hxx"
#include "head.hxx"
#include "minipool.hxx"
#include "pool.hxx"


namespace alf {

namespace gc {

// PtrPool is a special pool to keep track of user's weak pointers.
// User can register pointers as weak pointers and gc will walk through
// each of them after gc to update them incase the object pointed to was
// removed by gc.
class WPtrPool : public pool {
public:

  WPtrPool() : pool(0), T_(0), n_(0), m_(0) { init(); }
  ~WPtrPool();

  WPtrPool & enlarge(); // increase the pointer pool

  // register a pointer.
  void wptr_register(GCpool & gc, Fpool & fp, Lpool & lp, gcobj * & p);

  // remove a registration of this pointer.
  void wptr_unregister(gcobj * & p);

  // remove all registrations of this pointer.
  void wptr_unregister_all(gcobj * & p);

  // remove all registrations of all pointers.
  void wptr_unregister_all();

  void gc_update_wptrs();

  void update_pp(head * h1, head * h2, ssize_t delta);

  int find(gcobj * & p) const;

private:

  struct entry {
    head * h;
    gcobj ** pp;
  };

  void init();

  void swap(entry & a, entry & b)
  { entry tmp = a; a = b; b = tmp; }

  gcobj * gc_update_wptr(gcobj * p);

  entry * T_;
  size_t n_; // number of elements in use
  size_t m_; // capacity of T_.

}; // end of class WPtrPool

}; // end of namespace gc

}; // end of namespace alf


#endif
