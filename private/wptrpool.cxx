
#include "../gc.hxx"

#include "wptrpool.hxx"

// PtrPool is a special pool to keep track of user's weak pointers.
// User can register pointers as weak pointers and gc will walk through
// each of them after gc to update them incase the object pointed to was
// removed by gc.

alf::gc::WPtrPool::~WPtrPool()
{
  wptr_unregister_all();
  delete [] T_;
}

// increase the pointer pool
alf::gc::WPtrPool & alf::gc::WPtrPool::enlarge()
{
  std::size_t newm = m_ == 0 ? 32 : m_ < 1024 ? m_ + m_ : m_ + 1024;
  entry * p = new entry[newm];
  std::memset(p + n_, 0, (newm - n_)*sizeof(entry));
  std::memcpy(p, T_, n_*sizeof(entry));
  delete [] T_;
  T_ = p;
  m_ = newm;
  return *this;
}

// register a pointer.
void
alf::gc::WPtrPool::wptr_register(GCpool & gcp, Fpool & fp, Lpool & lp,
				 gcobj * & p)
{
  gcobj ** pp = & p;

  if (pp) {
    minipool * mp = gcp.block_in_pool(pp);
    head * h = 0;

    if (mp == minipool::BAD_MINIPOOL)
      // do not register this pointer.
      return;

    if (mp != 0 || (mp = fp.block_in_pool(pp)) != 0)
      h = mp->get_block_head(pp);
    else
      h = lp.get_block_head(pp);

    // if there is a block but it is not allocated to an object
    // we get a special head value return here - check for that:
    if (h == head::BAD_BLOCK)
      // do not register this pointer.
      return;

    // if h == 0 it is not in any block
    // verify the pointer is in user area.
    if (h != 0 && ! h->in_obj(pp))
      // something is very wrong.
      // don't register this pointer.
      return;

    // let us insert it in the proper position
    // we sort on address of the pointer.
    int k = n_;
    while (--k >= 0 && pp < T_[k].pp)
      T_[k+1] = T_[k];
    // k < 0 || pp >= T_[k]
    T_[++k].pp = pp;
    T_[k].h = h;
    ++n_;
  }
}

// remove a registration of this pointer.
void alf::gc::WPtrPool::wptr_unregister(gcobj * & p)
{
  std::size_t k = n_;

  while (k > 0) {
    if (T_[--k].pp == & p) {
      if (k < --n_) // k is not last, swap places with last.
	T_[k] = T_[n_];
      // From here it is n_ that is removed.
      T_[n_] = entry();
      return;
    }
  }
}

// remove all registrations of this pointer.
void alf::gc::WPtrPool::wptr_unregister_all(gcobj * & p)
{
  std::size_t k = n_;

  while (k > 0) {
    if (T_[--k].pp == & p) {
      if (k < --n_) // k is not last, swap places with last.
	T_[k] = T_[n_];
      // From here it is n_ that is removed.
      T_[n_] = entry();
      // T_[k] is formerly T_[n_] which we have processed previously
    }
  }
}

// remove all registrations of all pointers.
void alf::gc::WPtrPool::wptr_unregister_all()
{
  std::memset(T_, 0, n_*sizeof(*T_));
}

alf::gc::gcobj *
alf::gc::WPtrPool::gc_update_wptr(gcobj * p)
{
  while (true) {

    head * h = head::get_head_safe(p);

    switch (h ? h->gctype() : -1) {

    case head::GCOBJ:
    case head::FROZEN:
    case head::LOBJ:
      // still an object at same location, just continue.
      return p;

    case head::GCMOVED:
    case head::GCFROZEN:
    case head::UNFROZEN:
      if ((p = h->p) == 0)
	// object is removed.
	return 0;
      // object has moved, do again with this pointer
      continue;

    case head::GCRM:
    case head::FREMOVED:
    case head::FMERGED:
    case head::LREMOVED:
      // object is gone.
      return 0;

    default:
      if (h)
	throw fatal_error("Unknown gctype " + h->gcflags_str());
      throw fatal_error("Corrupt allocation block");
    }
  }
}

void alf::gc::WPtrPool::gc_update_wptrs()
{
  std::size_t k = n_;
  
  while (k) {
    gcobj ** pp = T_[--k].pp;
    if (pp && *pp)
      *pp = gc_update_wptr(*pp);
  }
}

int alf::gc::WPtrPool::find(gcobj * & p) const
{
  gcobj ** pp = & p;
  int k = n_;
  while (k > 0)
    if (T_[--k].pp == pp) return k;
    else if (T_[k].pp < pp) return -1;
  return -1;
}

void alf::gc::WPtrPool::update_pp(head * h1, head * h2, ssize_t delta)
{
  ssize_t k = n_;
  std::size_t nb = 0; // number of entries with .h == h1.
  ssize_t nk = -1; // index of lowest entry with .h == h1.
  entry * e = 0;

  // first scan past those that are on addresses higher than h1 block.
  while (--k >= 0) {
    e = T_ + k;
    if (e->h <= h1 && e->h != 0)
      break;
  }
  // k < 0 || (T_[k].h != 0 && T_[k].h <= h1)
  
  while (k >= 0 && (e = T_ + k)->h == h1) {
    e->h = h2;
    // update the pointer value
    e->pp = reinterpret_cast<gcobj **>
      (reinterpret_cast<char *>(e->pp) + delta);
    ++nb; // number of entries that matches block.
    nk = k;
    --k;
  }
  // Now we need to find the new insertion point for this block
  // of nb entries starting at nk..nk+nb-1
  if (nb > 0 && delta != 0) {
    std::size_t be = nk + nb; // first index not in area.
    std::size_t j;

    // changed nb entries at nk..be-1

    // find new insertion point.
    // Since all entries are in the same block it doesn't matter which
    // pointer I use - so I pick the one at nk.
    gcobj ** pp = T_[nk].pp; // The value
    // also need a save area for the modified entries.
    entry * b = new entry[nb];
    // save the modified entries in a temporary storage.
    std::memcpy(b, T_ + nk, nb*sizeof(entry));

    if (delta > 0) {
      // we need to move them higher up.
      j = n_;
      while (--j >= be && pp < T_[j].pp);
      // if j < be it means that all of them are higher than
      // our block, so it stays in the same place.
      // if j >= be it means that j+1..end are higher and should
      // stay where they are and the elements low..j should move
      // down to nk..nk+j-be and the elements from nk..be-1
      // should move up to j+1-nb..j
      if (j >= be) {
	// we have already saved the elements from nk..be - 1
	std::memcpy(T_ + nk, T_ + be, (j + 1 - be)*sizeof(entry));
	// T_ + j + 1 - nb..T_ + j
	std::memcpy(T_ + j + 1 - nb, b, nb*sizeof(entry));
      }
    } else if (delta < 0) {
      // will move to lower addresses, start searching at nk.
      j = nk;
      while (--j >= 0 && pp < T_[j].pp);
      // j < 0 means we should move the changed entries to beginning
      // of array.
      // j >= 0 means 0..j are before these entries and should
      // stay where they are, entries j+1..nk-1
      // should move up to nb+j+1..be-1 and the changed entries
      // should move down to j+1..nb+j
      std::memmove(T_ + nb + j + 1, T_ + j + 1, (nk - j - 1)*sizeof(entry));
      std::memcpy(T_ + j + 1, b, nb*sizeof(entry));
    }
    // don't need b any more.
    delete [] b;
  }
  // The array should be sorted once again.
}

void alf::gc::WPtrPool::init()
{
  T_ = new entry[m_ = 32];
  n_ = 0;
}      
