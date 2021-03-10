// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_UTILS_HH__
#define __ASE_UTILS_HH__

#include <ase/cxxaux.hh>
#include <ase/strings.hh>

namespace Ase {

// == Debugging ==
inline bool               debug_enabled     () ASE_ALWAYS_INLINE ASE_PURE;
bool                      debug_key_enabled (const char *conditional) ASE_PURE;
bool                      debug_key_enabled (const std::string &conditional) ASE_PURE;
std::string               debug_key_value   (const char *conditional) ASE_PURE;
template<class ...A> void debug             (const char *cond, const char *format, const A &...args) ASE_ALWAYS_INLINE;
template<class ...A> void fatal_error       (const char *format, const A &...args) ASE_NORETURN;
template<class ...A> void warning           (const char *format, const A &...args);
template<class... A> void printout          (const char *format, const A &...args) ASE_PRINTF (1, 0);
template<class... A> void printerr          (const char *format, const A &...args) ASE_PRINTF (1, 0);

const char*                      ase_gettext (const String &untranslated);
template<class... A> const char* ase_gettext (const char *format, const A &...args) ASE_PRINTF (1, 0);

// == Jump Tables ==
/// Create a `std::array<Fun,N>`, where `Fun` is returned from `mkjump (INDICES…)`.
template<typename MkFun, size_t ...INDICES> static auto
make_indexed_table (const MkFun &mkjump, std::index_sequence<INDICES...>)
{
  constexpr size_t N = sizeof... (INDICES);
  using Fun = decltype (mkjump (std::integral_constant<std::size_t, N - 1>()));
  const std::array<Fun, N> jumptable = {
    mkjump (std::integral_constant<std::size_t, INDICES>{})...
  };
  return jumptable;
}

/// Create a jump table `std::array<Fun,LAST>`, where `Fun` is returned from `mkjump (0 … LAST)`.
/// Note, `mkjump(auto)` is a lambda template, invoked with `std::integral_constant<unsigned long, 0…LAST>`.
template<std::size_t LAST, typename MkFun> static auto
make_case_table (const MkFun &mkjump)
{
  return make_indexed_table (mkjump, std::make_index_sequence<LAST + 1>());
}

// == EventFd ==
/// Wakeup facility for IPC.
class EventFd
{
  int      fds[2];
  void     operator= (const EventFd&) = delete; // no assignments
  explicit EventFd   (const EventFd&) = delete; // no copying
public:
  explicit EventFd   ();
  int      open      (); ///< Opens the eventfd and returns -errno.
  bool     opened    (); ///< Indicates whether eventfd has been opened.
  void     wakeup    (); ///< Wakeup polling end.
  int      inputfd   (); ///< Returns the file descriptor for POLLIN.
  bool     pollin    (); ///< Checks whether events are pending.
  void     flush     (); ///< Clear pending wakeups.
  /*Des*/ ~EventFd   ();
};

// == JobQueue for cross-thread invocations ==
struct JobQueue {
  enum Policy { SYNC, };
  using Caller = std::function<void (Policy, const std::function<void()>&)>;
  explicit             JobQueue    (const Caller &caller);
  template<typename F> std::invoke_result_t<F>
  inline               operator+=  (const F &job);
private:
  const Caller caller_;
};

// == Implementation Details ==
template<typename F> std::invoke_result_t<F>
JobQueue::operator+= (const F &job)
{
  using R = std::invoke_result_t<F>;
  if constexpr (std::is_same<R, void>::value)
    caller_ (SYNC, job);
  else // R != void
    {
      R result;
      call_remote ([&job, &result] () {
        result = job();
      });
      return result;
    }
}

void debug_message (const char *cond, const std::string &message);
void diag_message  (uint8 code, const std::string &message);

/// Global boolean to reduce debugging penalty where possible
extern bool ase_debugging_enabled;

/// Check if any kind of debugging is enabled by $ASE_DEBUG.
inline bool ASE_ALWAYS_INLINE ASE_PURE
debug_enabled()
{
  return ASE_UNLIKELY (ase_debugging_enabled);
}

/// Issue a printf-like debugging message if `cond` is enabled by $ASE_DEBUG.
template<class ...Args> inline void ASE_ALWAYS_INLINE
debug (const char *cond, const char *format, const Args &...args)
{
  if (debug_enabled())
    {
      if (ASE_UNLIKELY (debug_key_enabled (cond)))
        debug_message (cond, string_format (format, args...));
    }
}

/** Issue a printf-like message and abort the program, this function will not return.
 * Avoid using this in library code, aborting may take precious user data with it,
 * library code should instead use warning(), info() or assert_return().
 */
template<class ...Args> void ASE_NORETURN
fatal_error (const char *format, const Args &...args)
{
  diag_message ('F', string_format (format, args...));
  while (1);
}

/// Issue a printf-like warning message.
template<class ...Args> void
warning (const char *format, const Args &...args)
{
  diag_message ('W', string_format (format, args...));
}

/// Print a message on stdout (and flush stdout) ala printf(), using the POSIX/C locale.
template<class... Args> void
printout (const char *format, const Args &...args)
{
  diag_message ('o', string_format (format, args...));
}

/// Print a message on stderr (and flush stderr) ala printf(), using the POSIX/C locale.
template<class... Args> void
printerr (const char *format, const Args &...args)
{
  diag_message ('e', string_format (format, args...));
}

/// Translate a string, using the ASE locale.
template<class... Args> const char*
ase_gettext (const char *format, const Args &...args)
{
  return ase_gettext (string_format (format, args...));
}

/// Auxillary algorithms brodly useful
namespace Aux {

/// Erase first element for which `pred()` is true in vector or list.
template<class C> inline size_t
erase_first (C &container, const std::function<bool (typename C::value_type const &value)> &pred)
{
  for (auto iter = container.begin(); iter != container.end(); iter++)
    if (pred (*iter))
      {
        container.erase (iter);
        return 1;
      }
  return 0;
}

/// Erase all elements for which `pred()` is true in vector or list.
template<class C> inline size_t
erase_all (C &container, const std::function<bool (typename C::value_type const &value)> &pred)
{
  size_t c = 0;
  for (auto iter = container.begin(); iter != container.end(); /**/)
    if (pred (*iter))
      {
        iter = container.erase (iter);
        c++;
      }
    else
      iter++;
  return c;
}

/// Returns `true` if container element for which `pred()` is true.
template<typename C> inline bool
contains (const C &container, const std::function<bool (typename C::value_type const &value)> &pred)
{
  for (auto iter = container.begin(); iter != container.end(); iter++)
    if (pred (*iter))
      return true;
  return false;
}

// == Binary Lookups ==
template<typename RandIter, class Cmp, typename Arg, int case_lookup_or_sibling_or_insertion>
extern inline std::pair<RandIter,bool>
binary_lookup_fuzzy (RandIter begin, RandIter end, Cmp cmp_elements, const Arg &arg)
{
  RandIter current = end;
  size_t n_elements = end - begin, offs = 0;
  const bool want_lookup = case_lookup_or_sibling_or_insertion == 0;
  // const bool want_sibling = case_lookup_or_sibling_or_insertion == 1;
  const bool want_insertion_pos = case_lookup_or_sibling_or_insertion > 1;
  ssize_t cmp = 0;
  while (offs < n_elements)
    {
      size_t i = (offs + n_elements) >> 1;
      current = begin + i;
      cmp = cmp_elements (arg, *current);
      if (cmp == 0)
        return want_insertion_pos ? std::make_pair (current, true) : std::make_pair (current, /*ignored*/ false);
      else if (cmp < 0)
        n_elements = i;
      else /* (cmp > 0) */
        offs = i + 1;
    }
  /* check is last mismatch, cmp > 0 indicates greater key */
  return (want_lookup
          ? std::make_pair (end, /*ignored*/ false)
          : (want_insertion_pos && cmp > 0)
          ? std::make_pair (current + 1, false)
          : std::make_pair (current, false));
}

/** Perform a binary lookup to find the insertion position for a new element.
 * Return (end,false) for end-begin==0, or return (position,true) for exact match,
 * otherwise return (position,false) where position indicates the location for
 * the key to be inserted (and may equal end).
 */
template<typename RandIter, class Cmp, typename Arg>
extern inline std::pair<RandIter,bool>
binary_lookup_insertion_pos (RandIter begin, RandIter end, Cmp cmp_elements, const Arg &arg)
{
  return binary_lookup_fuzzy<RandIter,Cmp,Arg,2> (begin, end, cmp_elements, arg);
}

/** Perform a binary lookup to yield exact or nearest match.
 * return end for end-begin==0, otherwise return the exact match element, or,
 * if there's no such element, return the element last visited, which is pretty
 * close to an exact match (will be one off into either direction).
 */
template<typename RandIter, class Cmp, typename Arg>
extern inline RandIter
binary_lookup_sibling (RandIter begin, RandIter end, Cmp cmp_elements, const Arg &arg)
{
  return binary_lookup_fuzzy<RandIter,Cmp,Arg,1> (begin, end, cmp_elements, arg).first;
}

/** Perform binary lookup and yield exact match or @a end.
 * The arguments [ @a begin, @a end [ denote the range used for the lookup,
 * @a arg is passed along with the current element to the @a cmp_elements
 * function.
 */
template<typename RandIter, class Cmp, typename Arg>
extern inline RandIter
binary_lookup (RandIter begin, RandIter end, Cmp cmp_elements, const Arg &arg)
{
  /* return end or exact match */
  return binary_lookup_fuzzy<RandIter,Cmp,Arg,0> (begin, end, cmp_elements, arg).first;
}

/// Comparison function useful to sort lesser items first.
template<typename Value> static inline int
compare_lesser (const Value &v1, const Value &v2)
{
  return -(v1 < v2) | (v2 < v1);
}

/// Comparison function useful to sort greater items first.
template<typename Value> static inline int
compare_greater (const Value &v1, const Value &v2)
{
  return (v1 < v2) | -(v2 < v1);
}

} // Aux

} // Ase

#endif // __ASE_UTILS_HH__
