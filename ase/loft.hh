// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_LOFT_HH__
#define __ASE_LOFT_HH__

#include <ase/defs.hh>
#include <utility>

namespace Ase {

/// Loft - A special purpose memory allocator for lock- and obstruction-free thread progress.
namespace Loft {

/// Flags for allocator behavior.
enum Flags : size_t {
  PREFAULT_PAGES = 1,
};

} // Loft

/// A std::unique_ptr<> deleter for Loft allocations.
struct LoftFree {
  size_t size = 0;
  void (*dtor_)     (void*) = nullptr;
  void   operator() (void*);
};

/// A std::unique_ptr<> for Loft allocations.
template<typename T>
using LoftPtr = std::unique_ptr<T,LoftFree>;

/// Allocate `size` bytes (with limited alignment support), may return a nullptr.
LoftPtr<void>   loft_alloc              (size_t size, size_t align = 0);

/// Allocate and 0-initialize `nelem * elemsize` bytes, may return a nullptr.
LoftPtr<void>   loft_calloc             (size_t nelem, size_t elemsize, size_t align = 0);

/// Construct an object `T` from Loft memory, wrapped in a unique_ptr.
template<class T, class ...Args>
LoftPtr<T>      loft_make_unique        (Args &&...args);

/// Calculate the real bucket size allocated for a requested size.
size_t          loft_bucket_size        (size_t nbytes);

/// Configuration for Loft allocations.
struct LoftConfig {
  size_t preallocate = 0;       ///< Amount of preallocated available memory
  size_t watermark = 0;         ///< Watermark to trigger async preallocation
  Loft::Flags flags = Loft::Flags (0);
};

/// Configure watermark, notification, etc.
void   loft_set_config       (const LoftConfig &config);

/// Retrieve current configuration.
void   loft_get_config       (LoftConfig &config);

/// Install obstruction free callback to notify about watermark underrun.
void   loft_set_notifier     (const std::function<void()> &lowmem);

/// Grow the preallocated arena, issues syscalls.
size_t loft_grow_preallocate (size_t preallocation_amount = 0);


/// Statistics for Loft allocations.
struct LoftStats {
  std::vector<std::pair<size_t,size_t>> buckets; // size,count
  size_t narenas = 0;           /// Total number of arenas
  size_t available = 0;         /// Memory still allocatable from Arenas
  size_t allocated = 0;         /// Memory preallocated in Arenas
  size_t maxchunk = 0;          /// Biggest consecutive allocatable chunk.
};

/// Get statistics about current Loft allocations.
void      loft_get_stats    (LoftStats &stats);

/// Stringify LoftStats.
String    loft_stats_string (const LoftStats &stats);

namespace Loft {

/// Internal Helper
struct AllocatorBase {
protected:
  static void*  loft_btalloc    (size_t size, size_t align);
  static void   loft_btfree     (void *p, size_t size);
};

/// Allocator for STL containers.
template<typename T>
class Allocator : AllocatorBase {
public:
  using value_type = T;
  /**/                 Allocator  () noexcept = default;
  template<typename U> Allocator  (const Allocator<U>& other) noexcept   {}
  bool                 operator== (const Allocator<T> &o) const noexcept { return true; }
  bool                 operator!= (const Allocator<T> &o) const noexcept { return !this->operator== (o); }
  constexpr void       deallocate (T *p, size_t n) const noexcept        { loft_btfree (p, n); }
  [[nodiscard]] constexpr T*
  allocate (std::size_t n) const
  {
    void *const mem = loft_btalloc (n * sizeof (T), alignof (T));
    if (!mem)
      throw std::bad_alloc();
    return reinterpret_cast<T*> (mem);
  }
  static constexpr bool allows_read_after_free = true;
};

} // Loft

// == implementations ==
template<class T, class ...Args> inline LoftPtr<T>
loft_make_unique (Args &&...args)
{
  LoftPtr<void> vp = loft_alloc (sizeof (T), alignof (T));
  if (!vp) return {};
  T *const t = new (vp.get()) T (std::forward<Args> (args)...);
  LoftFree lfree = vp.get_deleter();
  void *vptr = vp.release();
  ASE_ASSERT_WARN (t == vptr); // require T* == void*
  lfree.dtor_ = [] (void *p) {
    T *const t = static_cast<T*> (p);
    t->~T();
  };
  // dprintf (2, "--LOFT: %s: ctor: %s: size=%zd ptr=%p dtor_=%p\n", __func__, typeid_name<T>().c_str(), lfree.size, t, lfree.dtor_);
  return std::unique_ptr<T,LoftFree> (t, lfree);
}

} // Ase

#endif /* __ASE_LOFT_HH__ */
