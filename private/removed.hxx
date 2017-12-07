#ifndef __GC_PRIV_REMOVED_HXX__
#define __GC_PRIV_REMOVED_HXX__

#include <cstdlib>

#include <string>

#include "../gc.hxx"

namespace alf {

namespace gc {

// this class represent an object that has been removed (delete)
class removed : public gcobj {
public:

  removed() { }
  virtual ~removed();

  virtual void gc_walker(const std::string & txt);

  void * operator new(std::size_t, void * p) { return p; }

}; // end of class removed.

}; // end of namespace gc

}; // end of namespace alf

#endif
