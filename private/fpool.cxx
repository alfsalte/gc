
#include <cstdlib>

#include <new>
#include <string>
#include <stdexcept>

#include "gcpool.hxx"
#include "fpool.hxx"
#include "gcstat.hxx"
#include "../../format/format.hxx"

// Fpool is the frozen pool, objects are moved to there when frozen
// and moved back to gcpool when unfrozen.
// Note that large objects are allocated in lpool and never moved.

alf::gc::Fpool::Fpool(statistics & S, std::size_t sz)
  : pool(sz), S_(S)
{
  minipool * a = new minipool(S, sz);
  F_.push_back(a);
  free_ = 0;
}

alf::gc::Fpool::~Fpool()
{
  // kill the list of pools.
  // not much point in doing it in reverse order since allocations
  // may be randomly among the pools anyway.
  pool_iterator a = F_.begin();
  while (a != F_.end()) {
    minipool * p = *a;
    delete p; // destructor will remove objects in pool.
    ++a;
  }
  // since all objects are gone, free list is no longer valid or needed.
  free_ = 0;
}

// resizing Fpool means add another minipool to our list.
// we never deallocate any minipool except when program exit.
alf::gc::Fpool & alf::gc::Fpool::enlarge(size_t inc)
{
  minipool * p = new minipool(S_, inc);
  F_.push_back(p);
  return *this;
}

// called to delete frozen obj.
bool alf::gc::Fpool::dealloc_(head * h, void * p)
{
  bool ret = false;

  if (h != 0 && p != 0) {
    // We assume destructor has already been called.
    switch (h->gctype()) {
    case head::FROZEN:
      // deallocate frozen obj.
      // place block into free list.
      new(p) Fremoved();
      h->flags = head::REMOVED | head::FREMOVED;
      link_free(h);
      ret = true;
      break;

    case head::UNFROZEN:
      // moved back to gc pool.
      return gc::deallocate_(h->p);

    case head::FREMOVED:
    case head::FMERGED:
      // already removed.
      return false;

    default:
      throw fatal_error("Wrong gctype - got " + h->gcflags_str());

    }
  }
  return ret;
}

// called by GCpool::freeze.
// h is existing GCpool hdr.
// p is pointer to GCpool obj.
// p2 receives the ptr to Fpool obj.
// return Fpool hdr for p2.
alf::gc::head *
alf::gc::Fpool::freeze_(PtrPool & pp, WPtrPool & wp, FPtrPool & fpp,
			head * h, gcobj * p, gcobj * & p2)
{
  static gc_allocation_error M("Fatal error, "
			       "cannot allocate object to freeze");

  // Find a pool with enough space, i.e. get a block with room for obj.
  std::size_t usz = h->usz;
  std::size_t sz = h->sz;
  // start by traversing free list.
  head * freep = free_;
  head * newh = 0;
  void * newp = 0;
  gcobj * newobj = 0;
  minipool * mp = 0;

  while (freep) {
    if (freep->sz >= sz) {
      // block is large enough.
      mp = (newh = freep)->mp;
      newobj = newh->obj();
      if (newh->sz >= sz + head::MINBLKSZ) {
	// large enough to split.
	// split off end and keep initial part in free list.
	newh = split(newh, sz); // new is new block large enough for us.
	break;
      }
      unlink_free(newh);
      break;
    }
    Fremoved * freeobj = freep->obj_Frm();
    freep = freeobj->next_;
  }

  if (newh == 0) {
    // nothing in free list, go through each pool and see if any has
    // free room.
    pool_iterator pp = F_.begin();
    while (pp != F_.end()) {
      mp = *pp;
      ++pp;
      if ((newh = mp->alloc_(usz, newp)) != 0) {
	newobj = reinterpret_cast<gcobj *>(newp);
	break;
      }
    }

    if (newh == 0) {
      // still nothing, try to enlarge the pool.
      std::size_t isz = F_.front()->size();
      if (isz < usz) isz = (usz + usz + 64*1024*1024 - 1) & -64*1024*1024;
      enlarge(isz);
      mp = F_.back();
      newh = mp->alloc_(usz, newp);
      if (newh == 0)
	throw M;
      newobj = reinterpret_cast<gcobj *>(newp);
    }
  }

  // Now we have got a chunk of memory large enough to hold the object.
  // Let's move it there.
  std::memcpy(newobj, p2, usz);
  // tell GCpool block that we have moved.
  new (p) moved(newh, newobj);
  h->p = newobj;
  h->flags = head::MOVED | head::GCFROZEN;
  h->fcnt = 0;
  newh -> p = newobj;
  newh -> flags = head::FROZEN;
  newh -> fcnt = 1;
  newh -> mp = mp;
  p2 = newobj;
  ssize_t delta = reinterpret_cast<char *>(newh) - reinterpret_cast<char *>(h);
  pp.update_pp(h, newh, delta);
  wp.update_pp(h, newh, delta);
  fpp.update_pp(h, newh, delta);

  return newh;
}

// h is Fpool block containing objet p.
// p2 receives the moved object in GCpool.
// do_gc is normally true. Exception is if you do multiple freeze() and
// unfreeze() in a row, you might want to wait with gc until all of those
// are done so you set it in false on all except the last.
// If you have other pointers to the frozen object, you will need to run
// gc::gc() to get those pointers updated which unfreeze_() will do for you
// if you set do_gc to true.

void
alf::gc::Fpool::unfreeze_(head * h, gcobj * p, head * h2, gcobj * p2)
{
  // h2 is already allocated, move obj to there and free up h
  // h is in Fpool, h2 is in GCpool.
  std::memcpy(p2, p, h->usz);
  h2->usz = h->usz;
  h2->p = h->p = p2;
  new(p) Fremoved();
  h -> flags = head::REMOVED | head::UNFROZEN;
  link_free(h); // place block in free list.
  h2->flags = head::GCOBJ;
  h2->fcnt = 0;
}

void alf::gc::Fpool::gc_walk()
{
  // Walk through all objects in Fpool and walk any objects found.
  pool_iterator p = F_.begin();
  while (p != F_.end()) {
    minipool * mp = *p;
    ++p;
    mp->fpool_gc_walk();
  }
}

void alf::gc::Fpool::gcbit_off()
{
  // Walk through all objects in Fpool and turn off head::GCBIT.
  pool_iterator p = F_.begin();
  while (p != F_.end()) {
    minipool * mp = *p;
    ++p;
    mp->gcbit_off();
  }
}

// link object into free list.
// will also attempt to merge it with prev and/or next.
void alf::gc::Fpool::link_free(head * h)
{
  if (h == 0) return;

  head * prv = 0, * p = free_;
  Fremoved * hobj = h->obj_Frm_safer();
  Fremoved * prvobj = 0;
  Fremoved * nxtobj = 0;

  while (p != 0 && p < h) {
    prv = p;
    prvobj = prv->obj_Frm_safer();
    p = prvobj->next_;
  }
  if (p == h)
    return; // already in free list.
  // p == 0 or p >= h and prv < h.
  // check if h is within prv block.
  head * after_prv = 0;
  if (prv != 0 && (after_prv = prv->next_head()) >  h) {
    // yes, so we are already in free list - set ourselves as FMERGED.
    h->flags = head::FMERGED;
    return;
  }

  // if h is immediately after prv, we can merge the two
  if (h == after_prv) {

    // since we merge it we do not insert it into list.
    // the prv block will increase in size and is already in the list.
    merge_next(prv); // prv's next block is h (i.e. our block).
    h = prv; // adjust h to prv - our new cur block (already in list).
    hobj = prvobj;

  } else {
    // h > after_prv - insert h into list right after prv but before
    // next block in list (p).

    hobj->next_ = p;
    hobj->prev_ = prv;

    if (prv)
      prvobj -> next_ = h;
    else // first obj.
      free_ = h;
    if (p) {
      nxtobj = p->obj_Frm_safer();
      nxtobj->prev_ = h;
    }
    h->flags |= head::FREE; // mark that we are in free list now.

  }
  head * nxt = h->next_head();
  if (p == nxt) {
    // we can merge the current block with the next.
    unlink_free(p); // first unlink it from free list.
    merge_next(h); // merge us with the following block.
  }
}

// unlink object from free list.
void alf::gc::Fpool::unlink_free(head * h)
{
  if (h == 0 || (h->flags & head::FREE) == 0) return;

  Fremoved * hobj = h->obj_Frm_safer();
  head * hnxt = hobj->next_;
  head * hprv = hobj->prev_;
  Fremoved * nxt = 0;
  Fremoved * prv = 0;

  if (hnxt) {
    nxt = hnxt->obj_Frm_safer();
    nxt->prev_ = hprv;
  }
  if (hprv) {
    prv = hprv->obj_Frm_safer();
    nxt->next_ = hnxt;
  } else {
    free_ = hnxt;
  }
  h->flags &= ~head::FREE;
}

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

// merge h and the next block - h may or may not be in free list
// but the block following it cannot be in free list.
// if the first block is in free list prior to merge, it will remain
// in free list, but size will change.

void alf::gc::Fpool::merge_next(head * h)
{
  if (h == 0) return;
  
  // to merge this and the next block, both bocks must be an Fremoved
  // object. FREMOVED, UNFROZEN etc all produce Fremoved.

  switch (h->gctype()) {
  case head::FREMOVED:
  case head::UNFROZEN:
    break;
  default:
    return; // do nothing.
  }

  head * hnxt = h -> next_head();

  switch (hnxt->gctype()) {
  case head::FREMOVED:
  case head::UNFROZEN:
    break;
  default:
    return; // do nothing.
  }
  merge__(h, hnxt);
}

// merge prev and h.
void alf::gc::Fpool::merge_prev(head * h)
{
  if (h == 0) return;
  
  // to merge this and the next block, both bocks must be an Fremoved
  // object. FREMOVED, UNFROZEN etc all produce Fremoved.

  switch (h->gctype()) {
  case head::FREMOVED:
  case head::UNFROZEN:
    break;
  default:
    return; // do nothing.
  }

  head * hprv = h -> prev_head();

  switch (hprv->gctype()) {
  case head::FREMOVED:
  case head::UNFROZEN:
    break;
  default:
    return; // do nothing.
  }
  merge__(hprv, h);
}

void alf::gc::Fpool::merge_(head * h, head * nxt)
{
  if (h == 0 || nxt == 0) return;
  
  // to merge this and the next block, both bocks must be an Fremoved
  // object. FREMOVED, UNFROZEN etc all produce Fremoved.

  switch (h->gctype()) {
  case head::FREMOVED:
  case head::UNFROZEN:
    break;
  default:
    return; // do nothing.
  }

  switch (nxt->gctype()) {
  case head::FREMOVED:
  case head::UNFROZEN:
    break;
  default:
    return; // do nothing.
  }

  merge__(h, nxt);
}

void alf::gc::Fpool::merge__(head * h, head * nxt)
{
  // to merge this and the next block, both bocks must be an Fremoved
  // object. FREMOVED, UNFROZEN etc all produce Fremoved.

  // unlink next obj if it is in free list.
  if (nxt->flags & head::FREE) unlink_free(nxt);
  tail * t = nxt->cur_tail();
  h->sz += nxt->sz;
  t->D_.sz = h->sz;
  nxt->flags = head::REMOVED | head::FMERGED;
}

// split h into two blocks, return ptr to new block.

// first block start at h and is size oldsize - sz
// second block start at (char *)h + newsz and is
// size sz. The newsz + sz == oldsize.
alf::gc::head * alf::gc::Fpool::split(head * h, std::size_t sz)
{
  std::size_t sztot = h->sz;
  if (sztot < sz + head::MINBLKSZ) return 0; // don't split.

  // sz1 >= MINBLKSZ and is size of first block.
  std::size_t sz1 = sztot - sz;
  h->sz = sz1;
  head * nxt = h -> next_head(); // start of new block.
  tail * t = reinterpret_cast<tail *>(nxt) - 1; // new tail for h.
  t->init(sz1, sizeof(Fremoved)); // ignore usz, use this size instead.
  nxt->b_init(h->mp, head::FREMOVED, sz, sizeof(Fremoved));
  nxt->Frm_p = new(nxt+1) Fremoved();
  return nxt;
}

// h is assumed to point to a free element - return
// a pointer to the Fremoved object if so.

// static
alf::gc::Fremoved *
alf::gc::Fpool::get_free(head * h)
{
  if (h == 0) return 0;
  return h->obj_Frm_safer();
}

// if pointer is found in a minipool, return that minipool.
// otherwise, return 0.
alf::gc::minipool *
alf::gc::Fpool::block_in_pool(const void * ptr)
{
  // Walk through all objects in Fpool and check if p is in that minipool.
  pool_iterator p = F_.begin();
  while (p != F_.end()) {
    minipool * mp = *p;
    ++p;
    if (mp != 0 && mp->block_in_pool(ptr))
      return mp;
  }
  return 0;
}

// if pointer is found in a minipool, return that minipool.
// otherwise, return 0.
alf::gc::minipool *
alf::gc::Fpool::block_in_pool(const void * ptr, const void * eptr)
{
  // Walk through all objects in Fpool and check if p is in that minipool.
  pool_iterator iter = F_.begin();
  while (iter != F_.end()) {
    minipool * mp = *iter;
    ++iter;
    if (mp != 0) {
      switch (mp->block_in_pool_(ptr, eptr)) {
      case head::PARTIAL:
	return minipool::BAD_MINIPOOL;
      case head::FULL:
	return mp; // found it.
      }
    }
  }
  return 0;
}
