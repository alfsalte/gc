#ifndef __GC_PRIV_FPOOL_HXX__
#define __GC_PRIV_FPOOL_HXX__

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
// #include "ptrpool.hxx"
// #include "fptrpool.hxx"
// #include "wptrpool.hxx"
#include "gcstat.hxx"

namespace alf {

namespace gc {

class PtrPool;
class WPtrPool;
class FPtrPool;

// Fpool is the frozen pool, objects are moved to there when frozen
// and moved back to gcpool when unfrozen.
// Note that large objects are allocated in lpool and never moved.
class Fpool : public pool {
public:

  Fpool(statistics & S, std::size_t sz);
  ~Fpool();

  // resizing Fpool means add another minipool to our list.
  // we never deallocate any minipool except when program exit.
  Fpool & enlarge(std::size_t inc);

  bool dealloc_(head * h, void * p);

  // called by GCpool::freeze.
  // do_gc is normally true. Exception is if you do several freeze()
  // and unfreeze() in a row you may set it to false on all except the
  // last of these so that gc is only done once at end after all are done.
  head *
  freeze_(PtrPool & pp, WPtrPool & wp, FPtrPool & fpp,
	  head * h, gcobj * p, gcobj * & p2);

  // already allocated space for obj in h2, move it to there.
  // h is block in Fpool. h2 is block in GCpool.
  // p is pointer to obj in Fpool. p2 is pointer to obj in GCpool.
  void
  unfreeze_(head * h, gcobj * p, head * h2, gcobj * p2);

  void gc_walk();
  void gcbit_off(); // clear GCBIT on objs.

  // functions to manage free list.
  void link_free(head * h); // link object into free list.
  void unlink_free(head * h); // unlink object from free list.

  // if two consecutive blocks are both unused, we can merge them
  // This merge is only done after a gc::gc(), so that we are sure
  // that the unused block does not have any pointers to it.
  // I.e. If an object is not in use and gc() is run, then
  // it is considered safe to discard this head object and it can
  // be merged with the block before it. The first of the two blocks
  // are not that important other than that it must be unused
  // as it will still have its header after the merge.
  // This creates a large block which can be used again by some alloc.
  // calling merge too soon will confuse matters when a pointer
  // to the beginning of the vanished block tries to access its head
  // which isn't there any more.
  // This will normally not be a problem since we can keep the head
  // data and changed the gctype to MERGED meaning go to prev head
  // and examine it to contain the full block. Further a dangling pointer
  // just have to be set to 0 as the object is already removed
  // if the block is allocated again, the block is generally not split
  // in two unless the usz is a lot smaller than the available space.

  // merge h and the next block
  void merge_next(head * h);

  // merge prev and h.
  void merge_prev(head * h);

  // split h into two blocks, return ptr to new block.
  // first block start at h and is size oldsize - sz
  // second block start at (char *)h + newsz and is
  // size sz. The newsz + sz == oldsize.
  head * split(head * h, std::size_t sz);

  // h is assumed to point to a free element - return
  // a pointer to the Fremoved object if so.
  static Fremoved * get_free(head * h);

  // if pointer is found in a minipool, return that minipool.
  // otherwise, return 0.
  minipool * block_in_pool(const void * p);
  minipool * block_in_pool(const void * p, const void * q);

  minipool * block_in_pool(const void * p, std::size_t sz)
  { return block_in_pool(p, reinterpret_cast<const char *>(p) + sz); }

private:

  typedef std::list<minipool * > pool_list_type;
  typedef pool_list_type::iterator pool_iterator;

  // h and nxt are two consecutive blocks to be merged.
  void merge_(head * h, head * nxt); // with some checks.
  void merge__(head * h, head * nxt); // without checks.

  statistics & S_;

  // our list of minipools.
  std::list<minipool *> F_;

  // fpool keep track of a list of free blocks
  // sorted on addresses and consecutive blocks are merged.
  head * free_; // free list for Fpool

}; // end of class Fpool.

}; // end of namespace gc

}; // end of namespace alf


#endif
