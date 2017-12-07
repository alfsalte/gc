#ifndef __GC_PRIV_POOL_HXX__
#define __GC_PRIV_POOL_HXX__

#include <cstdlib>

#include <string>

#include "../gc.hxx"
#include "moved.hxx"
#include "removed.hxx"
#include "Fremoved.hxx"
#include "tail.hxx"
#include "head.hxx"
#include "minipool.hxx"

namespace alf {

namespace gc {

// base class for all pools.
// This class does not actually hold or manage the pool, that is
// done in subclass.
class pool {
public:

  pool(std::size_t sz);
  ~pool() { }

  std::size_t size() const { return sz_; } // total size including overhead.
  std::size_t size_used() const { return usz_; } // including overhead.
  std::size_t size_available() const { return sz_ - usz_; } // free space.

  // statistics for user space (usz arg).
  std::size_t usz_tot_alloc() const { return usz_alloc_; }
  std::size_t usz_tot_dealloc() const { return usz_dealloc_; }
  std::size_t usz_cur_alloc() const { return usz_alloc_ - usz_dealloc_; }

  // same but including overhead HEADs and TAILs of allocations.
  std::size_t sz_tot_alloc() const { return sz_alloc_; }
  std::size_t sz_tot_dealloc() const { return sz_dealloc_; }
  std::size_t sz_cur_alloc() const { return sz_alloc_ - sz_dealloc_; }

  int n_tot_alloc() const { return n_alloc_; } // number of allocs.
  int n_tot_dealloc() const { return n_dealloc_; } // number of deallocs.
  int n_cur_alloc() const { return n_alloc_ - n_dealloc_; }

protected:

  // does no resize, just update variables.
  void resize_(std::size_t newsz) { sz_ = newsz; }
  void grow_(std::size_t inc) { sz_ += inc; }

  // no alloc or dealloc, just update variables.
  void alloc_(std::size_t usz, std::size_t bsz);
  void dealloc_(std::size_t usz, std::size_t bsz);

  std::size_t sz_; // size of pool
  std::size_t usz_; // size of used part of pool including overhead.

  std::size_t usz_alloc_; // total size allocated (h->usz).
  std::size_t usz_dealloc_; // total size deallocated (h->usz).
  std::size_t sz_alloc_; // total size allocated (h->sz)
  std::size_t sz_dealloc_; // total size deallocated (h->sz).

  int n_alloc_; // number of calls to alloc.
  int n_dealloc_; // number of calls to dealloc.

}; // end of class pool

}; // end of namespace gc

}; // end of namespace alf


#endif
