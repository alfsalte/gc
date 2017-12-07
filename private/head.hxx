#ifndef __GC_PRIV_HEAD_HXX__
#define __GC_PRIV_HEAD_HXX__

#include <cstdlib>

#include "../gc.hxx"
#include "moved.hxx"
#include "removed.hxx"
#include "fremoved.hxx"
#include "tail.hxx"
#include "minipool.hxx"

namespace alf {

namespace gc {

// this is a head for each gcobj object.
// i.e. each gcobj allcoated is preceeded by a head obj and succeded
// by a tail (see below).
// every allocation consists of HEAD + OBJ + GAP + TAIL.
// HEAD is the header declared in this file.
// TAIL is the tail declared in tail.hxx
// OBJ is usz bytes of user data.
// GAP is 0-sizeof(size_t) - 1 bytes to ensure TAIL is on size_t alignment.

// HEAD ends with a deadbeef area (data filled with the value 0xdeadbeef)
// and TAIL starts with a similar deadbeef area to guard
// against user accidently modifying head or tail if he access data just
// after or before his own area.

// head_base contain all data in header except the deadbeef area.

////////////////////////////////
// head_base

struct head_base {

  std::size_t magic; // magic value.
  unsigned int flags; // flags
  unsigned int fcnt; // frozen counter -  should be 0 in GCpool
  std::size_t sz; // size of head + gcobj + tail, i.e. pointer to next head.
  std::size_t usz; // user requested size of gcobj. Arg to new.

  // pointer to the user object.
  // Normally this is simply hdr->obj() i.e. pointing to end of head.
  // If moved it will point to the object's new location
  // If removed it will be 0.
  union {
    void * vp; // void ptr to object.
    gcobj * p; // ptr to object.
    moved * mv_p; // ptr to moved object.
    removed * rm_p; // ptr to removed object.
    Fremoved * Frm_p; // ptr to Fremoved object.
  };

  // pointer to minipool for GCpool objects and
  // Fminipool for Fpool objects.
  // this is 0 for Lpool objs.

  minipool * mp; // pointer to minipool which this block belongs to.

  // This is the modified user requested size of gcobj.
  // user may request any size but we make sure that asz is always
  // a multiple of 8 and that is the size allocated for the obj.
  // size_t asz; // asz == (usz + 7) & -8;

  // if there is a filler before and/or after the object we would also
  // need size of the first filler. In this case the allocation would
  // look like:
  // HEAD + PRE + OBJ + POST + TAIL
  //
  // In this we would need the size of PRE. Size of POST is not necessary
  // since sz - sizeof(tail) would give pointer to tail.
  // sizeof(head) + sizeof(PRE) + asz would give pointer to POST.
  // sz - sizeof(head) - sizeof(tail) - asz - sizeof(PRE)
  // gives you size of POST.

  // adjust value to make it at least as large as sizeof(Fremoved)
  // and also make it a multiple of sizeof(std::size_t).
  static constexpr std::size_t asz(std::size_t usz)
  {
    std::size_t x = usz < sizeof(Fremoved) ? sizeof(Fremoved) : usz;
    
    return (x + (sizeof(std::size_t) - 1)) & -sizeof(std::size_t);
  }

}; // end of struct head_base.

////////////////////////////////
// head_base__
struct head_base__ : head_base {

  enum { HEADSZ__ = 80 };

  char deadbeef[HEADSZ__ - sizeof(head_base)];

}; // end of struct head_base__

////////////////////////////////
// head

struct head : head_base__ {

  enum { MAGIC = 0x127f3b0da95a29dbULL };

  // flags values.
  enum {
    // lower bits give gctype of object.

    // GC pool values.

    GCOBJ     = 1, // regular GCpool object

    // GCpool object has moved. In its place is a gc::moved object
    // which contains a pointer to the new location of the object.
    GCMOVED   = 2,

    // GCpool object has been removed. Either explicitly by user
    // or implicitly by garbage collector.
    GCRM      = 3,

    // GCpool object has moved to Fpool (frozen).
    GCFROZEN  = 4, // moved to F pool.

    // Fpool values.
    FROZEN    = 5, // regular obj in Fpool. 

    // Object has moved back to GCpool. In its place is an
    // Fremoved object that also have a pointer to the new location
    // in hdr->p. The block is also inserted into free list
    // and is available for others to allocate.
    UNFROZEN  = 6, // moved back to GCpool.

    // Object has been deleted while frozen. In its place is an
    // Fremoved object with hdr->p == 0.
    FREMOVED  = 7, // deleted while frozen.

    // This block is no longer a separate block. The header is kept in
    // place as long as possible with the gctype() FMERGED but
    // this block is actually in the user part of the previous block
    // and can be overwritten by user any moment.
    // To find the start of the block get prev_head() which should have
    // a size that includes this block.
    FMERGED   = 8, // object is removed and block is merged with prev blk.

    // Regular object in Lpool.
    LOBJ      = 9, // regular obj in L pool

    // This object has been removed from Lpool. This is a very transient
    // object and will be deleted shortly after.
    LREMOVED  = 10, // L pool obj removed.

    // mask to get the various gctypes above.
    POOLMASK  = 0x0f,

    // This bit is usually 0, except when in the middle of gc_walk.
    // We set it upon first visit by gc_walk and then if set
    // we know we have done this object already and will merely update
    // the pointer that got us here.
    GCBIT = 0x80, // set this flag to detect circular pointers.

    // This bit is set if the object has moved.
    MOVED = 0x40,

    // This bit is set if the object has been removed.
    REMOVED = 0x20,

    // This bit is set if the object has been inserted into free list (Fpool).
    FREE = 0x10, // object is in free list (Fpool).
  };

  // return values form varios in_.... functions:
  enum {
    NOT_HERE, // area is not in the region.
    PARTIAL, // area is partially in the region.
    FULL, // area is fully in the region.
  };

  static
  constexpr
  head * BAD_BLOCK = reinterpret_cast<head *>(0x123);

  // GCOBJ start at 1 and we want an extra 0 at end so +2.
  static const char * S_gctypes[LREMOVED + 2];

  enum { HEADSZ = head_base__::HEADSZ__ };
  enum { MINBLKSZ = head_base::asz(sizeof(Fremoved)) };

  static const char * gctype_str(int t);
  static std::string gcflags_str(int f);

  const char * gctype_str() const { return gctype_str(flags); }
  std::string gcflags_str() const { return gcflags_str(flags); }

  bool magic_ok() const { return magic == MAGIC; }
  bool check(minipool * real_mp) const;
  bool check(minipool * real_mp, int gctype) const;

  int gctype() const { return flags & POOLMASK; }
  bool visited() const { return (flags & GCBIT) != 0; }

  bool set_visited()
  { bool v = (flags & GCBIT) != 0; flags |= GCBIT; return v; } 

  bool unvisit()
  { bool v = (flags & GCBIT) != 0; flags &= ~GCBIT; return v; }

  bool not_visited() const
  { return (flags & GCBIT) == 0; }

  bool has_moved() const { return (flags & MOVED) != 0; }
  bool is_removed() const { return (flags & REMOVED) != 0; }
  bool is_free() const { return (flags & FREE) != 0; }
  head & set_free() { flags |= FREE; return *this; }
  head & set_unfree() { flags &= ~FREE; return *this; }

  char * obj_charp() const
  { return reinterpret_cast<char *>(const_cast<head *>(this + 1)); }

  gcobj * obj() const
  { return reinterpret_cast<gcobj *>(const_cast<head *>(this + 1)); }

  moved * obj_mv() const
  { return reinterpret_cast<moved *>(const_cast<head *>(this + 1)); }

  removed * obj_rm() const
  { return reinterpret_cast<removed *>(const_cast<head *>(this + 1)); }

  Fremoved * obj_Frm() const
  { return reinterpret_cast<Fremoved *>(const_cast<head *>(this + 1)); }

  moved * obj_mv_safe() const
  { return dynamic_cast<moved *>(obj()); }

  removed * obj_rm_safe() const
  { return dynamic_cast<removed *>(obj()); }

  Fremoved * obj_Frm_safe() const
  { return dynamic_cast<Fremoved *>(obj()); }

  Fremoved * obj_Frm_safer() const;
  Fremoved * obj_Frm_safest() const;

  // pointer to end of user region.
  void * objend() const
  { return reinterpret_cast<void *>(obj_charp() + usz); }

  // return the minimum allocation size that can hold an area of size usz.
  static constexpr std::size_t asz_base(std::size_t usz)
  {
    return static_cast<std::size_t>
      ((usz + sizeof(std::size_t) - 1) & -sizeof(std::size_t));
  }


  static constexpr std::size_t asz(std::size_t usz)
  {
    // assumes Fremoved is the largest of our own objects.
    // i.e. sizeof(moved) <= sizseof(Fremoved)
    // and  sizeof(removed) <= sizeof(Fremoved)
    // The latter is obvious since Fremoved is a subclass of removed
    // and the former is also true since Fremoved contain two pointers
    // and moved also contain two pointers.
    return head_base::asz(usz);
  }

  static constexpr std::size_t bsz(std::size_t sz)
  {
    std::size_t k = head_base::asz(sizeof(head_base__) + sizeof(tail)
				   + sizeof(Fremoved));
    if (sz > k) k = head_base::asz(sz);
    return k;
  }

  std::size_t asz() const
  { return asz(usz); }

  char * next_head_charp() const
  { return reinterpret_cast<char *>(const_cast<head *>(this)) + sz; }

  head * next_head() const
  { return reinterpret_cast<head *>(next_head_charp()); }

  tail * prev_tail() const
  { return reinterpret_cast<tail *>(const_cast<head *>(this)) - 1; }

  head * prev_head() const
  { return prev_tail()->cur_head(); }

  tail * cur_tail() const
  { return reinterpret_cast<tail *>(next_head_charp()) - 1; }

  std::size_t allocated_size() const { return asz(usz); }
  std::size_t size() const { return sz; }
  std::size_t overhead() const { return sz - usz; }
  std::size_t obj_size() const { return usz; }

  std::size_t gap_size() const // space between object and tail.
  // sizeof(size_t) - (usz & (sizeof(size_t) - 1))
  { return sizeof(std::size_t) - (usz & (sizeof(std::size_t) - 1)); }


  gcobj * objbyptr() const { return p; }
  moved * objbyptr_m() const { return mv_p; }
  removed * objbyptr_rm() const { return rm_p; }
  Fremoved * objbyptr_Frm() const { return Frm_p; }

  // return true if p is pointing to a location inside this block.
  // return true if low <= p <= high
  static
  bool
  in_area_range(const void * low, const void * high, const void * p)
  { return low <= p && p <= high; }

  // return true if low <= p < high
  static
  bool
  in_area_loc(const void * low, const void * high, const void * p)
  { return low <= p && p < high; }

  // is area p..q completely inside low..high?
  // is low <= p <= q <= high - we assume p <= q and do not check for that.
  static
  bool
  in_area_loc(const void * low, const void * high,
	      const void * p, const void * q)
  { return low <= p && q <= high; }

  // is area at p of size z fully in the region defined by low..high?
  // i.e. is low <= p && p + z <= high?
  static
  bool
  in_area_loc(const void * low, const void * high,
	      const void * p, std::size_t z)
  {
    return in_area_loc(low, high, p,
		       reinterpret_cast<const void *>
		       (reinterpret_cast<const char *>(p) + z));
  }

  bool in_block(const void * p) const
  { return in_area_loc(this, next_head(), p); }

  bool in_block(const void * p, const void * q) const
  { return in_area_loc(this, next_head(), p, q); }

  bool in_block(const void * p, std::size_t sz) const
  { return in_block(p, reinterpret_cast<const char *>(p) + sz); }

  int in_block_(const void * p, const void * q) const;

  int in_block_(const void * p, std::size_t sz) const
  { return in_block_(p, reinterpret_cast<const char *>(p) + sz); }

  bool in_head(const void * p) const
  { return in_area_loc(this, this + 1, p); }

  bool in_obj(const void * p) const
  { return in_area_loc(this + 1, objend(), p); }

  bool in_obj(const void * p, const void * q) const
  { return in_area_loc(this + 1, objend(), p, q); }

  bool in_obj(const void * p, std::size_t sz) const
  { return in_obj(p, reinterpret_cast<const char *>(p) + sz); }

  int in_obj_(const void * p, const void * q) const;

  int in_obj_(const void * p, std::size_t sz) const
  { return in_obj_(p, reinterpret_cast<const char *>(p) + sz); }

  bool in_gap(const void * p) const // area after user obj before tail.
  { return in_area_loc(objend(), cur_tail(), p); }

  bool in_tail(const void * p) const
  { return in_area_loc(cur_tail(), next_head(), p); }
  
  gcobj * objbyptr_safe() const
  { return dynamic_cast<gcobj *>(p); }

  moved * objbyptr_m_safe() const
  { return dynamic_cast<moved *>(p); }

  removed * objbyptr_rm_safe() const
  { return dynamic_cast<removed *>(p); }

  Fremoved * objbyptr_Frm_safe() const
  { return dynamic_cast<Fremoved *>(p); }

  minipool * mpool() { return mp; }

  // although we some times have void pointers, obj should point to
  // a gcobj object.
  static head * get_head(void * obj)
  {
    if (obj == 0) return 0;
    return reinterpret_cast<head *>(obj) - 1;
  }

  static head * get_head_safe(void * obj);

  // fill n bytes at dest with the data in data, start with offset
  // off.
  static void fill(char * dest, unsigned int data, std::size_t n);


  // init header.
  void h_init(minipool * mp, int fl, std::size_t bsz, std::size_t usz);

  // init head and tail.
  void b_init(minipool * mp, int fl, std::size_t bsz, std::size_t usz);

  // check that p is a gcobj pointer pointing to the obj() position
  // of a block and return the head of that block.
  // This checks gc_pool, f_pool and l_pool.
  static
  head * pointer_ok(gcobj * p);

  // check that p is a data that is located somewhere inside a gcobj
  // and return the head of that block if ok.
  static
  head * data_ok(const void * p);

  static
  head * data_ok(const void * p, const void * q);

  static
  head * data_ok(const void * p, std::size_t sz)
  { return data_ok(p, reinterpret_cast<const char *>(p) + sz); }

  static
  bool nogc_data_ok(const void * p);

  static
  bool nogc_data_ok(const void * p, const void * q);

  static
  bool nogc_data_ok(const void * p, std::size_t sz)
  { return nogc_data_ok(p, reinterpret_cast<const char *>(p) + sz); }

}; // end of struct head.

// return values:
// 0 area is not in pool at all.
// 1 area is partially in pool
// 2 area is completely in pool.
// we assume p <= q.
inline
int minipool::block_in_pool_(const void * p, const void * q)
{
  if (q <= p_ || p_ + sz_ <= p)
    return head::NOT_HERE;
  if (p_ <= p && q <= p_ + sz_)
    return head::FULL;
  return head::PARTIAL;
}

inline
int minipool::block_in_pool_(const void * p, std::size_t sz)
{ return block_in_pool_(p, reinterpret_cast<const char *>(p) + sz); }

}; // end of namespace gc

}; // end of namespace alf


#endif
