#ifndef __GC_PRIV_LPOOL_HXX__
#define __GC_PRIV_LPOOL_HXX__

#include <cstdlib>

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
#include "gcstat.hxx"

namespace alf {

namespace gc {

// Lpool is a pool used to manage objects that are too large
// to be stored and moved around in GCpool.
// These objects are never moved once allocated.
// Freezing these objects will simply increase a counter and
// unfreezing them will decrease that counter.
// You can regard these objects as permanently frozen except that
// they are not kept in a memory area managed by the pool.
// instead the pool only keep track of where these objects are and identify
// them properly.
class Lpool : public pool {
public:

  statistics & S_;
  gcobj ** L_;
  std::size_t n_;
  std::size_t m_;

  Lpool(statistics & S) : pool(0), S_(S), L_(0), n_(0), m_(0) { }
  ~Lpool() { cleanup(); delete [] L_; }

  Lpool & enlarge();

  head * alloc_(size_t usz, void * & ptr);
  bool dealloc_(head * h, void * p);

  void gc_walk(); // walk through all frozen large objs.
  void gc_cleanup(); // garbage collect Lpool objs.
  void gcbit_off();

  // since gc_cleanup doesn't actually delete the objects
  // we do that here.
  void gc_cleanup2();
  void cleanup(); // remove all objs (called by destructor).

  // return the index in L_ where p is found - return -1 if not found.
  ssize_t find(void * p) const;

  // destroy LOBJ at h.
  // deallocates the block.
  // does not call gcobj destructor.
  void destroy_(head * h);

  // if pointer is found in lpool, return that block.
  // otherwise, return 0.
  head * get_block_head(const void * p);

  head * get_block_head(const void * p, const void * q);

  head * get_block_head(const void * p, std::size_t sz)
  { return get_block_head(p, reinterpret_cast<const char *>(p) + sz); }

}; // end of class Lpool.

}; // end of namespace gc

}; // end of namespace alf


#endif
