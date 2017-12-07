
#include <cstring>

#include <exception>

#include "minipool.hxx"
#include "head.hxx"

alf::gc::minipool & alf::gc::minipool::resize(size_t newsz)
{
  if (usz_ == 0) {
    // if called from Fpool make sure all objs are removed from free list
    // before we call resize().
    if (del_) delete [] p_;
    p_ = new char[newsz];
    sz_ = newsz;
    del_ = true;
  }
  return *this;
}

// This is basic allocate function. Just allocate if room
// and initialize the block with head and tail.
// If no room, return 0.
alf::gc::head * alf::gc::minipool::alloc_(size_t usz, void * & p)
{
  // note - usz = user size, usz_ = used size of pool.
  size_t asz = head::asz(usz);

  // total size of allocated area.
  size_t tsz = sizeof(head) + sizeof(tail) + asz;

  if (usz_ + tsz > sz_)
    // not enough room, fail - this will typically trigger a gc()
    // or resize depending on the pool.
    return 0;

  char * hp = p_ + usz_;
  head * h = reinterpret_cast<head *>(hp);
  char * objp = hp + sizeof(head);
  char * ep = hp + tsz;
  char * tp = ep - sizeof(tail);
  tail * t = reinterpret_cast<tail *>(tp);
  usz_ += tsz; // claim the area.
  std::memset(h, 0, tsz); // clear it.

  // prepare head and tail.
  h->b_init(this, head::GCOBJ, tsz, usz);
  p = h->vp;
  return h;
}

void alf::gc::minipool::dealloc_(head * h, void * p)
{
  if (p != 0 && h != 0) {
    // destructor should have already been called, p is now blank.
    // HEAD and TAIL should be valid.
    h->p = 0;
    // we do not set flags let caller do that.
  }
}

void alf::gc::minipool::fpool_gc_walk()
{
  // walk through elements in this minipool.
  // The objects are assumed to be frozen so we count any
  // that isn't moved or removed as live objects.
  void * ep = reinterpret_cast<void *>(p_ + usz_);
  head * h = reinterpret_cast<head *>(p_);

  while (h < ep) {
    head * nexth = h->next_head();

    if (nexth > ep || h->magic != head::MAGIC)
      throw fatal_error("Corrupt minipool");

    switch (h->gctype()) {
    case head::FROZEN:

      if ((h->flags & head::GCBIT) == 0) {
	// gc_walk this object.

	h->flags |= head::GCBIT; // mark we are visiting.
	gcobj * obj = h->obj();
	obj->gc_walker("Frozen obj");
      }
      break;

    case head::UNFROZEN:
    case head::FREMOVED:

      // removed obj, just skip it.
      break;

    default:

      // FMERGED - should never occur, prev block corrupt.
      // GCOBJ, GCMOVED, GCRM, GCFROZEN - GCpool obj.
      // LOBJ, LREMOVED - Lpool obj.
      // anything else - garbage.

      throw fatal_error("Wrong gctype in head");
    }
    h = nexth;
  }
}

// walk through elements in this minipool and turn off head::GCBIT.
void alf::gc::minipool::gcbit_off()
{
  void * ep = reinterpret_cast<void *>(p_ + usz_);
  head * h = reinterpret_cast<head *>(p_);

  while (h < ep) {
    head * nexth = h->next_head();

    if (nexth > ep || h->magic != head::MAGIC)
      throw fatal_error("Corrupt minipool");

    h->flags &= ~head::GCBIT; // turn off gcbit.
    h = nexth;
  }
}

void alf::gc::minipool::gc_cleanup()
{
  // walk through the buffer and destroy any objects left there.
  // we also turn off GCBIT whenever it is on.

  const char * bufe = p_ + usz_;
  char * pp = p_;

  while (pp < bufe) {
    head * h = reinterpret_cast<head *>(pp);
    gcobj * obj = reinterpret_cast<gcobj *>(h + 1);
    std::size_t bsz = h->sz, usz;
    pp += bsz;

    switch (h->gctype()) {
    case head::GCOBJ:
      // cleanup unreachable object.
      // TODO: add counters here...
      usz = h->usz;
      obj->~gcobj(); // call destructor.
      new(obj) removed;
      h->flags = head::REMOVED | head::GCRM;
      S_.dealloc(bsz, usz);
      continue;

    case head::GCMOVED:
      // object has moved, just make sure GCBIT is off.
    case head::GCRM:
      // object already removed, just make sure GCBIT is off.
    case head::GCFROZEN:
      // object is frozen and moved, just make sure GCBIT is off.
#if 0
    case head::FROZEN:
    case head::UNFROZEN:
    case head::FREMOVED:
    case head::FMERGED:
      // should not cleanup Fpool, anyway, just make sure GCBIT is off.
    case head::LOBJ:
    case head::LREMOVED:
      // should not access Lpool from here but we turn off GCBIT.
#endif
      h->flags &= ~head::GCBIT;
      continue;
    default:
      // should not get any other values in GCpool
      throw fatal_error("Invalid object state in gc: ");
    }
  }
  if (pp > bufe)
    throw fatal_error("Invalid size in gc minipool");
  usz_ = 0;
}

// This can be called both by minipool and Fminipool.
// called by destructor.

void alf::gc::minipool::cleanup()
{
  // walk through the buffer and destroy any objects left there.
  // we also turn off GCBIT whenever it is on.

  const char * bufe = p_ + usz_;
  char * pp = p_;

  while (pp < bufe) {
    head * h = reinterpret_cast<head *>(pp);
    gcobj * obj = reinterpret_cast<gcobj *>(h + 1);
    std::size_t bsz = h->sz, usz;
    pp += bsz;

    switch (h->gctype()) {
    case head::GCOBJ:
      // cleanup object.
      usz = h->usz;
      obj->~gcobj(); // call destructor.
      new(obj) removed;
      h->flags = head::REMOVED | head::GCRM;
      S_.dealloc(bsz, usz);
      continue;

    case head::FROZEN:
      // cleanup frozen object.
      usz = h->usz;
      obj->~gcobj(); // call destructor.
      // this is called when we are shutting down so no need to place
      // in free list.
      new(obj) Fremoved;
      h->flags = head::REMOVED | head::FREMOVED;
      S_.dealloc(bsz, usz);
      S_.unfreeze(bsz, usz);
      continue;

    case head::GCMOVED:
      // object has moved, just make sure GCBIT is off.
    case head::GCRM:
      // object already removed, just make sure GCBIT is off.
    case head::GCFROZEN:
      // object is frozen and moved, just make sure GCBIT is off.
    case head::UNFROZEN:
      // Object has moved back to gcpool, turn off GCBIT.
    case head::FREMOVED:
    case head::FMERGED:
#if 0
    case head::LOBJ:
    case head::LREMOVED:
      // should not access Lpool from here but we turn off GCBIT.
#endif
      h->flags &= ~head::GCBIT;
      continue;

    default:
      // should not get any other values in GCpool
      throw fatal_error("Invalid object state in gc: ");
    }
  }
  if (pp > bufe)
    throw fatal_error("Invalid size in minipool");
  usz_ = 0;
}

// if pointer is found in this minipool, return that block.
// otherwise, return 0.
alf::gc::head *
alf::gc::minipool::get_block_head(const void * p)
{
  head * e = reinterpret_cast<head *>(p_ + usz_);

  if (p_ <= p && p <= e) {

    head * h = reinterpret_cast<head *>(p_);
    while (h < e) {
      head * nxt = h->next_head();
      if (p < nxt)
	return h;
      h = nxt;
    }
  }
  return 0;
}

// if area is found in completely inside a single block in this minipool,
// return that block.
// Otherwise, if it is overlapping several blocks, or partially inside
// one block return head::BAD_BLOCK.
// otherwise, return 0.
// we assume p <= q.
alf::gc::head *
alf::gc::minipool::get_block_head(const void * p, const void * q)
{
  head * e = reinterpret_cast<head *>(p_ + usz_);

  if (q <= p_ || e <= p)
    return 0;

  // p_ < q && p <= q && p < e
  if (p < p_ || e < q)
    return head::BAD_BLOCK;

  // p_ <= p <= q <= e and p_ < q and p < e
  head * h = reinterpret_cast<head *>(p_);

  while (h < e) {
    head * nxt = h->next_head();
    if (q <= nxt)
      return h;
    if (p < nxt)
      return head::BAD_BLOCK;
    h = nxt;
  }
  if (h > e)
    throw fatal_error("Corrupt minipool.");

  return 0;
}
