#ifndef __GC_PRIV_GCPOOL_HXX__
#define __GC_PRIV_GCPOOL_HXX__

#include <cstdlib>

#include <string>

#include "../gc.hxx"
#include "moved.hxx"
#include "removed.hxx"
#include "Fremoved.hxx"
#include "tail.hxx"
#include "head.hxx"
#include "minipool.hxx"
#include "pool.hxx"
#include "fpool.hxx"
#include "lpool.hxx"
//#include "ptrpool.hxx"
//#include "fptrpool.hxx"
//#include "wptrpool.hxx"
#include "gcstat.hxx"

namespace alf {

namespace gc {

class PtrPool;
class WPtrPool;
class FPtrPool;

// GCpool.
class GCpool : public pool {
public:

  GCpool(statistics & S, size_t sz);
  ~GCpool();

  // resizing the pool. Will trigger a gc.
  GCpool & resize(std::size_t newsz);

  // allocates/deallocates and then updates variables.
  // also returns if we did or did not do gc during alloc_.
  head * alloc_(std::size_t usz, void * & p, bool & did_gc);
  // return true if we actually removed an obj.
  bool dealloc_(head * h, void * p);

  // just bare allocate/deallocate
  // also returns if we did or did not do gc during alloc_.
  head * alloc__(std::size_t usz, void * & p, bool & did_gc);
  bool dealloc__(head * h, void * p);

  // do gc on this pool.
  // swap active_ and other_ and move live objs in other_ to active_.
  void do_gc_(PtrPool & pp, FPtrPool & fpp, Lpool & lp,
	      Fpool & fp, WPtrPool & wp);

  // update pointers
  void do_gc_update_pointers(PtrPool & pp, FPtrPool & fpp, Lpool & lp,
			     Fpool & fp, WPtrPool & wp);

  // freeze_ is in Fpool.

  // move object from f_pool to active_.
  head * unfreeze_(Fpool & fp, PtrPool & pp, WPtrPool & wp, FPtrPool & fpp,
		   head * h, gcobj * ptr,
		   gcobj * & gcptr);

  // move object from other_ to active_
  // return ptr to new location.
  gcobj *
  move(PtrPool & pp, WPtrPool & wp, FPtrPool & fpp, head * h, void * p);

  // Move an object from h to this minipool at specified block.
  //head * move_(head * h, void * p);

  std::size_t usz_tot_freeze() const { return usz_freeze_; }
  std::size_t usz_tot_unfreeze() const { return usz_unfreeze_; }
  std::size_t usz_cur_freeze() const { return usz_freeze_ - usz_unfreeze_; }
  std::size_t sz_tot_freeze() const { return sz_freeze_; }
  std::size_t sz_tot_unfreeze() const { return sz_unfreeze_; }
  std::size_t sz_cur_freeze() const { return sz_freeze_ - sz_unfreeze_; }
  int n_tot_freeze() const { return n_freeze_; }
  int n_tot_unfreeze() const { return n_unfreeze_; }
  int n_cur_freeze() const { return n_freeze_ - n_unfreeze_; }

  // if pointer is found in a minipool, return that minipool.
  // otherwise, return 0.
  minipool * block_in_pool(const void * p);

  // p==0 always returns 0.
  // if region is found completely within a given minipool, return
  // that minipool.
  // if  region is partially within our data but not completely inside
  // one single minipool - return minipool::BAD_MINIPOOL.
  // if region is completely outside our region, return 0.
  // we assume p <= q and q == (char *)p + sizeof area
  minipool * block_in_pool(const void * p, const void * q);

  minipool * block_in_pool(const void * p, std::size_t sz)
  { return block_in_pool(p, reinterpret_cast<const char *>(p) + sz); }

private:

  statistics & S_;

  // size of area pointed to by p is twize that of A_.sz_ and B_.sz_.
  // A_ and B_ always have the same size except during a resize.
  char * p_; // the pool space used by both A_ and B_.
  minipool * active_; // which pool is the active pool
  minipool * other_; // the other pool.

  minipool A_; // the two pools.
  minipool B_;

  std::size_t p_sz; // size of area pointed to by p. (twice the pool size).

  std::size_t usz_freeze_;
  std::size_t usz_unfreeze_;
  std::size_t sz_freeze_;
  std::size_t sz_unfreeze_;

  int n_freeze_;
  int n_unfreeze_;

}; // end of class GCpool

}; // end of namespace gc

}; // end of namespace alf


#endif
