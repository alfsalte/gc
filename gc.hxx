#ifndef __ALF_GC_HXX__
#define __ALF_GC_HXX__

#include <exception>
#include <iostream>
#include <string>

namespace alf {

namespace gc {

class gcobj;

// if you have a pointer that isn't handled by gc you might want to
// know if the pointer is still good. I.e. does it point to a valid gcobj
// instance?

bool gc_pointer_ok_(gcobj * p);
bool gc_data_ok_(const void * p); // p == 0 or data is ok.
bool gc_data_ok_(const void * p, const void * q); // p == 0 or data is ok.
bool gc_data_ok_(const void * p, std::size_t sz); // p == 0 or data is ok.
bool gc_data_ok_nz_(const void * p); // data is ok and p != 0.
bool gc_data_ok_nz_(const void * p, const void * q); // data is ok and p != 0.
bool gc_data_ok_nz_(const void * p, std::size_t sz); // data is ok and p != 0.
bool gc_nogc_data_ok_(const void * p); // data is not in gc area.
bool gc_nogc_data_ok_(const void * p, const void * q);
bool gc_nogc_data_ok_(const void * p, std::size_t sz);
bool gc_nogc_data_ok_nz_(const void * p); // data is not in gc area.
bool gc_nogc_data_ok_nz_(const void * p, const void * q);
bool gc_nogc_data_ok_nz_(const void * p, std::size_t sz);

// This one checks that the pointer is ok and that the object it points
// to is a valid T instance (T must be a managed class).
template <typename T>
inline
bool gc_pointer_ok(T * p)
{
  return gc_pointer_ok_(p) && dynamic_cast<T *>((gcobj *)p) == p;
}

// This one checks that a reference is ok.
template <typename T>
inline
bool gc_data_ok(T & d)
{ return gc_data_ok_nz_(& d, sizeof(T)); }

template <typename T>
inline
bool gc_nogc_data_ok(T & d)
{ return gc_nogc_data_ok_(& d, sizeof(T)); }

// referenced by gcobj class.
gcobj * gc_walk_(const std::string & txt, gcobj * ptr);
void * allocate(size_t sz);
void deallocate(void * ptr);

// register a top level pointer.
// any pointer registered this way will be a root pointer for gc walk.
// if the pointer is inside a gcobj object directly or indirectly by
// being in a struct that is inside a gcobj, you must supply the
// pointer to the gcobj that contain the pointer. If the pointer is static,
// global or otherwise not in a gcobj - set ob to 0.
void register_root_ptr_(const std::string & txt, gcobj ** pp);

// unregister this root ptr. If you have registered the same root
// pointer several times you need to call this for each register
// or you can call the function below to unregister all of them at once.
// Note: if you have registered the same root ptr with several
// different texts you might not unregister the same as you registered.
// However, your registration will then go away when you unregister
// the same root ptr again - eventually.
void unregister_root_ptr_(gcobj ** pp);
// unregister all registrations of this root ptr.
void unregister_all_root_ptrs(gcobj ** pp);
// unregister all registrations.
void unregister_all_root_ptrs();

// register a non-gcobj object and gc_walk function.
void register_obj_(const std::string & txt, void * obj,
		   void f(const std::string &, void *));

void unregister_obj_(void * obj);
void unregister_all_objs_(void * obj);
void unregister_all_objs_();

void register_weak_pointer_(gcobj * & p);
void unregister_weak_pointer_(gcobj * & p);
void unregister_all_weak_pointers_(gcobj * & p);
void unregister_all_weak_pointers();

//////////////////////////////
// register_root_ptr

// T has gcobj as baseclass.
template <typename T>
inline
void register_root_ptr(const std::string & txt, T * & p)
{ register_root_ptr_(txt, reinterpret_cast<gcobj **>(& p)); }

// T has gcobj as baseclass.
template <typename T>
inline
void unregister_root_ptr(T * & p)
{ unregister_root_ptr_(& p); }

////////////////////////////////
// register_obj

template <typename T>
inline
void
register_obj(const std::string & txt,
	     T & d,
	     void f(const std::string &, T &))
{
  typedef void walk_func(const std::string &, T &);

  register_obj_(txt, & d, reinterpret_cast<walk_func *>(f));
}

template <typename T>
inline
void
register_obj(const std::string & txt,
	     T * p,
	     void f(const std::string &, T *))
{
  register_obj_(txt, p, f);
}

// T is some struct or class that does not have gcobj as baseclass.
template <typename T>
inline
void unregister_obj(T & obj)
{ unregister_obj_(reinterpret_cast<void *>(& obj)); }

// T is some struct or class that does not have gcobj as baseclass.
template <typename T>
inline
void unregister_obj(T * obj)
{ unregister_obj_(reinterpret_cast<void *>(obj)); }

////////////////////////////////
// register_weak_pointer
template <typename T>
void register_weak_pointer(T * & p)
{ register_weak_pointer_(reinterpret_cast<gcobj * &>(p)); }

template <typename T>
void unregister_weak_pointer(T * & p)
{ unregister_weak_pointer_(reinterpret_cast<gcobj * &>(p)); }

template <typename T>
void unregister_all_weak_pointers(T * & p)
{ unregister_all_weak_pointers_(reinterpret_cast<gcobj * &>(p)); }


////////////////////////////////
// gc_walk

// T has gcobj as baseclass.
// txt can be used to describe the pointer.
template <typename T>
inline
void gc_walk(const std::string & txt, T * & ptr)
{ ptr = reinterpret_cast<T *>(gc_walk_(txt, ptr)); }


////////////////////////////
// gcobj
//
// baseclass for all managed objects.
class gcobj {
public:
  gcobj() { }
  virtual ~gcobj();

  // This must be implemented by all managed objects.
  //
  //
  // The body of it contains calls to gc_walk (see below)
  // for each pointer in the object.
  // txt is a string that can be used to identify the object reached
  // through a pointer.
  // Note that weak pointers should not be walked - that's the point
  // objects pointed to by only weak pointers are not visible and
  // so should be removed by gc - gc will also set the weak pointer
  // to 0 for any objects it removes.
  virtual void gc_walker(const std::string & txt) = 0;

  void * operator new(size_t sz) { return allocate(sz); }
  void operator delete(void * p) { deallocate(p); }

  // reallocation is not provided - it may involve moving the object
  // which will invalidate any pointers to it.
  // You can achieve almost the same effect by allocating a new object
  // and copy data to it and if the smaller object then have a pointer
  // to the larger object and refer to it whenever any operations are
  // requested on it.

  // Note that if object is large, neither freeze nor unfreeze will
  // actually move the object. Large objects never move.

  // freeze an object. Moves it to frozen pool.
  // if do_ptrs is true we do a gc_update_pointers() before we return.
  // This is true even if we didn't actually move any object (but we
  // might have moved objects on previous freeze/unfreeze where we
  // had this arg false).
  static gcobj * S_freeze_(gcobj * ptr, bool do_ptrs = true);
  // unfreeze an object, moves it back to gc pool.
  static gcobj * S_unfreeze_(gcobj * ptr, bool do_ptrs = true);

  // freeze an object. T has gcobj as baseclass.
  // This locks the object in memory so that it won't move until unfrozen.
  // This allow you to pass the object to code that cannot handle moving
  // objects.
  template <typename T>
  static void freeze(T * & ptr, bool do_ptrs = true)
  { ptr = (T *)S_freeze_(ptr, do_ptrs); }

  template <typename T>
  static void unfreeze(T * & ptr, bool do_ptrs = true)
  { ptr = (T *)S_unfreeze_(ptr, do_ptrs); }


private:

  // declared but not defined - do not call this.
  void * operator new [] (size_t sz) = delete;

}; // end of class gcobj

////////////////////////////
// freeze/unfreeze

// T has gcobj as baseclass.
// if do_ptrs is true we will do a gc_update_pointers after freeze.
// only reason you might want that to be false is if you are going
// to immediately call freeze or unfreeze again for additional objects
// and there's no point to update pointers until you have done
// all of them. In that case let all except the last have do_ptrs
// set to false.
template <typename T>
inline
void freeze(T * & ptr, bool do_ptrs = true)
{ ptr = (T *)gcobj::S_freeze_(ptr, do_ptrs); }

template <typename T>
inline
void unfreeze(T * & ptr, bool do_ptrs = true)
{ ptr = (T *)gcobj::S_unfreeze_(ptr, do_ptrs); }

/////////////////////////////
// gcdataobj

// if an object contains no pointer, it can inherit from this
// class - it provides a dummy walker that does nothing as it
// assumes the object has only data but no pointers.
// It can contain pointers but not pointers to gcobj objects
// unless they are weak pointers nor pointers to objects that
// directly or indirectly have non-weak pointers to gcobj objects.
class gcdataobj : public gcobj {
public:
  gcdataobj() { }
  virtual ~gcdataobj();

  virtual void gc_walker(const std::string &);

}; // end of class gcdataobj

////////////////////////
// pointer

// this helps on registered root pointers. It registers itself
// on construction and unregisters itself upon destruction.
// You can use this as local variable in a stack for example
// to always have a registered pointer as local variable.
template <typename T>
class pointer {
public:

  typedef T * pointer_type;
  typedef T & ref_type;
  typedef const T * const_pointer_type;
  typedef const T & const_ref_type;

  pointer() : p_(0) { }

  pointer(const std::string & txt)
    : p_(0)
  { register_root_ptr(txt, p_); }

  pointer(const std::string & txt, T * p)
    : p_(p)
  { register_root_ptr(txt, p_); }

  pointer(const std::string & txt, const pointer & p)
    : p_(p.p_)
  { register_root_ptr(txt, p_); }

  ~pointer() { this->gc_unregister_all(); }

  pointer & operator = (T * p) { p_ = p; return *this; }
  pointer & operator = (const pointer & p) { p_ = p.p_; return *this; }

  pointer & gc_register(const std::string & txt)
  { register_root_ptr(txt, p_); }

  pointer & gc_unregister() { unregister_root_ptr(p_); }

  pointer & gc_unregister_all()
  { unregister_all_root_ptrs(reinterpret_cast<gcobj **>(& p_)); }

  operator const T * () const { return p_; }
  operator T * () { return p_; }
  T * operator -> () { return p_; }
  const T * operator -> () const { return p_; }

  const T & operator * () const { return *p_; }
  T & operator * () { return *p_; }

  // ++, --, += and so on is not defined since registered pointers
  // are used to point to single objects and the address value
  // isn't of any particular interest.

private:

  T * p_;

}; // end of class pointer

////////////////////////////////////
// weak_pointer

template <typename T>
class weak_pointer {
public:

  weak_pointer() : p_(0) { register_weak_pointer(p_); }
  weak_pointer(T * p) : p_(p) { register_weak_pointer(p_); }

  weak_pointer(const weak_pointer & p)
    : p_(p.p_)
  { register_weak_pointer(p_); }

  ~weak_pointer() { unregister_weak_pointer(p_); }

  operator const T * () const { return p_; }
  operator T * () { return p_; }
  T * operator -> () { return p_; }
  const T * operator -> () const { return p_; }

  const T & operator * () const { return *p_; }
  T & operator * () { return *p_; }

  weak_pointer & wptr_register()
  { register_weak_ptr(p_); }

  weak_pointer & wptr_unregister() { unregister_weak_pointer(p_); }
  weak_pointer & wptr_unregister_all() { unregister_all_weak_pointers(p_); }

  static
  void unregister_all_weak_pointers()
  { alf::gc::unregister_all_weak_pointers(); }

private:

  T * p_;

};

///////////////////////////////////////////
// if user do gc_walk on a weak pointer we will have none of it!
template <typename T>
inline
void gc_walk(const std::string &, weak_pointer<T> &)
{ }

//////////////////////////////////////
// data

// wrapper class for non-gcobj object.
// T is assumed to have a static function named
// gc_walker(const std::string & txt) that does the gc_walk.

// say you have a class or struct foo and it is written something
// like this - A and B are assumed to be two subclasses of gcobj.
//
// struct foo {
//     A * aptr;
//     B * bptr;
//
//     static void gc_walker(const std::string & txt)
//     {
//         alf::gc::gc_walk(txt + ".aptr", aptr);
//         alf::gc::gc_walk(txt + ".bptr", bptr);
//     }
// }; // end of struct foo
//
//
//    data<foo> X("X");
//
//    X is a foo, i.e. X.aptr and X.bptr works as expected. Any methods
//    in foo can be called as X.method(...).
//    However, X is also registered in gc and it will walk through
//    and update the pointers X.aptr and X.bptr as the objects they are
//    pointing to are moved around by gc.
//    When the object X is destroyed, the object is automatically
//    unregistered. If you need a default constructor
//    you need to explicitly register it later by calling
//    X.register_obj("text") - where "text" is the descriptive text.
//    This text can be used in error detection and can be used to figure
//    out exactly which pointer is wrong if for example a dangling pointer
//    is detected.

template <typename T>
struct data : public T {

  data(const std::string & txt)
  { alf::gc::register_obj(txt, this, T::gc_walker); }

  data() { /* defer registration */ }

  // use this if T has constructor with args.
  template <typename... Args>
  data(const std::string & txt, Args... args)
    : T(args...)
  { alf::gc::register_obj(txt, this, T::gc_walker); }

  ~data() { gc_unregister_all_objs(); }

  data & gc_register_obj(const std::string & txt)
  { alf::gc::register_obj(txt, this, T::gc_walker); }

  data & gc_unregister_obj() { alf::gc::unregister_obj(this); }
  data & gc_unregister_all_objs() { alf::gc::unregister_all_objs_(this); }

}; // end of struct data

// To mimic the gc_walk function above for non-gcobj objects:

template <typename T>
inline
void gc_walk_not_gcobj(const std::string & txt, T * ptr)
{
  if (ptr) ptr->gc_walker(txt);
}

template <typename T>
inline
void gc_walk_not_gcobj(const std::string & txt, T & ref)
{
  ref.gc_walker(txt);
}

// use these if gc_walker is a (regular or virtual) member function.
template <typename T>
inline
void gc_walk(const std::string & txt, data<T> & ref)
{ ref.gc_walker(txt); }

template <typename T>
inline
void gc_walk(const std::string & txt, data<T> * ptr)
{ if (ptr) ptr->gc_walker(txt); }

// use these if gc_walker is a static function instead
// with signature void T::gc_walker(const std::string & txt, T & ref)
template <typename T>
inline
void gc_walk_s(const std::string & txt, data<T> & ref)
{ T::gc_walker(txt, ref); }

// or if gc_walker has signature
// T::gc_walker(const std::string & txt, T * ptr)
template <typename T>
inline
void gc_walk_s(const std::string & txt, data<T> * ptr)
{
  if (ptr) T::gc_walker(txt, ptr);
}

// Note that if a class is both used as data<Klass> as well as
// this gc_walk_not_gcobj function it needs two gc_walker functions:
// static void gc_walker(const std::string & txt, Klass & ref);
// and
// /* virtual */
// void gc_walker(const std::string & txt);
// typically the static one will then simply call the regular
// (virtual) member function so it will look like this:
//
// // static
// void Klass::gc_walker(const std::string & txt, Klass & ref)
// {
//    ref.gc_walker(txt);
// }
//

//////////////////////////////////////
// other functions.

// some functions provided for statistics.
int num_allocs(); // number of allcoations (new).
int num_deallocs(); // number of deallocations (delete).
int num_cur_allocs(); // number of currently allocated objects.

// without overhead.
std::size_t usize_allocated(); // total size of allocations.
std::size_t usize_deallocated(); // total size of deallocations.
std::size_t usize_cur_allocated(); // size of currently allocated objects.

// including overhead.
std::size_t size_allocated(); // total size of allocations.
std::size_t size_deallocated(); // total size of deallocations.
std::size_t size_cur_allocated(); // size of currently allocated objects.

int num_frozen();
int num_unfrozen();
int num_cur_frozen();

std::size_t usize_frozen();
std::size_t usize_unfrozen();
std::size_t usize_cur_frozen();

std::size_t size_frozen();
std::size_t size_unfrozen();
std::size_t size_cur_frozen();

// return total time in seconds spent on gc.
// pointer will receive time spent including nano seconds. 
time_t time_gc(struct timeval * tv = 0);

int num_gc(); // number of times gc() is called.

void reset_num_gc(); // reset num_gc() and time_gc().

// return true if we have started but not yet completed a gc.
// This should always be true inside gc_walker functions but if
// those functions calls other functions you might want to test
// for this case. For example - attempt to allocate new area while inside
// gc is generally a bad idea.
bool in_gc();

////////////////////////////////////
// gc

void gc(); // explicit call to gc.

////////////////////////////
// gc_update_pointers

// This is a version of gc that does not reclaim or move any data
// but only update all pointers if any objects has been removed
// or moved previously.
void gc_update_pointers();

std::size_t pool_size(); // size of current gc pool.

// resize current gc pool. This will trigger a gc().
void resize(std::size_t newsz);

// Set/get the size threshold for putting objects in large pool.
std::size_t large_size();

// Set the size, return old size.
// if newsz < 256, it is set to 256.
std::size_t set_large_size(std::size_t newsz);

std::ostream & report(std::ostream & os);

inline
void report()
{ report(std::cout); }

//////////////////////////////////
// gc_error

// baseclass for all our exceptions.
class gc_error : public std::exception {
public:

  gc_error(const std::string & text) : M_(text) { }
  gc_error(std::string && text) : M_(std::move(text)) { }
  gc_error(const char * text) : M_(text) { }
  virtual ~gc_error();

  virtual const char * what() const throw();

protected:

  std::string M_;

};

// exceptions

/////////////////////////////////
// dangling_pointer

// a pointer that points to removed object.
class dangling_pointer : public gc_error {
public:

  dangling_pointer(const std::string & text) : gc_error(text) { }
  dangling_pointer(std::string && text) : gc_error(std::move(text)) { }
  dangling_pointer(const char * text) : gc_error(text) { }

  virtual ~dangling_pointer();

}; // end of class dangling_pointer

//////////////////////////////////////
// fatal_error

class fatal_error : public gc_error {
public:

  fatal_error(const std::string & text) : gc_error(text) { }
  fatal_error(std::string && text) : gc_error(std::move(text)) { }
  fatal_error(const char * text) : gc_error(text) { }

}; // end of class fatal_error

/////////////////////////////////////////
// gc_allocation_error

class gc_allocation_error : public std::bad_alloc {
public:

  gc_allocation_error() { }
  gc_allocation_error(const std::string & text) : M_(text) { }
  gc_allocation_error(std::string && text) : M_(std::move(text)) { }
  gc_allocation_error(const char * text) : M_(text) { }
  virtual ~gc_allocation_error();

  gc_allocation_error & emsg(const std::string & text)
  { M_ = text; return *this; }

  gc_allocation_error & emsg(std::string && text)
  { M_ = std::move(text); return *this; }

  gc_allocation_error & emsg(const char * text)
  { M_ = text; return *this; }

  virtual const char * what() const throw();

protected:

  std::string M_;

}; // end of class gc_allocation_error.

}; // end of namespace gc

}; // end of namespace alf

#endif
