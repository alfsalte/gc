
#include <sys/time.h>

#include <ctime>

#include <string>
#include <ostream>

#include "../gc.hxx"

#include "moved.hxx"
#include "removed.hxx"
#include "fremoved.hxx"
#include "minipool.hxx"
#include "tail.hxx"
#include "head.hxx"

#include "gcpool.hxx"
#include "fpool.hxx"
#include "lpool.hxx"
#include "ptrpool.hxx"
#include "fptrpool.hxx"
#include "wptrpool.hxx"

#include "../../format/format.hxx"

alf::gc::statistics S;
alf::gc::GCpool gc_pool(S, 128*1024*1024);
alf::gc::Fpool f_pool(S, 32*1024*1024);
alf::gc::Lpool large_pool(S);
alf::gc::PtrPool ptr_pool;
alf::gc::FPtrPool fptr_pool;
alf::gc::WPtrPool wptr_pool;

std::size_t large_sz = 128*1024; // 128K is large by default.

///////////////////////////////////////////
//
// These logically belongs in head.cxx but they reference the
// variables gc_pool, f_pool and large_pool so is defined here.
// check if pointer is ok and return head to it if so.

// check that p is a data that is located somewhere inside a gcobj
// and return the head of that block if ok.

//static
alf::gc::head * alf::gc::head::data_ok(const void * p)
{
  if (p == 0) // 0 pointers are always ok and result in a 0 ptr return.
    return 0;

  minipool * mp = gc_pool.block_in_pool(p);
  head * h = 0;

  if (mp == minipool::BAD_MINIPOOL)
    return head::BAD_BLOCK;

  if (mp != 0 || (mp = f_pool.block_in_pool(p)) != 0)
    h = mp->get_block_head(p);
  else
    h = large_pool.get_block_head(p);

  // if there is a block but it is not allocated to an object
  // we get a special head value return here - check for that:
  if (h == head::BAD_BLOCK)
    return h;

  // if h == 0 it is not in any block - error.
  if (h == 0 || ! h->in_obj(p))
    return head::BAD_BLOCK;

  // pointer is ok, return head.
  return h;
}

//static
alf::gc::head *
alf::gc::head::data_ok(const void * p, const void * q)
{
  if (p == 0)
    return 0;
  head * h = data_ok(p);
  if (h == 0 || h == BAD_BLOCK)
    return BAD_BLOCK;
  if (! h->in_obj(p, q))
    return BAD_BLOCK;
  return h;
}

//static
bool
alf::gc::head::nogc_data_ok(const void * p)
{
  if (p == 0) // 0 pointers are always ok and result in a 0 ptr return.
    return true;

  if (gc_pool.block_in_pool(p) != 0)
    return false;

  if (f_pool.block_in_pool(p) != 0)
    return false;

  return large_pool.get_block_head(p) == 0;
}

//static
bool
alf::gc::head::nogc_data_ok(const void * p, const void * q)
{
  if (p == 0) // 0 pointers are always ok and result in a 0 ptr return.
    return true;

  if (gc_pool.block_in_pool(p,q) != 0)
    return false;

  if (f_pool.block_in_pool(p,q) != 0)
    return false;

  return large_pool.get_block_head(p,q) == 0;

}

// static
alf::gc::head *
alf::gc::head::pointer_ok(gcobj * p)
{
  if (p == 0)
    return 0;

  head * h = data_ok(p);

  if (h == head::BAD_BLOCK)
    return h;

  // if h == 0 it is not in any block - error.
  if (h->obj() != p)
    return head::BAD_BLOCK;

  // pointer is ok, return head.
  return h;
}

bool
alf::gc::gc_pointer_ok_(gcobj * p)
{
  return head::pointer_ok(p) != head::BAD_BLOCK;
}

bool
alf::gc::gc_data_ok_(const void * p)
{
  return head::data_ok(p) != head::BAD_BLOCK;
}

// p == 0 or data is ok.
bool
alf::gc::gc_data_ok_(const void * p, const void * q)
{ return head::data_ok(p, q) != head::BAD_BLOCK; }

// p == 0 or data is ok.
bool
alf::gc::gc_data_ok_(const void * p, std::size_t sz)
{ return head::data_ok(p, sz) != head::BAD_BLOCK; }

bool
alf::gc::gc_data_ok_nz_(const void * p)
{
  return p != 0 && head::data_ok(p) != head::BAD_BLOCK;
}

// data is ok and p != 0.
bool
alf::gc::gc_data_ok_nz_(const void * p, const void * q)
{ return p != 0 && head::data_ok(p, q) != head::BAD_BLOCK; }

// data is ok and p != 0.
bool
alf::gc::gc_data_ok_nz_(const void * p, std::size_t sz)
{ return p != 0 && head::data_ok(p, sz) != head::BAD_BLOCK; }

// data is not in gc area.
bool
alf::gc::gc_nogc_data_ok_(const void * p)
{ return head::nogc_data_ok(p); }

bool
alf::gc::gc_nogc_data_ok_(const void * p, const void * q)
{ return head::nogc_data_ok(p, q); }

bool
alf::gc::gc_nogc_data_ok_(const void * p, std::size_t sz)
{ return head::nogc_data_ok(p, sz); }

// data is not in gc area.
bool
alf::gc::gc_nogc_data_ok_nz_(const void * p)
{ return p != 0 && head::nogc_data_ok(p); }

bool
alf::gc::gc_nogc_data_ok_nz_(const void * p, const void * q)
{ return p != 0 && head::nogc_data_ok(p, q); }

bool
alf::gc::gc_nogc_data_ok_nz_(const void * p, std::size_t sz)
{ return p != 0 && head::nogc_data_ok(p, sz); }

/////////////////////////////////////
//
// These logically belongs in gcobj.cxx but they reference
// the variables S, gc_pool, f_pool and large_pool so they
// are defined here.

// freeze an object. Moves it to frozen pool.
// static
alf::gc::gcobj *
alf::gc::gcobj::S_freeze_(gcobj * ptr, bool do_ptrs /* = true */)
{
  gcobj * ret = ptr;

  if (ptr) {

    head * h = head::get_head_safe(ptr);
    head * h2 = h;


    switch (h ? h->gctype() : -1) {
    case head::GCOBJ:
      // first freeze - move to Fpool.
      h2 = f_pool.freeze_(ptr_pool, wptr_pool, fptr_pool, h, ptr, ret);
      break;

    case head::FROZEN:
      // already frozen, just inc the counter.
    case head::LOBJ:
      // large object, just inc the counter.

      ++h->fcnt;
      break;

    default:

      throw fatal_error("Cannot freeze obj");
    }
    S.freeze(h2->sz, h2->usz);
  }
  if (do_ptrs)
    gc::gc_update_pointers();

  // since obj is frozen it hasn't moved, just return ret.
  return ret;
}

alf::gc::gcobj *
alf::gc::gcobj::S_unfreeze_(gcobj * ptr, bool do_ptrs /* = true */ )
{
  gcobj * ret = ptr;
  head * h2 = 0;

  if (ptr) {

    head * h = h2 = head::get_head_safe(ptr);
    bool didit = true;

    switch (h ? h->gctype() : -1) {
    case head::FROZEN:
      if (--h->fcnt == 0)
	// counter == 0, unfreeze it.
	h2 = gc_pool.unfreeze_(f_pool, ptr_pool, wptr_pool, fptr_pool,
			       h, ptr, ret);
      break;

    case head::LOBJ:
      if (h->fcnt > 0)
	--h->fcnt;
      break;

    case head::GCOBJ:
      // trying to unfreeze an object that's not frozen.
      didit = false;
      break;

    case head::GCMOVED:
    case head::GCFROZEN:
    case head::UNFROZEN:
      // object has moved, delegate to new place.
      return S_unfreeze_(h->p);

    case head::GCRM:
    case head::FREMOVED:
      // object has been removed - return 0. Do not update statistics.
      return 0;

    default:

      throw fatal_error("Cannot unfreeze obj");
    }

    if (didit)
      S.unfreeze(h2->sz, h2->usz);
  }
  if (do_ptrs) {
    gc::gc_update_pointers();
    // if object just melted to gc_pool it has now moved, return
    // ptr to live obj.
    ret = h2 ? h2->p : 0;
  }

  return ret;
}

////////////////////////////////////////////////
// gc_walk_
//

alf::gc::gcobj *
alf::gc::gc_walk_(const std::string & txt, gcobj * ptr)
{
  if (ptr == 0) return 0;

  head * h = head::get_head_safe(ptr);
  head * h2;
  gcobj * ret = h->p;

  if (h) {

    if (h->set_visited())
      // already visited this obj, just return possible new ptr.
      return ret;
      
    // we are visiting now.
    switch (h->gctype()) {

    case head::GCOBJ:
      // regular object - move it.
      ret = gc_pool.move(ptr_pool, wptr_pool, fptr_pool, h, ptr);
      break;

    case head::GCMOVED:
      // object has already moved - this should never happen.
      throw fatal_error("GCMOVED on first visit");

    case head::GCRM:
    case head::FREMOVED:
    case head::FMERGED:
    case head::LREMOVED:
      // object has been removed by user - dangling pointer.
      // object is removed, throw dangling_pointer error.
      throw dangling_pointer(std::string("object at ") +
			     txt + " no longer exist");
    case head::GCFROZEN:
      // object has been frozen, it is now in f_pool.
      // and should stay there. We will walk it and report new addr.
    case head::FROZEN:
      // object is in f_pool, we will walk it and keep it where it is.
    case head::UNFROZEN:
      // object has moved back to GC pool, report new addr.
    case head::LOBJ:
      // object is in large_pool. walk it and keep it where it is.

      break;

    default:

      throw fatal_error("gc corrupted");
    }
  }
  ret->gc_walker(txt);
  return ret;
}

//////////////////////////////////
// allocate

// main function to allocate objects in GC.
// called by user to allcoate area for managed objects.
// Typically called by:
// new T...; where T is a managed class (has gcobj as superclass somewhere).
void * alf::gc::allocate(size_t sz)
{
  head * h;
  void * p;
  bool did_gc = false;

  if (sz >= large_sz)
    h = large_pool.alloc_(sz, p);
  else
    h = gc_pool.alloc_(sz, p, did_gc);
  S.alloc(h->sz, sz);
  return p;
}

//////////////////////////////////
// deallocate_

bool alf::gc::deallocate_(void * ptr)
{
  bool rm = false;
  if (ptr) {

    head * h = head::get_head_safe(ptr);
    std::size_t usz = h->usz;
    std::size_t sz = h->sz;

    switch (h->gctype()) {

    case head::GCOBJ:
      rm = gc_pool.dealloc_(h, ptr);
      break;

    case head::GCRM:
    case head::FREMOVED:
    case head::FMERGED:
      // object is already removed - do nothing.
      return false;

    case head::GCMOVED:
    case head::GCFROZEN:
    case head::UNFROZEN:
      // object has moved, delegate to new location.
      return gc::deallocate_(h->p);

    case head::FROZEN:
      rm = f_pool.dealloc_(h, ptr);
      break;

    case head::LOBJ:
      rm = large_pool.dealloc_(h, ptr);
      break;

    default:
      throw fatal_error("gctype corrupt - got " + h->gcflags_str());
    }
    S.dealloc(sz, usz);
    // If we actually removed an object - update any pointers
    // will also update weak pointers.
    if (rm)
      alf::gc::gc_update_pointers();
  }
  return rm;
}

//////////////////////////////////
// allocate

void alf::gc::deallocate(void * ptr)
{
  deallocate_(ptr);
}

//////////////////////////////////
// root ptr functions

// register a top level pointer.
// any pointer registered this way will be a root pointer for gc walk.
void alf::gc::register_root_ptr_(const std::string & txt, gcobj ** pp)
{
  ptr_pool.ptr_register(txt, pp, gc_pool, f_pool, large_pool);
}

void alf::gc::unregister_root_ptr_(gcobj ** pp)
{
  ptr_pool.ptr_unregister(pp);
}

void alf::gc::unregister_all_root_ptrs();

//////////////////////////////////
// data<..> functions

void alf::gc::register_obj_(const std::string & txt, void * d,
			    void f(const std::string &, void *))
{
  fptr_pool.fun_register(gc_pool, f_pool, large_pool, txt, d, f);
}

void alf::gc::unregister_obj_(void * d)
{
  fptr_pool.fun_unregister(d);
}

void alf::gc::unregister_all_objs_(void * d)
{
  fptr_pool.fun_unregister_all(d);
}

void alf::gc::unregister_all_objs_()
{
  fptr_pool.fun_unregister_all();
}

//////////////////////////////////
// weak ptr functions

void alf::gc::register_weak_pointer_(gcobj * & p)
{
  wptr_pool.wptr_register(gc_pool, f_pool, large_pool, p);
}

void alf::gc::unregister_weak_pointer_(gcobj * & p)
{
  wptr_pool.wptr_unregister(p);
}

void alf::gc::unregister_all_weak_pointers_(gcobj * & p)
{
  wptr_pool.wptr_unregister_all(p);
}

void alf::gc::unregister_all_weak_pointers()
{
  wptr_pool.wptr_unregister_all();
}

//////////////////////////////////
// statistics functions

// some functions provided for statistics.
int alf::gc::num_allocs() // number of allcoations (new).
{
  return S.n_a;
}

int alf::gc::num_deallocs() // number of deallocations (delete).
{
  return S.n_d;
}

int alf::gc::num_cur_allocs() // number of currently allocated objects.
{
  return S.n_cur_a();
}

std::size_t alf::gc::usize_allocated() // total size of allocations.
{
  return S.usz_a;
}

std::size_t alf::gc::usize_deallocated() // total size of deallocations.
{
  return S.usz_d;
}

// total size of currently allocated objects.
std::size_t alf::gc::usize_cur_allocated()
{
  return S.usz_cur_a();
}

// total size including overhead and pointer pool.
std::size_t alf::gc::size_allocated()
{
  return S.sz_a;
}

std::size_t alf::gc::size_deallocated() // total size of deallocations.
{
  return S.sz_d;
}

// total size of currently allocated objects.
std::size_t alf::gc::size_cur_allocated()
{
  return S.sz_cur_a();
}

int alf::gc::num_frozen()
{
  return S.n_freeze;
}

int alf::gc::num_unfrozen()
{
  return S.n_unfreeze;
}

int alf::gc::num_cur_frozen()
{
  return S.n_cur_f();
}

std::size_t alf::gc::usize_frozen()
{
  return S.usz_f;
}

std::size_t alf::gc::usize_unfrozen()
{
  return S.usz_u;
}

std::size_t alf::gc::usize_cur_frozen()
{
  return S.usz_cur_f();
}

std::size_t alf::gc::size_frozen()
{
  return S.sz_f;
}

std::size_t alf::gc::size_unfrozen()
{
  return S.sz_u;
}

std::size_t alf::gc::size_cur_frozen()
{
  return S.sz_u;
}

// return total time in seconds spent on gc.
// pointer will receive time spent including nano seconds. 
time_t alf::gc::time_gc(struct timeval * ptv /* = 0 */ )
{
  return S.time_gc(ptv);
}

int alf::gc::num_gc() // number of times gc() is called.
{
  return S.n_gc;
}

void alf::gc::reset_num_gc() // reset num_gc() and time_gc().
{
  S.reset_num_gc();
}

bool alf::gc::in_gc()
{
  return S.in_gc;
}

void alf::gc::gc() // explicit call to gc.
{
  struct timeval start;
  struct timeval stop;
  struct timeval diff;

  if (! S.in_gc) {
    S.in_gc = true;
    gettimeofday(& start, 0);
    gc_pool.do_gc_(ptr_pool, fptr_pool, large_pool, f_pool, wptr_pool);
    gettimeofday(& stop, 0);
    timersub(& stop, & start, & diff);
    S.gc_add_timing(diff);
    S.in_gc = false;
  }
}

void alf::gc::gc_update_pointers()
{
  gc_pool.do_gc_update_pointers(ptr_pool, fptr_pool, large_pool,
				f_pool, wptr_pool);
}

std::size_t alf::gc::pool_size() // size of current gc pool.
{
  return gc_pool.size();
}

// resize current gc pool. This will trigger a gc().
void alf::gc::resize(std::size_t newsz)
{
  gc_pool.resize(newsz);
}

// Set/get the size threshold for putting objects in large pool.
std::size_t alf::gc::large_size()
{
  return large_sz;
}

// Set the size, return old size.
// if newsz < 4096, it is set to 4096.
std::size_t alf::gc::set_large_size(std::size_t newsz)
{
  if (newsz < 4096) newsz = 4096; // large_size is at least 4k.
  std::size_t osz = large_sz;
  large_sz = newsz;
  return osz;
}

std::ostream & alf::gc::report(std::ostream & os)
{
  return S.report(os);
}
