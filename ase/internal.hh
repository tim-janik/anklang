// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __ASE_INTERNAL_HH__
#define __ASE_INTERNAL_HH__

// Import simple ASE types into global scope
using Ase::uint8;
using Ase::uint16;
using Ase::uint32;
using Ase::uint64;
using Ase::int8;
using Ase::int16;
using Ase::int32;
using Ase::int64;
using Ase::unichar;
using Ase::String;

/// Retrieve the translation of a C or C++ string.
#define _(...)          ::Ase::_ (__VA_ARGS__)
/// Mark a string for translation, passed through verbatim by the preprocessor.
#define N_(str)         (str)

/// Constrain, apply, notify and implement a property change, the property name must equal `__func__`.
#define APPLY_IDL_PROPERTY(lvalue, rvalue)      ASE_OBJECT_APPLY_IDL_PROPERTY(lvalue, rvalue)

/// Yield the number of C @a array elements.
#define ARRAY_SIZE(array)               ASE_ARRAY_SIZE (array)

/// Return from the current function if `expr` is unmet and issue an assertion warning.
#define assert_return(expr, ...)        ASE_ASSERT_RETURN (expr, __VA_ARGS__)
/// Return from the current function and issue an assertion warning.
#define assert_return_unreached(...)    ASE_ASSERT_RETURN_UNREACHED (__VA_ARGS__)
/// Issue an assertion warning if `expr` evaluates to false.
#define assert_warn(expr)               ASE_ASSERT_WARN (expr)

/// Indentation helper for editors that cannot (yet) decipher `if constexpr`
#define	if_constexpr	if constexpr

/// Produce a const char* string, wrapping @a str into C-style double quotes.
#define CQUOTE(str)                                     ASE_CQUOTE(str)

#ifdef __G_MACROS_H__
#undef MAX
#undef MIN
#endif

/// Yield maximum of `a` and `b`.
#define MAX(a,b)        ((a) >= (b) ? (a) : (b))

/// Yield minimum of `a` and `b`.
#define MIN(a,b)        ((a) <= (b) ? (a) : (b))

/// Hint to the compiler to optimize for @a cond == TRUE.
#define ISLIKELY(cond)  ASE_ISLIKELY (cond)
/// Hint to the compiler to optimize for @a cond == FALSE.
#define UNLIKELY(cond)  ASE_UNLIKELY (cond)

/// Return silently if @a cond does not evaluate to true with return value @a ...
#define return_unless(cond, ...)        ASE_RETURN_UNLESS (cond, __VA_ARGS__)

/// Create a Ase::StringVector, from a const char* C-style array.
#define STRING_VECTOR_FROM_ARRAY(ConstCharArray)        ASE_STRING_VECTOR_FROM_ARRAY(ConstCharArray)

/// Register `IMPL` with Jsonipc and indicate it inherits from `INTERFACE`.
#define JSONIPC_INHERIT(IMPL, INTERFACE)        \
  [[maybe_unused]] static bool ASE_CPP_PASTE2 (ase_inherit__, __COUNTER__) =   \
    ( Jsonipc::Class< IMPL >().inherit< INTERFACE >() , 0 )

/// Register `func` as an integrity test.
#define TEST_INTEGRITY(FUNC)        static void FUNC() __attribute__ ((__cold__, __unused__)); \
  static ::Ase::Test::IntegrityCheck ASE_CPP_PASTE2 (__Ase__Test__IntegrityCheck__line, __LINE__) { #FUNC, FUNC, 'I' }

/// Register `func` as a benchmark test.
#define TEST_BENCHMARK(FUNC)        static void FUNC() __attribute__ ((__cold__, __unused__)); \
  static ::Ase::Test::IntegrityCheck ASE_CPP_PASTE2 (__Ase__Test__IntegrityCheck__line, __LINE__) { #FUNC, FUNC, 'B' }

namespace Ase {

#ifdef ASE_ENABLE_DEBUG
constexpr const bool __DEV__ = true;
#else
constexpr const bool __DEV__ = false;
#endif

namespace Test {

// == IntegrityCheck ==
struct IntegrityCheck {
  using TestFunc = void (*) ();
  IntegrityCheck (const char *name, TestFunc func, char hint)
  {
    func_ = func;
    name_ = name;
    next_ = first_;
    first_ = this;
  }
  static void deferred_init(); // see testing.cc
private:
  const char *name_;
  TestFunc func_;
  IntegrityCheck *next_;
  static IntegrityCheck *first_;    // see testing.cc
};

} } // Ase::Test

#endif  // __ASE_INTERNAL_HH__
