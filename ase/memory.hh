// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MEMORY_HH__
#define __ASE_MEMORY_HH__

#include <ase/utils.hh>

namespace Ase {

/// Utilities for allocating cache line aligned memory from huge pages.
namespace FastMemory {

/// Minimum alignment >= cache line size, see getconf LEVEL1_DCACHE_LINESIZE.
inline constexpr size_t cache_line_size = 64;

} // FastMemory

// Allocate cache-line aligned memory block from fast memory pool, MT-Safe.
void*   fast_mem_alloc  (size_t size);
// Free a memory block allocated with aligned_malloc(), MT-Safe.
void    fast_mem_free   (void *mem);

/// Array with cache-line-alignment containing a fixed numer of PODs.
template<typename T, size_t ALIGNMENT = FastMemory::cache_line_size>
class FastMemArray {
  static_assert (std::is_trivially_copyable<T>::value);
  static_assert (ALIGNMENT <= FastMemory::cache_line_size);
  static_assert ((ALIGNMENT & (ALIGNMENT - 1)) == 0);
  static_assert (alignof (T) <= ALIGNMENT);
  const size_t n_elements_ = 0;
  T     *const data_ = nullptr;
  ASE_CLASS_NON_COPYABLE (FastMemArray);
protected:
  void     range_check (size_t n) const
  {
    if (n >= n_elements_)
      throw std::out_of_range (string_format ("FastMemArray::range_check: n >= size(): %u >= %u", n, size()));
  }
public:
  FastMemArray (size_t n_elements) :
    n_elements_ (n_elements),
    data_ ((T*) fast_mem_alloc (sizeof (T) * n_elements_))
  {
    std::fill (begin(), end(), T());
  }
  FastMemArray (const std::vector<T>& elements) :
    n_elements_ (elements.size()),
    data_ ((T*) fast_mem_alloc (sizeof (T) * n_elements_))
  {
    std::copy (elements.begin(), elements.end(), begin());
  }
  ~FastMemArray()
  {
    static_assert (std::is_trivially_destructible<T>::value);
    fast_mem_free (data_);
  }
  T*       begin      ()                { return &data_[0]; }
  T*       end        ()                { return &data_[n_elements_]; }
  size_t   size       () const          { return n_elements_; }
  T&       operator[] (size_t n)        { return data_[n]; }
  const T& operator[] (size_t n) const  { return data_[n]; }
  T&       at         (size_t n)        { range_check (n); return data_[n]; }
  const T& at         (size_t n) const  { range_check (n); return data_[n]; }
};

namespace FastMemory {

// == NewDeleteBase ==
class NewDeleteBase {
  static constexpr const std::align_val_t __cxxalignment = std::align_val_t (__STDCPP_DEFAULT_NEW_ALIGNMENT__);
  static void    delete_  (void *ptr, std::size_t sz, std::align_val_t al);
  static void*   new_     (std::size_t sz, std::align_val_t al);
public:
  // 'static' is implicit for operator new/delete
  void* operator new      (std::size_t sz)                            { return new_ (sz, __cxxalignment); }
  void* operator new[]    (std::size_t sz)                            { return new_ (sz, __cxxalignment); }
  void* operator new      (std::size_t sz, std::align_val_t al)       { return new_ (sz, al); }
  void* operator new[]    (std::size_t sz, std::align_val_t al)       { return new_ (sz, al); }
  // variants without size_t MUST NOT be defined for the following to be used
  void  operator delete   (void *ptr, std::size_t sz)                 { delete_ (ptr, sz, __cxxalignment); }
  void  operator delete[] (void *ptr, std::size_t sz)                 { delete_ (ptr, sz, __cxxalignment); }
  void  operator delete   (void *ptr, std::size_t sz, std::align_val_t al) { delete_ (ptr, sz, al); }
  void  operator delete[] (void *ptr, std::size_t sz, std::align_val_t al) { delete_ (ptr, sz, al); }
};

/// Internal allocator handle.
struct Allocator;
using AllocatorP = std::shared_ptr<Allocator>;

/// Reference for an allocated memory block.
struct Block {
  void  *const block_start = nullptr;
  const uint32 block_length = 0;
  Block& operator= (const Block &src) { this->~Block(); new (this) Block (src); return *this; }
  /*ctor*/ Block   (void *b, uint32 l) : block_start (b), block_length (l) {}
  /*copy*/ Block   (const Block &src) = default;
  /*dflt*/ Block   () = default;
};

/// Memory area (over-)aligned to cache size and utilizing huge pages.
struct Arena {
  /// Create isolated memory area.
  explicit Arena     (uint32 mem_size, uint32 alignment = cache_line_size);
  /// Alignment for block addresses and length.
  size_t   alignment () const;
  /// Address of memory area.
  uint64   location  () const;
  /// Reserved memory area in bytes.
  uint64   reserved  () const;
  /// Create a memory block from cache-line aligned memory area, MT-Unsafe.
  Block    allocate  (uint32 length) const;
  Block    allocate  (uint32 length, std::nothrow_t) const;
  /// Realease a previously allocated block, MT-Unsafe.
  void     release   (Block allocatedblock) const;
protected:
  explicit Arena     (AllocatorP);
  AllocatorP fma; ///< Identifier for the associated memory allocator.
};

/// Interface to the OS huge page allocator.
struct HugePage {
  using HugePageP = std::shared_ptr<HugePage>;
  /// Alignment of the memory area.
  size_t alignment () const { return start_ ? size_t (1) << __builtin_ctz (size_t (start_)) : 0; }
  size_t size      () const { return size_; }          ///< Size in bytes of the memroy area.
  char*  mem       () const { return (char*) start_; } ///< Allocated memroy area.
  static HugePageP allocate  (size_t minimum_alignment, size_t bytelength);
protected:
  void  *const start_;
  const size_t size_;
  explicit HugePage (void *m, size_t s);
  virtual ~HugePage () {}
};
using HugePageP = HugePage::HugePageP;

} // FastMemory

/// Compact, deduplicating string variant for constant strings.
class CString final {
  uint quark_ = 0;
  void qset (uint quark) noexcept;
public:
  using size_type = std::string::size_type;
  using const_iterator = std::string::const_iterator;
  using const_reference = std::string::const_reference;
  using const_reverse_iterator = std::string::const_reverse_iterator;
  /*dtor*/          ~CString     () noexcept                     { qset (0); }
  /*ctor*/           CString     (const char *c)                 { assign (c); }
  /*ctor*/           CString     (const char *c, size_type s)    { assign (c, s); }
  /*ctor*/           CString     (const std::string &s)          { assign (s); }
  /*ctor*/ constexpr CString     () noexcept                     = default;
  /*copy*/ constexpr CString     (const CString &c) noexcept     { assign (c); }
  /*move*/ constexpr CString     (CString &&c) noexcept          { assign (c); }
  constexpr CString& operator=   (const CString &c) noexcept     { return assign (c); }
  constexpr CString& operator=   (CString &&c) noexcept          { return assign (c); }
  CString&           operator=   (const std::string &s) noexcept { return assign (s); }
  CString&           operator=   (const char *c) noexcept        { return assign (c); }
  CString&           operator=   (char ch) noexcept              { return assign (std::addressof (ch), 1); }
  CString&           operator=   (std::initializer_list<char> l) { return assign (l.begin(), l.size()); }
  constexpr CString& assign      (const CString &c) noexcept     { qset (c.quark_); return *this; }
  CString&           assign      (const std::string &s) noexcept;
  CString&           assign      (const char *c, size_type s)    { return assign (std::string (c, s)); }
  CString&           assign      (const char *c) noexcept        { return assign (std::string (c)); }
  CString&           assign      (size_type count, char ch) noexcept { return this->operator= (std::string (count, ch)); }
  const std::string& string      () const;
  const char*        c_str       () const noexcept      { return string().c_str(); }
  const char*        data        () const noexcept      { return string().data(); }
  const_reference    at          (size_type pos) const  { return string().at (pos); }
  const_reference    operator[]  (size_type pos) const  { return string().operator[] (pos); }
  size_type          capacity    () const noexcept      { return string().capacity(); }
  size_type          length      () const noexcept      { return string().length(); }
  bool               empty       () const noexcept      { return string().empty(); }
  size_type          size        () const noexcept      { return string().size(); }
  const_iterator     begin       () const noexcept      { return string().begin(); }
  const_iterator     cbegin      () const noexcept      { return string().cbegin(); }
  const_iterator     end         () const noexcept      { return string().end(); }
  const_iterator     cend        () const noexcept      { return string().cend(); }
  const_reverse_iterator rbegin  () const noexcept      { return string().rbegin(); }
  const_reverse_iterator crbegin () const noexcept      { return string().crbegin(); }
  const_reverse_iterator rend    () const noexcept      { return string().rend(); }
  const_reverse_iterator crend   () const noexcept      { return string().crend(); }
  /*conv*/  operator std::string () const noexcept      { return string(); }
  friend bool operator== (CString a, CString b)            { return a.quark_ == b.quark_; }
  friend bool operator== (CString a, const std::string &b) { return a.string() == b; }
  friend bool operator== (const std::string &a, CString b) { return a == b.string(); }
  friend bool operator== (CString a, const char *b)        { return a.string() == b; }
  friend bool operator== (const char *a, CString b)        { return a == b.string(); }
  friend std::strong_ordering operator<=> (CString a, CString b) { return a.string() <=> b.string(); }
  friend std::strong_ordering operator<=> (CString a, const String &b) { return a.string() <=> b; }
  friend std::strong_ordering operator<=> (const String &a, CString b) { return a <=> b.string(); }
  friend std::strong_ordering operator<=> (CString a, const char *b)   { return a.string() <=> b; }
  friend std::strong_ordering operator<=> (const char *a, CString b)   { return a <=> b.string(); }
  friend std::string operator+  (const std::string &s, CString c)      { return s + c.string(); }
  friend std::string operator+  (CString c, const std::string &s)      { return c.string() + s; }
  static CString     lookup     (const std::string &s);
  static uint        temp_quark_impl (CString c);
  static CString     temp_quark_impl (uint maybequark);
  friend std::ostream& operator<< (std::ostream &os, CString c)        { os << c.string(); return os; }
  static constexpr const std::string::size_type npos = -1;
};
using CStringS = std::vector<CString>;

} // Ase

namespace std {
template<>
struct hash<::Ase::CString> {
  /// Hash value, equal to the std::hash<> value of the corresponding std::string.
  size_t
  operator() (const ::Ase::CString &cs) const
  {
    return ::std::hash<::std::string>{} (cs.string());
  }
};
} // std

#endif /* __ASE_MEMORY_HH__ */
