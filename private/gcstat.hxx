#ifndef __GC_GCSTAT_HXX__
#define __GC_GCSTAT_HXX__

#include <sys/time.h>

#include <cstdlib>
#include <cstring>

#include <iostream>

namespace alf {

namespace gc {

struct statistics {

  struct timeval timing;
  std::size_t usz_a;
  std::size_t usz_d;
  std::size_t usz_f;
  std::size_t usz_u;
  std::size_t sz_a;
  std::size_t sz_d;
  std::size_t sz_f;
  std::size_t sz_u;
  int n_freeze;
  int n_unfreeze;
  int n_a;
  int n_d;
  int n_gc;
  bool in_gc;

  statistics()
  { std::memset(this, 0, sizeof(*this)); }

  std::size_t usz_cur_a() const { return usz_a - usz_d; }

  std::size_t sz_cur_a() const { return sz_a - sz_d; }

  int n_cur_a() const { return n_a - n_d; }

  std::size_t usz_cur_f() const { return usz_f - usz_u; }
  std::size_t sz_cur_f() const { return sz_f - sz_u; }
  int n_cur_f() const { return n_freeze - n_unfreeze; }

  void alloc(std::size_t sz, std::size_t usz)
  {
    ++n_a;
    usz_a += usz;
    sz_a += sz;
  }

  void dealloc(std::size_t sz, std::size_t usz)
  {
    ++n_d;
    usz_d += usz;
    sz_d += sz;
  }

  void freeze(std::size_t sz, std::size_t usz)
  {
    ++n_freeze;
    usz_f += usz;
    sz_f += sz;
  }

  void unfreeze(std::size_t sz, std::size_t usz)
  {
    ++n_unfreeze;
    usz_u += usz;
    sz_u += sz;
  }

  void gc_add_timing(const struct timeval & t);

  std::ostream & report(std::ostream & os) const;

  // reset timing and num_gc data.
  void reset_num_gc();

  time_t time_gc(struct timeval * ptv = 0) const;

}; // end of struct statistics

}; // end of namespace gc

}; // end of namespace alf

#endif
