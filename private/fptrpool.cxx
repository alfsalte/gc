
#include "../gc.hxx"
#include "fptrpool.hxx"

alf::gc::FPtrPool::~FPtrPool()
{
  fun_unregister_all();
  char * tbl = reinterpret_cast<char *>(T_);
  T_ = 0;
  delete [] tbl;
}

alf::gc::FPtrPool & alf::gc::FPtrPool::enlarge()
{
  std::size_t newm = m_ == 0 ? 32 : m_ < 1024 ? m_ + m_ : m_ + 1024;
  // so that we do not call constructors for entries we haven't made.
  entry * p = reinterpret_cast<entry *>(new char[newm*sizeof(entry)]);
  std::memset(p + n_, 0, (newm - n_)*sizeof(entry));
  std::memcpy(p, T_, n_*sizeof(entry));
  delete [] reinterpret_cast<char *>(T_);
  T_ = p;
  m_ = newm;
  return *this;
}

// register a pointer.
void alf::gc::FPtrPool::fun_register(GCpool & gcp, Fpool & fp, Lpool & lp,
				     const std::string & txt,
				     void * obj,
				     void (* f)(const std::string &, void *))
{
  if (obj != 0 && f != 0) {
    minipool * mp = gcp.block_in_pool(obj);
    head * h = 0;

    if (mp == minipool::BAD_MINIPOOL)
      // do not register this pointer.
      return;

    if (mp != 0 || (mp = fp.block_in_pool(obj)) != 0)
      h = mp->get_block_head(obj);
    else
      h = lp.get_block_head(obj);
    // if there is a block but it is not allocated to an object
    // we get a special head value return here - check for that:
    if (h == head::BAD_BLOCK)
      // do not register this pointer.
      return;

    // if h == 0 it is not in any block
    // verify the pointer is in user area.
    if (h != 0 && ! h->in_obj(obj))
      // something is very wrong.
      // don't register this pointer.
      return;

    if (n_ == m_) enlarge();
    new(T_ + n_++) entry(txt, h, obj, f);
  }
}


// remove a registration of this pointer. If you have registered
// the same pointer multiple times you should call unregister for each
// register.
void alf::gc::FPtrPool::fun_unregister(void * obj)
{
  std::size_t k = n_;

  while (k > 0) {
    if (T_[--k].obj == obj) {
      if (k < --n_) // k is not last, swap places with last.
	swap(T_[k], T_[n_]);
      // From here we pretend it is n_ that is removed.
      T_[n_].~entry();
      return;
    }
  }
}

// remove all registrations of this pointer.
void alf::gc::FPtrPool::fun_unregister_all(void * obj)
{
  std::size_t k = n_;

  while (k > 0) {
    if (T_[--k].obj == obj) {
      if (k < --n_) // k is not last, swap places with last.
	swap(T_[k], T_[n_]);
      // From here we pretend it is n_ that is removed.
      T_[n_].~entry();
      // We have a new element in T_[k] and so ought to continue from there
      // and do ++k, but we already know that T_[k] is not pp since
      // we already seen it earlier, so we do not, continue from
      // k-1 instead.
    }
  }
}

// remove all registrations of all pointers.
// called by destructor
void alf::gc::FPtrPool::fun_unregister_all()
{
  while (n_)
    T_[--n_].~entry();
}

void alf::gc::FPtrPool::gc_walk()
{
  std::size_t k = 0;
  while (k < n_) {
    entry & e = T_[k++];
    e.f(e.txt, e.obj);
  }
}

void alf::gc::FPtrPool::update_pp(head * h1, head * h2, ssize_t delta)
{
  ssize_t k = n_;
  while (k) {
    entry & e = T_[--k];
    if (e.h == h1) {
      e.h = h2;
      // update the pointer value
      e.obj = reinterpret_cast<void *>
	(reinterpret_cast<char *>(e.obj) + delta);
    }
  }
}

void alf::gc::FPtrPool::init()
{
  enum {ISIZE = 32};
  
  m_ = ISIZE;
  // so that we do not call constructors for entries we haven't made.
  // This is T_ = new entry[ISIZE] without calling constructors for
  // entry elements.
  T_ = reinterpret_cast<entry *>(new char[ISIZE*sizeof(entry)]);
  n_ = 0;
}
