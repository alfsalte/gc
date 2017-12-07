
#include "../gc.hxx"

// virtual
alf::gc::gc_allocation_error::~gc_allocation_error()
{ }

// virtual
const char * alf::gc::gc_allocation_error::what() const throw()
{
  return M_.c_str();
}
