#ifndef __GC_PRIV_MINIPOOL_HXX__
#define __GC_PRIV_MINIPOOL_HXX__

#include <cstdlib>

#include <string>

#include "../gc.hxx"
#include "moved.hxx"
#include "removed.hxx"
#include "Fremoved.hxx"
#include "tail.hxx"
#include "gcstat.hxx"

namespace alf {

namespace gc {

struct head;

// minipool is the pool used for gc and frozen to actually allocate data.
// However, it does not keep statistics that is done in the pool object below.
struct minipool {

  enum { MAGIC = 0x9bdbf3a65de79db3 };

  static
  constexpr
  minipool *
  BAD_MINIPOOL = reinterpret_cast<minipool *>(0x13f);

  std::size_t magic_;
  std::size_t sz_; // total size of pool memory.
  std::size_t usz_; // first not yet allocated byte of memory.
  char * p_; // pointer to pool memory.
  statistics & S_;
  bool del_; // delete p_ when no longer needed. (we own the pool).

  // do not allocate space for pool yet.
  minipool(statistics & S)
    : magic_(MAGIC), sz_(0), usz_(0), p_(0), S_(S), del_(false)
  { }

  // use given pool.
  minipool(statistics & S, char * p, std::size_t sz, bool d = false)
    : magic_(MAGIC), sz_(sz), usz_(0), p_(p), S_(S), del_(d)
  { }

  // create our own pool
  minipool(statistics & S, std::size_t sz)
    : magic_(MAGIC), sz_(0), usz_(0), p_(0), S_(S), del_(false)
  {
    if (sz) {
      p_ = new char[sz];
      sz_ = sz;
      del_ = true;
    }
  }

  // grab a minipool from source.
  minipool(minipool && mp)
    : magic_(MAGIC), sz_(mp.sz_), usz_(mp.usz_),
      p_(mp.p_), S_(mp.S_), del_(mp.del_)
  {
    mp.usz_ = mp.sz_ = 0;
    mp.p_ = 0;
    mp.del_ = false;
  }

  ~minipool()
  {
    cleanup();
    if (del_) delete [] p_;
  }

  minipool & use(char * p, std::size_t sz, bool del = false)
  {
    // discard old pool use p instead.
    // do nothing if pool is in use.
    if (usz_ == 0) {
      // we do not use this from Fpool, if we did we would have to
      // ensure all objects in free list from this pool is removed
      // from free list before we delete p_.
      if (del_) delete [] p_;
      p_ = p; sz_ = sz;
      del_ = del;
    }
    return *this;
  }

  minipool & set_owner(bool d = true)
  { del_ = d; return *this; }

  minipool & resize(std::size_t newsz);

  std::size_t size() { return sz_; }
  std::size_t size_used() { return usz_; } // including overhead.
  std::size_t size_available() { return sz_ - usz_; } // free space.

  // just allocate.
  head * alloc_(std::size_t usz, void * & p);
  void dealloc_(head * h, void * p);

  // Fpool gc_walk
  void fpool_gc_walk();

  // gcbit_off
  void gcbit_off();

  // Move an object from h to this minipool if there is room.
  //head * move(head * h, void * p);

  // cleanup this mini pool - used by gc to clean up old active pool.
  void gc_cleanup();

  // cleanup this mini pool completely.
  // remove all objects. This one works for Fminipools also.
  // used by destructor.
  void cleanup();

  bool magic_ok() const { return magic_ == MAGIC; }

  // if pointer is found in this minipool, return that block.
  // otherwise, return 0.
  head * get_block_head(const void * p);

  // if area is found completely inside a single block in
  // in this minipool return that head. Otherwise if it is
  // partially inside a block in this minipool return head::BAD_BLOCK
  // Otherwise, if it is completely outside of this minipool
  // return 0.
  head * get_block_head(const void * p, const void * q);

  head * get_block_head(const void * p, std::size_t sz)
  { return get_block_head(p, reinterpret_cast<const char *>(p) + sz); }

  // return true if p_ <= p <= p_ + sz_
  bool block_in_pool(const void * p) { return p_ <= p && p < p_ + sz_; }

  // return true if area is completely in pool
  bool block_in_pool(const void * p, std::size_t sz)
  {
    return p_ <= p && reinterpret_cast<const char *>(p) + sz <= p_ + sz_;
  }

  // return true if area p..q is completely in pool
  bool block_in_pool(const void * p, const void * q)
  { return p_ <= p && q <= p_ + sz_; }

  // return values:
  // 0 area is not in pool at all.
  // 1 area is partially in pool
  // 2 area is completely in pool.
  // we assume p <= q.
  inline
  int block_in_pool_(const void * p, const void * q);

  inline
  int block_in_pool_(const void * p, std::size_t sz);

}; // end of struct minipool

}; // end of namespace gc

}; // end of namespace alf


#endif
