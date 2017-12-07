#ifndef __GC_PRIV_FPTRPOOL_HXX__
#define __GC_PRIV_FPTRPOOL_HXX__

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

class GCpool;
class Fpool;
class Lpool;

// PtrPool is a special pool to keep track of special functions
// to call during gc_walk. Say you have a struct that does not inherit
// from gcobj (since you don't have it on heap for example) but it contains
// many pointers to gcobj objects. Instead of registering each of them
// separately, you can register a single function that does gc_walk
// on that object. The object is never allocated so gc will never do
// gc_walk on it as such but it will call your function and that function
// can then do gc_walk on each of the pointers.
class FPtrPool : public pool {
public:

  FPtrPool() : pool(0), T_(0), n_(0), m_(0) { init(); }
  ~FPtrPool();

  FPtrPool & enlarge(); // increase the pointer pool

  // register an object and a function.
  // The function takes two arguments - a descriptive string
  // and a pointer to that data object
  void fun_register(GCpool & gcp, Fpool & fp, Lpool & lp,
		    const std::string & txt,
		    void * obj,
		    void (* gc_walk)(const std::string &, void *));

  // remove a registration of this pointer. If you have registered
  // the same pointer multiple times you should call unregister for each
  // register.
  void fun_unregister(void * d);

  // remove all registrations of this pointer.
  void fun_unregister_all(void * d);

  // remove all registrations of all pointers.
  void fun_unregister_all();

  void gc_walk();

  void update_pp(head * h1, head * h2, ssize_t delta);

private:

  void init();

  struct entry {
    std::string txt;
    head * h;
    void * obj;
    void (* f)(const std::string & s, void * d);

    entry(const std::string & t, head * h, void * o,
	  void f_(const std::string &, void *))
      : txt(t), obj(o), f(f_)
    { }

    entry(const entry & e) : txt(e.txt), h(e.h), obj(e.obj), f(e.f) { }
    entry(entry && e) : txt(std::move(e.txt)), h(e.h), obj(e.obj), f(e.f) { }

    entry & operator = (const entry & e)
    { txt = e.txt; h = e.h; obj = e.obj; f = e.f; return *this; }

    entry & operator = (entry && e)
    { txt = std::move(e.txt); h = e.h; obj = e.obj; f = e.f; return *this; }

    void * operator new(std::size_t, void * p) { return p; }

    entry &
    clear()
    { txt = std::string(); h = 0; obj = 0; f = 0; return *this; }

  }; // end of struct entry

  void swap(entry & a, entry & b)
  { entry tmp = a; a = b; b = tmp; }

  entry * T_;
  size_t n_; // number of elements in use
  size_t m_; // capacity of T_.

}; // end of class FPtrPool

}; // end of namespace gc

}; // end of namespace alf


#endif
