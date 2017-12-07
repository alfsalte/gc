
#include <cstring>

#include "../gc.hxx"

#include "head.hxx"
#include "pool.hxx"
#include "lpool.hxx"

// Lpool is a pool used to manage objects that are too large
// to be stored and moved around in GCpool.
// These objects are never moved once allocated.
// Freezing these objects will simply increase a counter and
// unfreezing them will decrease that counter.
// You can regard these objects as permanently frozen except that
// they are not kept in a memory area managed by the pool.
// instead the pool only keep track of where these objects are and identify
// them properly.

alf::gc::Lpool & alf::gc::Lpool::enlarge()
{
  std::size_t newm = m_ == 0 ? 32 : m_ < 1024 ? m_ + m_ : m_ + 1024;
  gcobj ** p = new gcobj *[newm];
  std::memcpy(p, L_, n_*sizeof(gcobj *));
  std::memset(p + n_, 0, (newm - n_)*sizeof(gcobj *));
  delete [] L_;
  L_ = p;
  m_ = newm;
  return *this;
}

alf::gc::head * alf::gc::Lpool::alloc_(size_t usz, void * & ptr)
{
  std::size_t asz =  head::asz(usz);
  std::size_t sz = asz + (sizeof(head) + sizeof(tail));
  char * p = new char[sz];
  head * h = reinterpret_cast<head *>(p);
  char * op = p + sizeof(head);
  char * oe = op + usz;
  char * e = p + sz;
  char * tp = e - sizeof(tail);
  tail * t = reinterpret_cast<tail *>(tp);
  h->b_init(0, head::LOBJ, sz, usz);
  gcobj * obj = h->p;
  // block is created - insert it into Lpool.
  if (n_ == m_)
    enlarge();
  L_[n_++] = obj;
  ptr = reinterpret_cast<void *>(op);
  return h;
}

bool alf::gc::Lpool::dealloc_(head * h, void * p)
{
  if (p == 0) return false;

  // look for p in L_.
  ssize_t x = find(p);

  if (x < 0) return false; // not found in Lpool.

  // let the last elem and x swap places.
  gcobj * obj = L_[x];
  if (x < --n_)
    L_[x] = L_[n_];
  // from here we pretend the removed element was n_.
  L_[n_] = 0;
  // we assume user has already destroyed the object at p
  // so we do not call destructor.
  destroy_(h);
  return true;
}

// walk through Lpool and walk any frozen objs.
void alf::gc::Lpool::gc_walk()
{
  std::size_t k = 0;

  while (k < n_) {

    gcobj * obj = L_[k++];
    head * h = head::get_head(obj);
    std::size_t bsz, usz;

    if (h == 0)
      throw fatal_error("Lpool has corrupt HEAD");

    switch (h->flags & head::POOLMASK) {
    case head::LOBJ:
      
      // have we seen this obj?
      if (h->fcnt == 0) continue; // not frozen, skip it.
      if (h->flags & head::GCBIT) continue; // already seen it, skip it.
      h->flags |= head::GCBIT; // mark it as seen.
      obj->gc_walker("Frozen obj");

      /* FALLTHRU */
    case head::LREMOVED:
      continue; // skip it it is no longer there.

    default:
      throw fatal_error("Lpool corrupt, obj flags is " + h->gcflags_str());
    }
  }
}

// walk through Lpool and walk any frozen objs.
void alf::gc::Lpool::gcbit_off()
{
  std::size_t k = 0;

  while (k < n_) {

    gcobj * obj = L_[k++];
    head * h = head::get_head(obj);

    if (h == 0)
      throw fatal_error("Lpool has corrupt HEAD");

    h->flags &= ~head::GCBIT;
  }
}

// garbage collect Lpool objs.
void alf::gc::Lpool::gc_cleanup()
{
  std::size_t k = n_;

  while (k) {

    gcobj * obj = L_[--k];
    head * h = head::get_head(obj);

    if (h == 0)
      throw fatal_error("Lpool has corrupt HEAD");

    if (h->gctype() != head::LOBJ)
      throw fatal_error("Lpool corrupt, obj flags is " + h->gcflags_str());

    // have we seen this obj?
    if (h->fcnt || (h->flags & head::GCBIT) != 0) {
      // turn off GCBIT.
      h->flags &= ~head::GCBIT;
      continue;
    }

    obj->~gcobj(); // destroy the obj.
    // do not actually delete it yet
    // we just mark it as removed for now so that wptr_pool
    // can access it.
    h->flags = head::REMOVED | head::LREMOVED;
    h->p = 0;
    new(obj) removed;
    S_.dealloc(h->sz, h->usz);
    // if we moved last obj to L_[k] we have a 'new' element here
    // but it is the same element we passed earlier so we skip it.
  }
}

// since gc_cleanup doesn't actually delete the objects
// we do that here.
void alf::gc::Lpool::gc_cleanup2()
{
  std::size_t k = n_;

  while (k) {

    gcobj * obj = L_[--k];
    head * h = head::get_head(obj);
    std::size_t bsz, usz;

    if (h == 0)
      throw fatal_error("Lpool has corrupt HEAD");

    switch (h->gctype()) {
    case head::LREMOVED:
      // delete this object. Destructor and so on is already
      // called, just delete it.
      // let it swap places with last elem.
      if (k < --n_)
	L_[k] = L_[n_];
      L_[n_] = 0;
      destroy_(h);
      /* FALLTHRU */

    case head::LOBJ:
      // object is fine, keep it.
      continue;

    default:
      throw fatal_error("Lpool corrupt, obj flags is " + h->gcflags_str());
    }
    // if we moved last obj to L_[k] we have a 'new' element here
    // but it is the same element we passed earlier so we skip it.
  }
}

// remove all Lpool objs.
void alf::gc::Lpool::cleanup()
{
  while (n_) {

    gcobj * obj = L_[--n_];
    head * h = head::get_head(obj);
    std::size_t bsz, usz;

    L_[n_] = 0;

    if (h == 0)
      throw fatal_error("Lpool has corrupt HEAD");

    switch (h->gctype()) {
    case head::LOBJ:
      bsz = h->sz;
      usz = h->usz;
      obj->~gcobj();
      destroy_(h);
      S_.dealloc(bsz, usz);

    case head::LREMOVED:
      continue;

    default:
      throw fatal_error("Lpool corrupt, obj flags is " + h->gcflags_str());
    }
  }
}

ssize_t alf::gc::Lpool::find(void * p) const
{
  if (p == 0) return -1;
  std::size_t k = 0;

  while (k < n_ && L_[k] != p) ++k;
  if (k < n_) return k;
  return -1;
}

void alf::gc::Lpool::destroy_(head * h)
{
  if (h->gctype() != head::LOBJ)
    throw fatal_error("Expected LOBJ here - not " + h->gcflags_str());

  // destructor for gcobj is assumed to have been called already.
  if (! h->check(0, head::LOBJ))
    throw fatal_error("Lpool corrupted.");
  delete [] reinterpret_cast<char *>(h);
}

alf::gc::head *
alf::gc::Lpool::get_block_head(const void * p)
{
  std::size_t k = n_;

  while (k) {

    gcobj * obj = L_[--k];
    head * h = head::get_head(obj);

    if (h == 0)
      throw fatal_error("Lpool has corrupt HEAD");

    switch (h->gctype()) {
    case head::LOBJ:
    case head::LREMOVED:
      if (h->in_block(p))
	return h;
      continue;

    default:
      throw fatal_error("Lpool corrupt, obj flags is " + h->gcflags_str());
    }
    // if we moved last obj to L_[k] we have a 'new' element here
    // but it is the same element we passed earlier so we skip it.
  }
  return 0;
}

alf::gc::head *
alf::gc::Lpool::get_block_head(const void * p, const void * q)
{
  std::size_t k = n_;

  while (k) {

    gcobj * obj = L_[--k];
    head * h = head::get_head(obj);

    if (h == 0)
      throw fatal_error("Lpool has corrupt HEAD");

    switch (h->gctype()) {
    case head::LOBJ:
    case head::LREMOVED:
      if (h->in_block(p,q))
	return h;
      continue;

    default:
      throw fatal_error("Lpool corrupt, obj flags is " + h->gcflags_str());
    }
    // if we moved last obj to L_[k] we have a 'new' element here
    // but it is the same element we passed earlier so we skip it.
  }
  return 0;
}
