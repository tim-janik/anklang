// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "loft.hh"
#include "platform.hh"
#include "atomics.hh"
#include "randomhash.hh"
#include "main.hh"
#include "internal.hh"
#include "testing.hh"
#include <sys/mman.h>
#include <wchar.h>

#define MDEBUG(...)     Ase::debug ("memory", __VA_ARGS__)
#define VDEBUG(...)     Ase::debug ("memory", __VA_ARGS__)      // more verbose

#define MEM_ALIGN(addr, ALIGNMENT)      (ALIGNMENT * size_t ((size_t (addr) + ALIGNMENT - 1) / ALIGNMENT))

namespace Ase::Loft {

// == LoftConfig ==
static constexpr size_t MINIMUM_HUGEPAGE = 2 * 1024 * 1024;
static std::atomic<Flags> config_flags = Flags (0);
static std::atomic<size_t> config_watermark = MINIMUM_HUGEPAGE;
static std::atomic<size_t> config_preallocate = 2 * MINIMUM_HUGEPAGE;
static std::atomic<std::function<void()>*> config_lowmem_cb = nullptr;
static std::atomic<size_t> config_lowmem_notified = 0;

struct ArenaSpan { uintptr_t addr, offset, size; };
using ArenaList = std::vector<ArenaSpan>;

// == BumpAllocator ==
/** BumpAllocator - satisfy allocations by bumping an offset into mmap based spans.
 * This allocator satisfies allocation requests from pre-allocated mmap regions.
 * The regions support transparent huge pages and are setup to allow upwards growth
 * instead of accumulating a long list of individual mappings.
 * As long as enough pre-allocated memory is available, allocation requests are served
 * via a single lock-free offset increment.
 * Atm, no memory is released back to the OS ever.
 */
struct BumpAllocator {
  void*  bump_alloc  (size_t size);
  size_t grow_spans  (size_t needed, bool preallocating);
  size_t free_block  () const;
  void   list_arenas (ArenaList &arenas);
  size_t totalmem    () const { return totalmem_; }
private:
  char* memmap     (size_t size, bool growsup);
  char* remap      (void *addr, size_t oldsize, size_t size);
  struct MMSpan {
    char *mmstart = nullptr;
    boost::atomic<uintptr_t> offset = 0;
    boost::atomic<uintptr_t> mmsize = 0;
    boost::atomic<MMSpan*> next = nullptr;
  };
  boost::atomic<size_t> totalmem_ = 0;
  boost::atomic<MMSpan*> spans_ = nullptr;
  std::mutex mutex_;
};

void
BumpAllocator::list_arenas (ArenaList &arenas)
{
  for (MMSpan *lspan = spans_; lspan; lspan = lspan->next)
    arenas.push_back ({ uintptr_t (lspan->mmstart), lspan->offset, lspan->mmsize });
}

size_t
BumpAllocator::free_block () const
{
  // find the last span
  MMSpan *lspan = spans_;
  while (lspan && lspan->next)
    lspan = lspan->next;
  return_unless (lspan, 0);
  // figure largest consecutive available block
  return lspan->mmsize - lspan->offset;
}

size_t
BumpAllocator::grow_spans (size_t needed, bool preallocating)
{
  const size_t entry_total = totalmem_;
  std::lock_guard<std::mutex> locker (mutex_);
  if (entry_total < totalmem_)
    return 0; // another thread grew the spans meanwhile
  if (!preallocating)
    warning ("BumpAllocator: growing from within loft_alloc (total=%u): need=%d bytes\n", totalmem_ + 0, needed);
  static const size_t PAGESIZE = sysconf (_SC_PAGESIZE);
  size_t totalmem = 0;
  // try growing the last span
  MMSpan *lspan = spans_;
  while (lspan && lspan->next)
    {
      totalmem += lspan->mmsize;
      lspan = lspan->next;
    }
  if (lspan)
    {
      const size_t oldsize = lspan->mmsize;
      size_t newsize = oldsize * 2;
      while (newsize < needed)
        newsize *= 2;
      char *mmstart = remap (lspan->mmstart, oldsize, newsize);
      if (mmstart == lspan->mmstart)
        {
          if (config_flags & PREFAULT_PAGES)
            for (size_t i = oldsize; i < newsize; i += PAGESIZE)
              mmstart[i] = 1;       // prefault pages
          lspan->mmsize = newsize;
          totalmem_ = totalmem + newsize;
          VDEBUG ("%s:%u: new-total=%dM: true\n", __func__, __LINE__, totalmem_ / (1024 * 1024));
          return newsize - oldsize;
        }
    }
  // allocate new span
  size_t mmsize = MINIMUM_HUGEPAGE;
  while (mmsize < needed)
    mmsize *= 2;
  mmsize = MEM_ALIGN (mmsize, MINIMUM_HUGEPAGE);
  char *mmstart = memmap (mmsize, !lspan);
  if (!mmstart)
    {
      VDEBUG ("%s:%u: false\n", __func__, __LINE__);
      return 0;
    }
  for (size_t i = 0; i < mmsize; i += PAGESIZE)
    mmstart[i] = 1;               // prefault pages
  MMSpan *nspan = new (mmstart) MMSpan();       // TODO: move MMSpan to malloc
  nspan->offset = MEM_ALIGN (sizeof (MMSpan), 64);
  nspan->mmstart = mmstart;
  nspan->mmsize = mmsize;
  if (lspan)
    lspan->next = nspan;
  else
    spans_ = nspan;
  totalmem_ = totalmem + nspan->mmsize;
  VDEBUG ("%s:%u: new-total=%dM: true\n", __func__, __LINE__, totalmem_ / (1024 * 1024));
  return mmsize;
}

char*
BumpAllocator::remap (void *addr, size_t oldsize, size_t size)
{
  assert_return (MEM_ALIGN (size, MINIMUM_HUGEPAGE) == size, nullptr);

  // remap, preserve start address
  char *memory = (char*) mremap (addr, oldsize, size, /*flags*/ 0, addr);
  if (memory == MAP_FAILED)
    {
      VDEBUG ("%s:%u: mremap: %p %d %d: %s\n", __func__, __LINE__, addr, oldsize, size, strerror (errno));
      return nullptr;
    }
  VDEBUG ("%s:%u: mremap: %p %d %d: %p\n", __func__, __LINE__, addr, oldsize, size, memory);
  if (madvise (memory, size, MADV_HUGEPAGE) < 0)
    VDEBUG ("%s: madvise(%p,%u,MADV_HUGEPAGE) failed: %s\n", __func__, memory, size, strerror (errno));
  return memory;
}

char*
BumpAllocator::memmap (size_t size, bool growsup)
{
  assert_return (MEM_ALIGN (size, MINIMUM_HUGEPAGE) == size, nullptr);
  const int PROTECTION = PROT_READ | PROT_WRITE;
  const int PRIVANON = MAP_PRIVATE | MAP_ANONYMOUS;

  const bool anon_hugepages = true;
  const bool reserved_hugepages = false; // prevents growth

  // probe for an address with some headroom for growth
  void *addr = nullptr;
  const size_t g1 = 1024 * 1024 * 1024;
  const size_t gigabytes = sizeof (size_t) <= 4 ? 3 * g1 : 256 * g1;
  for (size_t giga = gigabytes; giga >= 16 * 1024 * 1024; giga = giga >> 1)
    if ((addr = mmap (nullptr, giga, 0 * PROTECTION, PRIVANON, -1, 0)) != MAP_FAILED)
      {
        VDEBUG ("%s:%d: addr-hint size=%uMB: %p\n", __FILE__, __LINE__, giga / (1024 * 1024), addr);
        munmap (addr, giga);
        break;
      }
  if (addr == MAP_FAILED)
    addr = nullptr;

  // try reserved hugepages for large allocations
  char *memory;
  if (reserved_hugepages)
    {
      memory = (char*) mmap (addr, size, PROTECTION, PRIVANON | MAP_HUGETLB, -1, 0);
      if (memory != MAP_FAILED)
        {
          VDEBUG ("%s:%u: mmap: %p %d HUGETLB: %p\n", __func__, __LINE__, addr, size, memory);
          return memory;
        }
      VDEBUG ("%s:%u: mmap: %p %d HUGETLB: %s\n", __func__, __LINE__, addr, size, strerror (errno));
    }

  // mmap without HUGETLB at first, then try MADV_HUGEPAGE
  size_t areasize = size + MINIMUM_HUGEPAGE;
  memory = (char*) mmap (addr, areasize, PROTECTION, PRIVANON, -1, 0);
  if (memory == MAP_FAILED)
    {
      VDEBUG ("%s:%u: mmap: %p %d: %s\n", __func__, __LINE__, addr, areasize, strerror (errno));
      return nullptr;
    }
  VDEBUG ("%s:%u: mmap: %p %d: %p\n", __func__, __LINE__, addr, areasize, memory);
  // discard unaligned head
  const uintptr_t start = uintptr_t (memory);
  size_t extra = MEM_ALIGN (start, MINIMUM_HUGEPAGE) - start;
  if (extra && munmap (memory, extra) != 0)
    VDEBUG ("%s: munmap(%p,%u) failed: %s\n", __func__, memory, extra, strerror (errno));
  memory += extra;
  areasize -= extra;
  // discard unaligned tail
  extra = areasize - size;
  areasize -= extra;
  if (extra && munmap (memory + areasize, extra) != 0)
    VDEBUG ("%s: munmap(%p,%u) failed: %s\n", __func__, memory + areasize, extra, strerror (errno));
  assert_warn (areasize == size);
  VDEBUG ("%s:%u: mmap page-align: size=%d at: %p\n", __func__, __LINE__, size, memory);

  // linux/Documentation/admin-guide/mm/transhuge.rst
  if (anon_hugepages && madvise (memory, size, MADV_HUGEPAGE) < 0)
    VDEBUG ("%s: madvise(%p,%u,MADV_HUGEPAGE) failed: %s\n", __func__, memory, size, strerror (errno));
  return memory;
}

void*
BumpAllocator::bump_alloc (size_t size)
{
  assert_return (size && !(size & (64-1)), nullptr);
  for (MMSpan *span = spans_; span; span = span->next)
    {
      uintptr_t nmark, omark = span->offset;
      do // Note, `compare_exchange_weak == 0` does `omark := offset`
        {
          nmark = omark + size;
          if (nmark > span->mmsize)
            goto next_span;
        }
      while (!span->offset.compare_exchange_weak (omark, nmark));
      // check for watermark notifications
      if (!span->next &&                // only check watermark against last span
          span->mmsize - span->offset < config_watermark)
        {
          // block further notifications *before* calling
          if (0 == config_lowmem_notified++)
            {
              std::function<void()> *lowmem_cb = config_lowmem_cb;
              if (lowmem_cb)                // copy pointer to avoid calling null
                (*lowmem_cb) ();
            }
        }
      return span->mmstart + omark;
    next_span: ;
    }
  // no span could satisfy the request
  grow_spans (size, false);
  // retry
  return bump_alloc (size);
}

// == LoftBuckets ==
static constexpr unsigned SMALL_BLOCK_LIMIT = 8192;     // use 64 byte stepping for blocks up to this size
static constexpr unsigned SMALL_BLOCK_BUCKETS = SMALL_BLOCK_LIMIT / 64;
static constexpr unsigned NUMBER_OF_POWER2_BUCKETS = sizeof (uintptr_t) * 8;
static constexpr unsigned NUMBER_OF_BUCKETS = NUMBER_OF_POWER2_BUCKETS + SMALL_BLOCK_BUCKETS;

// Get size of slices in bytes handled by this bucket
static inline size_t
bucket_size (unsigned index)
{
  if (index < NUMBER_OF_POWER2_BUCKETS)
    return 1 << index;
  return (index - NUMBER_OF_POWER2_BUCKETS + 1) * 64;
}

// Get bucket index for a particular memory slice size
static inline unsigned
bucket_index (unsigned long long n)
{
  n += 0 == n;                                          // treat 0 like a 1
  if (n <= SMALL_BLOCK_LIMIT)
    return NUMBER_OF_POWER2_BUCKETS + (n - 1) / 64;
  constexpr unsigned CLZLL_BITS = sizeof (0ull) * 8;    // __builtin_clzll (0) is undefined
  const unsigned upper_power2_shift = CLZLL_BITS - __builtin_clzll (n - 1);
  return upper_power2_shift;
}

/** Lock-free, obstruction-free, non-coalescing, alloc-only bucket allocator
 *
 * Loft is a simple lock-free bucket allocator that maintains free-lists without
 * coalescing blocks or releasing memory back to the underlying bump allocator.
 * The free-list buckets are 64 bytes apart and all allocations are 64 byte aligned
 * to avoid false sharing of cache lines.
 * Allocation requests are satisfied with O(1) complexity using lock-free code paths.
 * Because memory is not released back, no block coalescing (or splitting) is implemented.
 */
struct LoftBuckets {
  explicit      LoftBuckets (BumpAllocator &bumpallocator) : bump_allocator (bumpallocator) {}
  LoftPtr<void> do_alloc    (size_t size, size_t align);
  void          do_free     (void *mem, size_t size);
  size_t        count       (unsigned bucket_index, const ArenaList &arenas);
private:
  struct Block {
    uintptr_t canary0 = CANARY0;
    boost::atomic<Block*> intr_ptr_ = nullptr; // atomic intrusive pointer
  };
  static_assert (sizeof (Block) <= 64);
  std::array<MpmcStack<Block>,NUMBER_OF_BUCKETS> buckets_ alignas (64);
  enum : uintptr_t {
    CANARY0 = 0xbe4da62f087c3519ull
  };
  bool          maybeok     (const Block *block, const ArenaList &arenas);
public:
  BumpAllocator &bump_allocator;
};

LoftPtr<void>
LoftBuckets::do_alloc (size_t size, size_t align)
{
  if (align > 64)
    return nullptr;                             // alignment not supported
  size += 0 == size;                            // treat 0 like a 1, malloc always yields a new pointer
  const unsigned bindex = bucket_index (size);
  if (bindex >= NUMBER_OF_BUCKETS)
    return nullptr;                             // size not supported
  const size_t bsize = bucket_size (bindex);
  auto block = buckets_[bindex].pop();
  if (block)
    assert_warn (block->canary0 == CANARY0);    // simple overwrite check
  else
    block = (Block*) bump_allocator.bump_alloc (bsize);
  block->canary0 = 0;
  void *mem = block;
  return LoftPtr<void> (mem, { bsize });
}

void
LoftBuckets::do_free (void *mem, size_t size)
{
  assert_return (mem);
  const unsigned bindex = bucket_index (size);
  assert_return (bindex < NUMBER_OF_BUCKETS);
  const size_t bsize = bucket_size (bindex);
  assert_return (bsize == size);
  auto block = new (mem) Block();
  buckets_[bindex].push (block);
}

bool
LoftBuckets::maybeok (const Block *block, const ArenaList &arenas)
{
  // test if block meets 64 byte alignment constraint
  const uintptr_t addr = uintptr_t (block);
  if (addr & 63)
    return false;
  // test if block is within known arena
  bool contained = false;
  for (const auto &a : arenas)
    if (addr >= a.addr && addr < a.addr + a.size)
      {
        contained = true;
        break;
      }
  // if the block pointer may be dereferenced, test if it is likely free
  if (contained && // may dereference
      block->canary0 == CANARY0)
    return true;
  return false;
}

size_t
LoftBuckets::count (unsigned bucket_index, const ArenaList &arenas)
{
  assert_return (bucket_index < NUMBER_OF_BUCKETS, 0);
  size_t counter = 0;
  // count buckets as long as they are *likely* within arenas
  for (Block *block = buckets_[bucket_index].peek(); block; block = block->intr_ptr_.load())
    if (maybeok (block, arenas))
      counter++;
    else
      break; // may not dereference ->next
  return counter;
}

} // Ase::Loft

// == Ase API ==
namespace Ase {
using namespace Loft;

static inline bool
no_allocators()
{
  static std::atomic<int> no_allocators_ = -1;
  if (ASE_UNLIKELY (no_allocators_ == -1))
    {
      const char *f = getenv ("ASE_DEBUG");
      no_allocators_ = f && (strstr (f, ":no-allocators") || strstr (f, "no-allocators") == f);
    }
  return no_allocators_;
}

static LoftBuckets&
the_pool()
{
  static LoftBuckets &loft_buckets = *([] () {
    BumpAllocator *bump_allocator = new (aligned_alloc (64, MEM_ALIGN (sizeof (BumpAllocator), 64))) BumpAllocator();
    LoftBuckets *loft_buckets = new (aligned_alloc (64, MEM_ALIGN (sizeof (LoftBuckets), 64))) LoftBuckets (*bump_allocator);
    return loft_buckets;
  }) ();
  return loft_buckets;
}

LoftPtr<void>
loft_alloc (size_t size, size_t align)
{
  if (ASE_UNLIKELY (no_allocators()))
    return { aligned_alloc (align, size), { size } };

  LoftBuckets &pool = the_pool();
  LoftPtr<void> ptr = pool.do_alloc (size, align);
  return ptr;
}

LoftPtr<void>
loft_calloc (size_t nelem, size_t elemsize, size_t align)
{
  const bool overflows = elemsize > 1 && nelem > ~size_t (0) / elemsize;
  if (overflows)
    return {};
  const size_t size = nelem * elemsize;
  if (ASE_UNLIKELY (no_allocators()))
    {
      void *p = aligned_alloc (align, size);
      if (p)
        wmemset ((wchar_t*) p, 0, size / sizeof (wchar_t));
      return { p, { size } };
    }

  LoftBuckets &pool = the_pool();
  LoftPtr<void> ptr = pool.do_alloc (size, align);
  wmemset ((wchar_t*) ptr.get(), 0, size / sizeof (wchar_t));
  return ptr;
}

size_t
loft_bucket_size (size_t nbytes)
{
  if (ASE_UNLIKELY (no_allocators()))
    return nbytes;
  const unsigned bindex = bucket_index (nbytes);
  if (bindex >= NUMBER_OF_BUCKETS)
    return 0;                           // size not supported
  const size_t bsize = bucket_size (bindex);
  return bsize;
}

void
LoftFree::operator() (void *p)
{
  if (ASE_UNLIKELY (no_allocators()))
    return free (p);

  LoftBuckets &pool = the_pool();
  pool.do_free (p, size);
}

void
loft_set_notifier (const std::function<void()> &lowmem)
{
  assert_return (config_lowmem_cb == nullptr);
  config_lowmem_cb = new std::function<void()> (lowmem);
  // can be installed only once
}

void
loft_set_config (const LoftConfig &config)
{
  // disable watermark to avoid spurious notifications during config updates
  config_watermark = 0;
  config_flags = config.flags;
  config_preallocate = config.preallocate;
  // reconfigure watermark
  config_watermark = config.watermark;
}

void
loft_get_config (LoftConfig &config)
{
  config.flags = config_flags;
  config.preallocate = config_preallocate;
  config.watermark = config_watermark;
}

size_t
loft_grow_preallocate (size_t preallocation_amount)
{
  LoftBuckets &pool = the_pool();
  BumpAllocator &bump_allocator = pool.bump_allocator;
  const size_t totalmem = bump_allocator.totalmem();
  // grow at least until config_preallocate
  preallocation_amount = std::max (0 + config_preallocate, totalmem) - totalmem;
  // grow at least to avoid watermark underrun
  const size_t maxchunk = bump_allocator.free_block();
  if (maxchunk <= config_watermark)
    preallocation_amount = std::max (preallocation_amount, 0 + config_watermark);
  // grow only if available memory is lower than requested
  if (maxchunk < preallocation_amount)
    {
      config_lowmem_notified = 0;
      // blocking call
      const size_t allocated = bump_allocator.grow_spans (preallocation_amount, true);
      config_preallocate = std::max (0 + config_preallocate, bump_allocator.totalmem());
      return allocated;
    }
  return 0;
}

void
loft_get_stats (LoftStats &stats)
{
  ArenaList arenas;
  LoftBuckets &pool = the_pool();
  BumpAllocator &bump_allocator = pool.bump_allocator;
  bump_allocator.list_arenas (arenas);
  stats.narenas = arenas.size();
  stats.allocated = 0;
  stats.available = 0;
  stats.maxchunk = 0;
  for (const auto &a : arenas)
    {
      stats.allocated += a.size;
      stats.available += a.size - a.offset;
      stats.maxchunk = std::max (stats.maxchunk, a.size - a.offset);
    }
  stats.buckets.clear();
  for (size_t i = 0; i < NUMBER_OF_BUCKETS; i++)
    {
      size_t count = pool.count (i, arenas);
      if (count)
        stats.buckets.push_back (std::make_pair (bucket_size (i), count));
    }
  std::sort (stats.buckets.begin(), stats.buckets.end(), [] (auto &a, auto &b) {
    return a.first < b.first;
  });
}

String
loft_stats_string (const LoftStats &stats)
{
  StringS s;
  constexpr size_t MB = 1024 * 1024;
  s.push_back (string_format ("%8u Arenas", stats.narenas));
  s.push_back (string_format ("%8u MB allocated", stats.allocated / MB));
  s.push_back (string_format ("%8u MB available", stats.available / MB));
  s.push_back (string_format ("%8u KB maximum chunk", stats.maxchunk / 1024));
  for (const auto &b : stats.buckets)
    if (b.second && 0 == (b.first & 1023))
      s.push_back (string_format ("%8u x %4u KB", b.second, b.first / 1024));
    else if (b.second)
      s.push_back (string_format ("%8u x %4u B", b.second, b.first));
  size_t t = 0;
  for (const auto &b : stats.buckets)
    t += b.first * b.second;
  s.push_back (string_format ("%8.1f MB in use", t * 1.0 / MB));
  return string_join ("\n", s);
}

} // Ase


// == tests ==
namespace {
using namespace Ase;

static const size_t N_THREADS = std::thread::hardware_concurrency();

TEST_INTEGRITY (loft_simple_tests);
static void
loft_simple_tests()
{
  using namespace Ase::Loft;
  unsigned bi, bs;
  bi = bucket_index (1);     bs = bucket_size (bi);     TCMP (bs, ==, 64u);  TASSERT (bi == NUMBER_OF_POWER2_BUCKETS);
  TASSERT (bi == bucket_index (0));
  bi = bucket_index (63);    bs = bucket_size (bi);     TCMP (bs, ==, 64u);  TASSERT (bi == NUMBER_OF_POWER2_BUCKETS);
  bi = bucket_index (64);    bs = bucket_size (bi);     TCMP (bs, ==, 64u);  TASSERT (bi == NUMBER_OF_POWER2_BUCKETS);
  bi = bucket_index (65);    bs = bucket_size (bi);     TCMP (bs, ==, 128u); TASSERT (bi == NUMBER_OF_POWER2_BUCKETS + 1);
  bi = bucket_index (127);   bs = bucket_size (bi);     TCMP (bs, ==, 128u); TASSERT (bi == NUMBER_OF_POWER2_BUCKETS + 1);
  bi = bucket_index (128);   bs = bucket_size (bi);     TCMP (bs, ==, 128u); TASSERT (bi == NUMBER_OF_POWER2_BUCKETS + 1);
  bi = bucket_index (129);   bs = bucket_size (bi);     TCMP (bs, ==, 192u); TASSERT (bi == NUMBER_OF_POWER2_BUCKETS + 2);
  bi = bucket_index (SMALL_BLOCK_LIMIT - 1); bs = bucket_size (bi);     TCMP (bs, ==, SMALL_BLOCK_LIMIT);
  bi = bucket_index (SMALL_BLOCK_LIMIT);     bs = bucket_size (bi);     TCMP (bs, ==, SMALL_BLOCK_LIMIT);
  TASSERT (bi + 1 == NUMBER_OF_BUCKETS);
  bi = bucket_index (SMALL_BLOCK_LIMIT + 1); bs = bucket_size (bi);     TCMP (bs, ==, SMALL_BLOCK_LIMIT * 2);
  bi = bucket_index (SMALL_BLOCK_LIMIT * 2); bs = bucket_size (bi);     TCMP (bs, ==, SMALL_BLOCK_LIMIT * 2);
  bi = bucket_index (SMALL_BLOCK_LIMIT * 2 + 1); bs = bucket_size (bi); TCMP (bs, ==, SMALL_BLOCK_LIMIT * 2 * 2);
  auto sp = loft_make_unique<String> ("Sample String");
  TASSERT ("Sample String" == *sp);
}

inline constexpr size_t N_ALLOCS = 20000;
inline constexpr size_t MAX_BLOCK_SIZE = 4 * 1024;

static std::atomic<bool> thread_shuffle_done = false;

struct ThreadData
{
  unsigned                    thread_id;
  std::thread                 thread;
  std::unique_ptr<std::mutex> to_free_mutex;
  std::vector<LoftPtr<void>>  to_free;           // [N_THREADS * N_ALLOCS]
  int                         n_to_free = 0;
  int                         n_freed = 0;
};
static std::vector<ThreadData> tdata;   // [N_THREADS]

static void
thread_shuffle_allocs (ThreadData *td)
{
  FastRng rng;
  uint64_t lastval = -1;
  for (size_t i = 0; i < N_ALLOCS; i++)
    {
      if (Test::verbose() && i % (N_ALLOCS / 20) == 0)
        printout ("%c", '@' + td->thread_id);

      // allocate block of random size and fill randomly
      size_t bytes = rng.next() % MAX_BLOCK_SIZE + 8;
      LoftPtr<void> mem = loft_alloc (bytes);
      char *cmem = (char*) mem.get();
      for (size_t f = 0; f < bytes; f++)
        cmem[f] = rng.next();

      // check that blocks are indeed randomly filled
      TASSERT (lastval != *(uint64_t*) &cmem[bytes - 8]); // last entry should differ from last block
      TASSERT (lastval != *(uint64_t*) &cmem[0]);         // first entry is usually touched by allocator
      lastval = *(uint64_t*) &cmem[bytes - 8];

      // associate block with random thread
      const size_t t = rng.next() % N_THREADS;
      {
        std::lock_guard<std::mutex> locker (*tdata[t].to_free_mutex);
        tdata[t].to_free[tdata[t].n_to_free] = std::move (mem);
        tdata[t].n_to_free++;
      }

      // shuffle thread execution order every once in a while
      if (rng.next() % 97 == 0)
        usleep (500);           // concurrent shuffling for multi core

      // free a block associated with this thread
      LoftPtr<void> ptr;
      std::lock_guard<std::mutex> locker (*td->to_free_mutex);
      if (td->n_to_free > 0)
        {
          td->n_to_free--;
          ptr = std::move (td->to_free[td->n_to_free]);
          td->n_freed++;
        }
      // ptr is freed here
    }
  thread_shuffle_done = true;
}

TEST_INTEGRITY (loft_shuffle_thread_allocs);
static void
loft_shuffle_thread_allocs()
{
  thread_shuffle_done = false;
  // allocate test data
  tdata.resize (N_THREADS);
  for (auto &td : tdata)
    {
      td.to_free_mutex = std::unique_ptr<std::mutex> (new std::mutex);
      td.to_free.resize (N_THREADS * N_ALLOCS);
    }
  // start one thread per core
  if (Test::verbose())
    printout ("Starting %d threads for concurrent Loft usage...\n", N_THREADS);
  for (size_t t = 0; t < N_THREADS; t++)
    {
      tdata[t].thread_id = 1 + t;
      tdata[t].thread = std::thread (thread_shuffle_allocs, &tdata[t]);
    }
  // effectively assigns a timeout for main_loop->iterate()
  main_loop->exec_timer ([] () { return !thread_shuffle_done; }, 50);
  // recurse into main loop for concurrent preallocations
  while (!thread_shuffle_done)  // wati until *any* thread is done
    main_loop->iterate (true);  // only allowed in unit tests
  // when done, join threads
  for (size_t t = 0; t < N_THREADS; t++)
    tdata[t].thread.join();
  // print stats
  if (Test::verbose())
    printout ("\n");
  if (Test::verbose())
    for (size_t t = 0; t < N_THREADS; t++)
      printout ("Thread %c: %d blocks freed, %d blocks not freed\n",
                '@' + tdata[t].thread_id, tdata[t].n_freed, tdata[t].n_to_free);
  tdata.clear();

  LoftStats lstats;
  loft_get_stats (lstats);
  if (Test::verbose())
    printout ("Loft Stats:\n%s\n", loft_stats_string (lstats));
}

} // Anon
