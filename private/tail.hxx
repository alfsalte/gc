#ifndef __GC_PRIV_TAIL_HXX__
#define __GC_PRIV_TAIL_HXX__

#include <cstdlib>

#include "../gc.hxx"

namespace alf {

namespace gc {

struct head;

// tail, following every gcobj.
struct tail {

  enum { TAILSIZE = 48 };
  enum { MAGIC = 0x973fdb825d6d3ce9 };

  struct data {
    size_t magic;
    size_t sz; // size of this allocation, gives us pointer back to head.
  }; // end of struct data

  char deadbeef[TAILSIZE - sizeof(data)]; // filler after user data.
  data D_;

  char * next_head_charp() const
  { return reinterpret_cast<char *>(const_cast<tail *>(this + 1)); }

  head * next_head() const
  { return reinterpret_cast<head *>(next_head_charp()); }

  head * cur_head() const
  { return reinterpret_cast<head *>(next_head_charp() - D_.sz); }

  bool magic_ok() const { return D_.magic == MAGIC; }

  std::size_t size() const { return D_.sz; }

  tail * init(std::size_t sz, std::size_t usz);
  // fetch usz from head and call init(sz, usz).
  tail * init(std::size_t sz);

}; // end of struct tail

}; // end of namespace gc

}; // end of namespace alf


#endif
