
#include <cstdlib>

#include <new>
#include <string>
#include <stdexcept>

#include "gcpool.hxx"
#include "fpool.hxx"
#include "lpool.hxx"
#include "ptrpool.hxx"
#include "gcstat.hxx"
#include "gcstat.hxx"
#include "../../format/format.hxx"

alf::gc::GCpool::GCpool(statistics & S, std::size_t sz)
  : pool(sz), S_(S), A_(S), B_(S)
{
  active_ = 0;
  p_ = 0;
  sz_ = 0;
  resize(sz);
}

alf::gc::GCpool::~GCpool()
{
  active_->cleanup();
  delete [] p_;
}

// resizing the pool. Will trigger a gc.
alf::gc::GCpool & alf::gc::GCpool::resize(std::size_t newsz)
{
  // newsz must be at least as large so that each minipool can
  // hold twice as much data as currently in use in active pool.

  std::size_t pool_usz = active_ ? active_->usz_ : 0;
  std::size_t pool_usz2 = pool_usz + pool_usz;

  if (newsz < pool_usz2)
    newsz = pool_usz2;

  // min 32 Mb per pool
  if (newsz < 32*1024*1024)
    newsz = 32*1024*1024;

  // round up to nearest 32Mb
  newsz = (newsz + (32*1024*1024LL - 1)) & -32*1024*1024LL;

  char * oldp = p_;
  // size for two minipools + space before and after.
  std::size_t newbigsz = newsz + newsz + 3*1024;

  std::size_t aoff = 1024;
  std::size_t aend = aoff + newsz;
  std::size_t boff = aend + 1024;
  std::size_t bend = boff + newsz;
  std::size_t tsz = bend + 1024;

  char * buff = new char[tsz]; // 1k space between pools.
  std::memset(buff, 0, tsz);
  head::fill(buff, 0xdeadbeef, 1024);
  head::fill(buff + aend, 0xdeadbeef, 1024);
  head::fill(buff + bend, 0xdeadbeef, 1024);

  if (active_ == & A_) {
    // A_ is active, set B_ first, then gc stuff over to there.
    B_.use(buff + boff, newsz, false);
    // now B_ is the new pool to use.
    gc::gc(); // move stuff over there.
    // B_ should be active_ now.
    A_.use(buff + aoff, newsz, false);
  } else if (active_ == & B_) {
    // B_ is active, set A_ first, then gc stuff over to there.
    A_.use(buff + aoff, newsz);
    // now A_ is the new pool.
    gc::gc(); // move stuff over there.
    // A_ should be active_ now.
    B_.use(buff + boff, newsz);
  } else {
    // both are free, just init both.
    A_.use(buff + aoff, newsz);
    B_.use(buff + boff, newsz);
    active_ = & A_;
    other_ = & B_;

    // also init conters in this case.
    usz_freeze_ = usz_unfreeze_ = sz_freeze_ = sz_unfreeze_ = 0;
    n_freeze_ = n_unfreeze_ = 0;
  }
  delete [] p_;
  p_ = buff;
  sz_ = newsz;
  p_sz = tsz;
  return *this;
}

alf::gc::head *
alf::gc::GCpool::alloc_(std::size_t usz,
			void * & p, // ptr to allocated space
			bool & did_gc) // did we do gc::gc()?
{
  head * h = alloc__(usz, p, did_gc);
  if (h == 0)
    throw fatal_error("alloc__ returned 0 pointer");

  h->flags = head::GCOBJ;
  h->fcnt = 0;
  // got an object. Update variables.
  usz_ = active_->usz_;
  usz_alloc_ += h->usz;
  sz_alloc_ += h->sz;
  ++n_alloc_;
  return h;
}

//
bool alf::gc::GCpool::dealloc_(head * h, void * p)
{
  bool ret = false;
  // object in block h pointed to by p is to be removed.
  if (p) {
    std::size_t usz = h->usz;
    std::size_t sz = h->sz;

    ret = dealloc__(h, p);
    h->flags = head::REMOVED | head::GCRM;

    usz_dealloc_ += usz;
    sz_dealloc_ += sz;
    ++n_dealloc_;
  }
  return ret;
}

alf::gc::head *
alf::gc::GCpool::alloc__(std::size_t usz, void * & p, bool & did_gc)
{
  static gc_allocation_error M("memory allocation failure");

  did_gc = false;
  head * h = active_->alloc_(usz, p);
  if (h) return h;
  // alloc failed, do a gc and try again.
  gc::gc();
  did_gc = true;
  if ((h = active_->alloc_(usz, p)) != 0)
    return h;
  // alloc failed again, we need to resize.
  std::size_t inc = sz_ + sz_;
  if (inc < usz) inc = usz;
  resize(inc);
  if ((h = active_->alloc_(usz, p)) == 0)
    throw M;
  return h;
}

bool alf::gc::GCpool::dealloc__(head * h, void * p)
{
  // object in block h pointed to by p is to be removed.
  if (p) {
    new(p) removed;
    h->flags = head::REMOVED | head::GCRM;
    h->p = 0;
    return true;
  }
  return false;
}

// do gc on this minipool, move live objs to new active pool.

// We first swap active and other pool. After swap, active pool will be
// empty and other pool contain old pool objs.
//
// We are supposed to do the following:
// step 1, clear GCBIT on all objects in other pool. This bit is assumed
// to be 0 so we skip this step.
//
// step 2. Walk through all live objects using the root pointers in
// ptr_pool. I.e. call ptr_pool.gc_walk(). This will move all live objects
// in GCpool (h->flags & POOLMASK) == GCOBJ to other_ pool. This step
// will set the GCBIT for all live objects. We also call Fpool.gc_walk()
// as those objects are always 'live' and is therefore regarded similar to
// root pointers.
//
// step 3. Walk through other_ pool and clean up any objects not moved.
// These are no longer reachable through pointers. I.e. this is the
// garbage collection step of the garbage collector. This step will also
// clear the GCBIT for all live objects, so that we can ignore step 1
// on next gc() as well.
//
// Note that GCBIT are never set for objects on active_ pool, only on
// other_ pool and Fpool and Lpool. I.e. When we set the bit we also
// move the object to active_ pool and next runs will reach the moved
// object and never go into the actual object which is only processed
// first time when it is moved. For frozen objects or large objects we
// reach the same object each time and so as we do cleanup of this pool
// we also need to walk through Fpool to clean up GCBIT
// for those objects as well and also walk through Lpool and clean up
// any unreachable objects and turn off the GCBIT for that pool as well.
// Note that we do not garbage collect on Fpool - that's the point of
// Fpool.

// do gc on this pool, move live objs to dest.
void alf::gc::GCpool::do_gc_(PtrPool & pp, FPtrPool & fpp,
			     Lpool & lp, Fpool & fp, WPtrPool & wp)
{
  // other_ is assumed to be empty.
  // swap active_ and other_
  minipool * mp = active_;
  active_ = other_;
  other_ = mp;
  pp.gc_walk();
  fpp.gc_walk();
  fp.gc_walk();
  lp.gc_walk();
  mp->gc_cleanup();
  fp.gcbit_off();
  lp.gc_cleanup();
  wp.gc_update_wptrs();
  lp.gc_cleanup2();
}

// do gc_walk and update pointers.
void alf::gc::GCpool::do_gc_update_pointers(PtrPool & pp, FPtrPool & fpp,
					    Lpool & lp,
					    Fpool & fp, WPtrPool & wp)
{
  minipool * mp = active_;
  pp.gc_walk();
  fpp.gc_walk();
  fp.gc_walk();
  lp.gc_walk();
  // turn off gcbit on all pools.
  mp->gcbit_off();
  fp.gcbit_off();
  lp.gcbit_off();
  // update weak pointers too.
  wp.gc_update_wptrs();
}

// unfreeze an object - move it from fpool to gcpool.
// h may be a block in GCpool, Fpool or Lpool although if it is
// in GCpool the block isn't frozen at all (do nothing).
alf::gc::head *
alf::gc::GCpool::unfreeze_(Fpool & fp,
			   PtrPool & pp, WPtrPool & wp, FPtrPool & fpp,
			   head * h, gcobj * ptr,
			   gcobj * & fptr)
{
  if (h == 0 || ptr == 0) return 0;

  head * h2 = h;
  gcobj * p2 = fptr = ptr;
  bool did_gc;
  void * p;

  if (h->gctype() != head::FROZEN)
    throw fatal_error("gctype is " + h->gcflags_str() + " not FROZEN");

  // object is frozen and counter should already be 0.
  // move to us.

  // allocate space for the new obj
  h2 = alloc__(h->usz, p, did_gc);
  // should we detect if alloc did a gc and force do_gc to false
  // if so? No - because any gc when allocating will update
  // other pointers but will not update pointers to this object
  // since it hasn't been unfrozen yet.
  // if (did_gc) do_gc = false;

  // unfreeze obj at h and move it to h2.
  fp.unfreeze_(h, ptr, h2, reinterpret_cast<gcobj *>(p));
  ssize_t delta = reinterpret_cast<char *>(h2) - reinterpret_cast<char *>(h);
  pp.update_pp(h, h2, delta);
  wp.update_pp(h, h2, delta);
  fpp.update_pp(h, h2, delta);

  return h2;
}

// move object from other_ to active_
alf::gc::gcobj *
alf::gc::GCpool::move(PtrPool & pp, WPtrPool & wp, FPtrPool & fpp,
		      head * h, void * p)
{
  head * h2 = 0;
  void * p2 = 0;
  gcobj * o2;

  if (h == 0 || p == 0) return 0;

  if (h->gctype() != head::GCOBJ)
    throw fatal_error(prf::format("object type is %s - not GCOBJ",
				  h->gctype_str()));

  if (h->mp == active_)
    // object is already in active pool.
    // let it stay there and return current obj.
    return reinterpret_cast<gcobj *>(p);

  if (h->mp != other_)
    throw fatal_error("Object neither in active nor other pool.");

  h2 = active_->alloc_(h->usz, p2);
  if (h2 == 0)
    // we do not accept allcoation failure here.
    throw fatal_error("Fatal error in gc 0001");
  std::memcpy(p2, p, h->usz);
  // remove the object in p, do not call destructor, the object
  // is still alive in obj2.
  new(p) moved(h2, o2 = reinterpret_cast<gcobj *>(p2));
  // since the object is not in Fpool we know it's not frozen.
  h->flags = head::MOVED | head::GCMOVED;
  ssize_t delta = reinterpret_cast<char *>(h2) - reinterpret_cast<char *>(h);
  pp.update_pp(h, h2, delta);
  wp.update_pp(h, h2, delta);
  fpp.update_pp(h, h2, delta);

  return o2;
}
// if pointer is found in a minipool, return that minipool.
// otherwise, return 0.
alf::gc::minipool * alf::gc::GCpool::block_in_pool(const void * p)
{
  if (p < p_ || p > p_ + p_sz)
    return 0;

  if (active_ && active_->block_in_pool(p))
    return active_;

  if (other_ && other_->block_in_pool(p))
    return other_;

  return minipool::BAD_MINIPOOL;
}

// if pointer is found in a minipool, return that minipool.
// otherwise, return 0.
alf::gc::minipool *
alf::gc::GCpool::block_in_pool(const void * p, const void * q)
{
  if (q < p_ || p_ + p_sz < p)
    return 0;

  if (active_ && active_->block_in_pool(p, q))
    return active_;

  if (other_ && other_->block_in_pool(p, q))
    return other_;

  return minipool::BAD_MINIPOOL;
}
