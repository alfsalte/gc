
#include "removed.hxx"

// This represent an object that has been removed.
// It is a dangling pointer but this has already been handled
// so this function should never be called.

// virtual
alf::gc::removed::~removed()
{
}

// virtual
void alf::gc::removed::gc_walker(const std::string &)
{
}



