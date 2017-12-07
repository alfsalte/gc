
#include "moved.hxx"

// this class represent an object that has moved (to another minipool or pool).

// virtual
alf::gc::moved::~moved()
{ }

// This object has already moved and should not be walked.
// We have already walked it when we moved it.

// virtual
void alf::gc::moved::gc_walker(const std::string &)
{ }

