
#include "../gc.hxx"

#include "head.hxx"

alf::gc::tail * alf::gc::tail::init(std::size_t sz, std::size_t usz)
{
  // This fills the whole user area except an area of size
  // sizeof(Fremoved).
  // To be exact we could have gotten the size of user area
  // but as it is often used for removed objects, the usz field
  // of head may not be accurate.
  D_.magic = MAGIC;
  D_.sz = sz;
  // start addr for filler area is deadbeef - the additional space.
  std::size_t spc = sz - (sizeof(head) + sizeof(tail)) - usz;
  //head::fill(deadbeef - spc, 0xdeadbeef, sizeof(deadbeef) + spc);
  head::fill(deadbeef - spc, 0x0b0b0b0b, sizeof(deadbeef) + spc);
  return this;
}

alf::gc::tail * alf::gc::tail::init(std::size_t sz)
{
  // get location of head and get usz from it.
  char * end = reinterpret_cast<char *>(this + 1);
  head * h = reinterpret_cast<head *>(end - sz);
  std::size_t usz = h->usz;

  init(sz, usz);
  return this;
}
