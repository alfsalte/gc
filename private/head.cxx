
#include <cstdlib>
#include <cstring>

#include <string>

#include "head.hxx"

// static
const char * alf::gc::head::S_gctypes[LREMOVED + 2] = {
  "none",
  "GCOBJ", "GCMOVED", "GCRM", "GCFROZEN",
  "FROZEN", "UNFROZEN", "FREMOVED", "FMERGED",
  "LOBJ", "LREMOVED",
  0 };

// static
const char * alf::gc::head::gctype_str(int t)
{
  static char b[30];
  t &= POOLMASK;
  if (t < GCOBJ || t > LREMOVED) {
    sprintf(b, "%d", t);
    return b;
  }
  return S_gctypes[t];
}

#if 0
static
char * stpcpy(char * d, const char * s)
{
  while ((*d = *s++) != 0) ++d;
  return d;
}
#endif

// static
std::string alf::gc::head::gcflags_str(int f)
{
  char buf[200];
  char * p = buf;

  if (f & GCBIT)
    p = stpcpy(p, "GCBIT");
  if (f & MOVED) {
    if (p != buf)
      *p++ = '|';
    p = stpcpy(p, "MOVED");
  }
  if (f & REMOVED) {
    if (p != buf)
      *p++ = '|';
    p = stpcpy(p, "REMOVED");
  }
  if (f & FREE) {
    if (p != buf)
      *p++ = '|';
    p = stpcpy(p, "FREE");
  }
  f &= POOLMASK;
  if (p != buf)
    *p++ = '|';
  p = stpcpy(p, gctype_str(f & POOLMASK));
  return std::string(buf, p - buf);
}

alf::gc::Fremoved * alf::gc::head::obj_Frm_safer() const
{
  Fremoved * p = obj_Frm_safe();
  if (p == 0)
    throw fatal_error("obj is not Fremoved");
  return p;
}

alf::gc::Fremoved * alf::gc::head::obj_Frm_safest() const
{
  Fremoved * p = obj_Frm_safer();

  if (! check(mp))
    throw fatal_error("obj block is corrupt");

  switch (gctype()) {
  case FREMOVED:
  case UNFROZEN:
  case FMERGED:
    return p;
  }

  throw fatal_error("free object wrong gctype");
}

bool alf::gc::head::check(minipool * real_mp) const
{
  // the sanity of head and tail.
  if (! magic_ok()) return false;
  int f = flags;
  int m = f & POOLMASK;
  head * h;

  if (m > LREMOVED) return false;
  if (usz > sz) return false;
  if (sz & (sizeof(std::size_t) - 1)) return false;
  if (mp != real_mp) return false;

  switch (m) {
  case GCOBJ:
    if (fcnt) return false; // fcnt should be 0 for GC pool objs.
    if (p != obj()) return false;
    break;

  case GCMOVED:
    if (fcnt) return false;
    if (p == 0 || p == obj()) return false;
    if ((h = get_head_safe(p)) == 0) return false;

    // since we pass h->mp here that check will always be true
    // so we need to check that value again in caller.
    if (! h->check(h->mp, GCOBJ)) return false;

    break;

  case GCRM:
    if (fcnt) return false;
    if (p) return false;

  case GCFROZEN:
    if (fcnt) return false; // this object is moved.
    if (p == 0 || p == obj()) return false;
    if ((h = get_head_safe(p)) == 0) return false;
    // same comment as for GCMOVED regarding h->mp.
    if (! h->check(h->mp, FROZEN)) return false;
    break;

  case FROZEN:
    if (fcnt == 0) return false; // this object is frozen.
    if (p != obj()) return false;
    break;

  case UNFROZEN:
    // this object is moved back to GC pool.
    if (p == 0 || p == obj()) return false;
    h = get_head_safe(p);
    if (! h->check(h->mp, GCOBJ)) return false;
    break;

  case FREMOVED:
  case FMERGED:
    // this object is deleted.
    if (p) return false;
    break;

  case LOBJ:
    if (mp) return false;
    if (p != obj()) return false;
    break;

  case LREMOVED:
    if (mp) return false;
    if (p) return false;
    break;
  }

  tail * t = cur_tail();
  if (! t->magic_ok()) return false;
  return t->size() == sz;
}

bool alf::gc::head::check(minipool * real_mp, int state) const
{
  // the sanity of head and tail.
  if (! check(real_mp)) return false;
  return (flags & POOLMASK) ==  state;
}

// static
alf::gc::head * alf::gc::head::get_head_safe(void * obj)
{
  if (obj == 0) return 0;
  head * h = get_head(obj);
  if (h == 0 || !h->check(h->mp)) return 0;
  return h;
}

// static
void alf::gc::head::fill(char * dest, unsigned int data, size_t n)
{
  union {
    char bb[sizeof(int)];
    unsigned int dd;
  };

  char * p = dest;
  void * e = dest + n;
  void * ee = e;
  int k;

  if (p < e) {
    dd = data;
    switch (k = reinterpret_cast<std::size_t>(p) & (sizeof(int) - 1)) {
    case 1:
      *p = bb[k++];
      if (++p == e)
	break;
      /* FALLTHRU */
    case 2:
      *p = bb[k++];
      if (++p == e)
	break;
      /* FALLTHRU */
    case 3:
      *p++ = bb[k++];
      /* FALLTHRU */
    case 0:
      break;
    }
    unsigned int * q = reinterpret_cast<unsigned int *>(p);
    ee = reinterpret_cast<void *>
      (reinterpret_cast<std::size_t>(e) & -sizeof(int));
    while (q < ee)
      *q++ = data;

    if (ee < e) {
      p = reinterpret_cast<char *>(q);
      k = 0;
      while (p < e)
	*p++ = bb[k++];
    }
  }
}

int alf::gc::head::in_block_(const void * p, const void * q) const
{
  if (q <= this)
    return NOT_HERE;
  head * next = next_head();
  if (next <= p)
    return NOT_HERE;
  if (q <= next && this <= p)
    return FULL;
  return PARTIAL;
}

int alf::gc::head::in_obj_(const void * p, const void * q) const
{
  const void * low = this + 1;
  const void * high = reinterpret_cast<const char *>(low) + usz;

  if (q <= low)
    return NOT_HERE;
  if (high <= p)
    return NOT_HERE;
  if (q <= high && low <= p)
    return FULL;
  return PARTIAL;
}

// init header.
void alf::gc::head::h_init(minipool * mpool,
			   int fl,
			   std::size_t bsz,
			   std::size_t u_sz)
{
  magic = MAGIC;
  flags = fl;
  fcnt = 0;
  sz = bsz;
  usz = u_sz;
  vp = reinterpret_cast<void *>(this + 1);
  mp = mpool;
  //fill(deadbeef, 0xdeadbeef, sizeof(deadbeef));
  fill(deadbeef, 0x0a0a0a0a, sizeof(deadbeef));
}

// init head and tail.
void alf::gc::head::b_init(minipool * mpool,
			   int fl,
			   std::size_t bsz,
			   std::size_t u_sz)
{
  std::memset(this, 0, bsz);
  h_init(mpool, fl, bsz, u_sz);
  char * end = reinterpret_cast<char *>(this) + bsz;
  tail * t = reinterpret_cast<tail *>(end - sizeof(tail));
  t->init(bsz, u_sz);
}

