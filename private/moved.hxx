#ifndef __GC_PRIV_MOVED_HXX__
#define __GC_PRIV_MOVED_HXX__

#include <cstdlib>

#include <string>

#include "../gc.hxx"

namespace alf {

namespace gc {

struct head;

// this class represent an object that has moved (to another pool).
class moved : public gcobj {
public:
  head * desth_;
  gcobj * dest_;

  moved(head * h, gcobj * dest) : desth_(h), dest_(dest) { }
  virtual ~moved();

  virtual void gc_walker(const std::string & txt);

  void * operator new(std::size_t, void * p) { return p; }

}; // end of class moved

}; // end of namespace gc

}; // end of namespace alf


#endif
