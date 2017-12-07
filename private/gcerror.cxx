
#include "../gc.hxx"

// virtual
alf::gc::gc_error::~gc_error()
{ }

// virtual
const char * alf::gc::gc_error::what() const throw()
{
  return M_.c_str();
}
