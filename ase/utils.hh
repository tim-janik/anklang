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
/// Create a `std::array<F,N>`, where `F` is returned from `mkjump (INDICES...)`.
template<typename J, size_t ...INDICES> static auto
make_jump_table_indexed (const J &mkjump, std::index_sequence<INDICES...>)
{
  constexpr size_t N = sizeof... (INDICES);
  using F = decltype (mkjump (std::integral_constant<std::size_t, N-1>()));
  std::array<F, N> jumptable = {
    mkjump (std::integral_constant<std::size_t, INDICES>{})...
  };
  return jumptable;
}

/// Create a `std::array<F,N>`, where `F` is returned from `mkjump (0 .. N-1)`.
template<std::size_t N, typename J> static auto
make_jump_table (const J &mkjump)
{
  return make_jump_table_indexed (mkjump, std::make_index_sequence<N>());
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

} // Aux

} // Ase

#endif // __ASE_UTILS_HH__
