#ifndef __GC_PRIV_PTRPOOL_HXX__
#define __GC_PRIV_PTRPOOL_HXX__

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
#include "gcpool.hxx"
#include "fpool.hxx"
#include "lpool.hxx"

namespace alf {

namespace gc {

// PtrPool is a special pool to keep track of user's top-level ptrs.
// User can register pointers as root pointers and gc will walk through
// each of them to detect live objects.
class PtrPool : public pool {
public:

  PtrPool() : pool(0), T_(0), n_(0), m_(0) { init(); }

  ~PtrPool();

  PtrPool & enlarge(); // increase the pointer pool

  // register a pointer.
  void ptr_register(const std::string & txt, gcobj ** pp,
		    GCpool & gcp, Fpool & fp, Lpool & lp);

  // remove a registration of this pointer. If you have registered
  // the same pointer multiple times you should call unregister for each
  // register.
  void ptr_unregister(gcobj ** pp);

  // remove all registrations of this pointer.
  void ptr_unregister_all(gcobj ** pp);

  // remove all registrations of all pointers.
  void ptr_unregister_all();

  void gc_walk();

  void update_pp(head * h1, head * h2, ssize_t delta);

private:

  void init();

  struct entry {
    std::string txt;
    // head for the block that own the pointer.
    // if pointer is not in a gcobj block, this value is 0.
    head * h;
    gcobj ** pp;

    entry(const std::string & t, head * hh, gcobj ** p)
      : txt(t), h(hh), pp(p)
    { }

    entry(const entry & e) : txt(e.txt), h(e.h), pp(e.pp) { }
    entry(entry && e) : txt(std::move(e.txt)), h(e.h), pp(e.pp) { }

    entry & operator = (const entry & e)
    { txt = e.txt; h = e.h; pp = e.pp; return *this; }

    entry & operator = (entry && e)
    { txt = std::move(e.txt); h = e.h; pp = e.pp; return *this; }

    void * operator new(std::size_t, void * p) { return p; }

    entry & clear() { txt = std::string(); h = 0; pp = 0; return *this; }

  }; // end of struct entry

  void swap(entry & a, entry & b)
  { entry tmp = a; a = b; b = tmp; }

  entry * T_;
  size_t n_; // number of elements in use
  size_t m_; // capacity of T_.

}; // end of class PtrPool



}; // end of namespace gc

}; // end of namespace alf


#endif
