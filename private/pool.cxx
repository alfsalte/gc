
#include "pool.hxx"

alf::gc::pool::pool(std::size_t sz)
  : sz_(sz), usz_(0),
    usz_alloc_(0), usz_dealloc_(0),
    sz_alloc_(0), sz_dealloc_(0),
    n_alloc_(0), n_dealloc_(0)
{ }

// no alloc, just update variables.
void alf::gc::pool::alloc_(std::size_t usz, std::size_t bsz)
{
  ++n_alloc_;
  usz_alloc_ += usz;
  sz_alloc_ += bsz;
}

// no dealloc, just update variables.
void alf::gc::pool::dealloc_(std::size_t usz, std::size_t bsz)
{
  ++n_dealloc_;
  usz_dealloc_ += usz;
  sz_dealloc_ += bsz;
}
