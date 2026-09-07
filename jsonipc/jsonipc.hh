// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <stdarg.h>
#include <cxxabi.h> // abi::__cxa_demangle
#include <algorithm>
#include <functional>
#include <typeindex>
#include <memory>
#include <vector>
#include <unordered_map>
#include <variant>
#include <map>
#include <set>

// Much of the API and some implementation ideas are influenced by https://github.com/pmed/v8pp/ and https://www.jsonrpc.org/.

#define JSONIPC_ISLIKELY(expr)          __builtin_expect (bool (expr), 1)
#define JSONIPC_UNLIKELY(expr)          __builtin_expect (bool (expr), 0)
#define JSONIPC_WARNING(fmt,...)        do { fprintf (stderr, "%s:%d: warning: ", __FILE__, __LINE__); fprintf (stderr, fmt, __VA_ARGS__); fputs ("\n", stderr); } while (0)
#define JSONIPC_ASSERT_RETURN(expr,...) do { if (JSONIPC_ISLIKELY (expr)) break; fprintf (stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #expr); return __VA_ARGS__; } while (0)

namespace Jsonipc {

#ifdef  JSONIPC_CUSTOM_SHARED_BASE
using SharedBase = JSONIPC_CUSTOM_SHARED_BASE;
#else
/// Common base type for polymorphic classes managed by `std::shared_ptr<>`.
struct JsonipcSharedBase : public virtual std::enable_shared_from_this<JsonipcSharedBase> {
  virtual ~JsonipcSharedBase() {}
};
using SharedBase = JsonipcSharedBase;
#endif

// == Json types ==
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator> >;
using JsonAllocator = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>;
using StringBufferWriter = rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfNullFlag>;
static constexpr const unsigned  rapidjson_parse_flags =
  rapidjson::kParseFullPrecisionFlag |
  rapidjson::kParseCommentsFlag |
  rapidjson::kParseTrailingCommasFlag |
  rapidjson::kParseNanAndInfFlag |
  rapidjson::kParseEscapedApostropheFlag;

// == C++ Utilities ==
/// Construct a std::string with printf-like syntax, ignoring locale settings.
static inline std::string
string_format (const char *format, ...)
{
  va_list vargs;
  va_start (vargs, format);
  static locale_t posix_c_locale = newlocale (LC_ALL_MASK, "C", NULL);
  locale_t saved_locale = uselocale (posix_c_locale);
  constexpr const size_t maxlen = 8192;
  char buffer[maxlen + 1 + 1] = { 0, };
  vsnprintf (buffer, maxlen, format, vargs);
  buffer[maxlen] = 0;
  uselocale (saved_locale);
  va_end (vargs);
  return std::string (buffer);
}

/// REQUIRES<value> - Simplified version of std::enable_if<cond,bool>::type to use SFINAE in function templates.
template<bool value> using REQUIRES = typename ::std::enable_if<value, bool>::type;

/// REQUIRESv<value> - Simplified version of std::enable_if<cond,void>::type to use SFINAE in struct templates.
template<bool value> using REQUIRESv = typename ::std::enable_if<value, void>::type;

/// Template class to identify std::shared_ptr<> classes
template<typename>   struct IsSharedPtr                     : std::false_type {};
template<typename T> struct IsSharedPtr<std::shared_ptr<T>> : std::true_type  {};

/// Test string equality at compile time.
static inline constexpr bool
constexpr_equals (const char *a, const char *b, size_t n)
{
  return n == 0 || (a[0] == b[0] && (a[0] == 0 || constexpr_equals (a + 1, b + 1, n - 1)));
}

/** Demangle a std::typeinfo.name() string into a proper C++ type name.
 * This function uses abi::__cxa_demangle() from <cxxabi.h> to demangle C++ type names,
 * which works for g++, libstdc++, clang++, libc++.
 */
static inline std::string
string_demangle_cxx (const char *mangled_identifier)
{
  int status = 0;
  char *malloced_result = abi::__cxa_demangle (mangled_identifier, NULL, NULL, &status);
  std::string result = malloced_result && !status ? malloced_result : mangled_identifier;
  if (malloced_result)
    free (malloced_result);
  return result;
}

/// Provide demangled stringified name for a type `T`.
template<class T> static inline std::string
rtti_typename()
{
  return string_demangle_cxx (typeid (T).name());
}

/// Provide demangled stringified name for the runtime type of object `o`.
template<class T> static inline std::string
rtti_typename (T &o)
{
  return string_demangle_cxx (typeid (o).name());
}

/// DerivesSharedPtr<T> - Check if `T` derives from `std::shared_ptr<>`.
template<class T, typename = void> struct DerivesSharedPtr : std::false_type {};
template<class T> struct DerivesSharedPtr<T, std::void_t< typename T::element_type > > :
    std::is_base_of< std::shared_ptr<typename T::element_type>, T > {};

/// Has_shared_from_this<T> - Check if `t.shared_from_this()` yields a `std::shared_ptr<>`.
template<class, class = void> struct Has_shared_from_this : std::false_type {};
template<typename T> struct Has_shared_from_this<T, std::void_t< decltype (std::declval<T&>().shared_from_this()) > > :
    DerivesSharedPtr< decltype (std::declval<T&>().shared_from_this()) > {};

/// Has___typename__<T> - Check if @a T provides a @a __typename__() method.
template<class, class = void> struct Has___typename__ : std::false_type {};
template<typename T>          struct Has___typename__<T, std::void_t< decltype (std::declval<const T&>().__typename__()) > > : std::true_type {};

/// Has_setget<T> - Check if type `T` provides methods `set()` and `get()`
template<class, class = void> struct Has_setget : std::false_type {};
template<typename T>          struct Has_setget<T, std::void_t< decltype (std::declval<T&>().set (std::declval<T&>().get())) > > : std::true_type {};

/// Provide the __typename__() of @a object, or its rtti_typename().
template<typename T, REQUIRES< Has___typename__<T>::value > = true> static inline std::string
get___typename__ (const T &o)
{
  return o.__typename__();
}
template<typename T, REQUIRES< !Has___typename__<T>::value > = true> static inline std::string
get___typename__ (const T &o)
{
  return rtti_typename (o);
}

/// Jsonipc exception that is relayed to caller when thrown during invocations.
struct bad_invocation : std::exception {
  const char* what           () const noexcept override { return reason_; }
  int         code           () const noexcept          { return code_; }
  explicit    bad_invocation (int code, const char *staticreason) noexcept :
    reason_ (staticreason), code_ (code) {}
private:
  const char *const reason_;
  const int   code_;
};

// == Forward Decls ==
class InstanceMap;
template<typename> struct Class;

// == Scope ==
using ScopeLocals = std::vector<std::shared_ptr<void>>;
using ScopeLocalsP = std::shared_ptr<ScopeLocals>;

/// Keep track of temporary instances during IpcDispatcher::dispatch_message().
class Scope {
  InstanceMap &instance_map_;
  ScopeLocals  scope_locals_;
  ScopeLocalsP localsp_;
  static std::vector<Scope*>&
  stack()
  {
    static thread_local std::vector<Scope*> stack_;
    return stack_;
  }
  static Scope*
  head()
  {
    auto &stack_ = stack();
    return stack_.empty() ? nullptr : stack_.back();
  }
public:
  template<typename T> static std::shared_ptr<T>
  make_shared ()
  {
    Scope *scope = head();
    if (!scope)
      throw std::logic_error ("Jsonipc::Scope::make_shared(): invalid Scope: nullptr");
    std::shared_ptr<T> sptr;
    if (scope)
      {
        sptr = std::make_shared<T>();
        scope->localsp_->push_back (sptr);
      }
    return sptr;
  }
  static InstanceMap*
  instance_map ()
  {
    Scope *scope = head();
    if (!scope)
      throw std::logic_error ("Jsonipc::Scope::instance_map(): invalid Scope: nullptr");
    return scope ? &scope->instance_map_ : nullptr;
  }
  explicit Scope (InstanceMap &instance_map, ScopeLocalsP localsp = {});
  ~Scope()
  {
    auto &stack_ = stack();
    stack_.erase (std::remove (stack_.begin(), stack_.end(), this), stack_.end());
  }
};

// == Convert ==
/// Template class providing C++ <-> JsonValue conversions for various types
template<typename T, typename Enable = void>
struct Convert;

// int types
template<typename T>
struct Convert<T, REQUIRESv< std::is_integral<T>::value > > {
  static T
  from_json (const JsonValue &value, T fallback = T())
  {
    if      (value.IsBool())    return value.GetBool();
    else if (value.IsInt())     return value.GetInt();
    else if (value.IsUint())    return value.GetUint();
    else if (value.IsInt64())   return value.GetInt64();
    else if (value.IsUint64())  return value.GetUint64();
    else if (value.IsDouble())  return value.GetDouble();
    else                        return fallback;        // !IsNumber()
  }
  static JsonValue
  to_json (T i, JsonAllocator &allocator)
  {
    return JsonValue (i);
  }
};

// bool type
template<>
struct Convert<bool> {
  static bool
  from_json (const JsonValue &value, bool fallback = bool())
  {
    return Convert<uint64_t>::from_json (value, fallback);
  }
  static JsonValue
  to_json (bool b, JsonAllocator &allocator)
  {
    return JsonValue (b);
  }
};

// floating point types
template<typename T>
struct Convert<T, REQUIRESv< std::is_floating_point<T>::value >> {
  static T
  from_json (const JsonValue &value, T fallback = T())
  {
    if      (value.IsBool())    return value.GetBool();
    else if (value.IsInt())     return value.GetInt();
    else if (value.IsUint())    return value.GetUint();
    else if (value.IsInt64())   return value.GetInt64();
    else if (value.IsUint64())  return value.GetUint64();
    else if (value.IsDouble())  return value.GetDouble();
    else                        return fallback;        // !IsNumber()
  }
  static JsonValue
  to_json (T f, JsonAllocator &allocator)
  {
    return JsonValue (f);
  }
};

// const char* type
template<>
struct Convert<const char*> {
  static const char*
  from_json (const JsonValue &value, const char *fallback = "")
  {
    return value.IsString() ? value.GetString() : fallback;
  }
  static JsonValue
  to_json (const char *str, size_t l, JsonAllocator &allocator)
  {
    return str ? JsonValue (str, l, allocator) : JsonValue();
  }
  static JsonValue
  to_json (const char *str, JsonAllocator &allocator)
  {
    return str ? JsonValue (str, strlen (str), allocator) : JsonValue();
  }
};

// std::string
template<>
struct Convert<std::string> {
  static std::string
  from_json (const JsonValue &value, const std::string &fallback = std::string())
  {
    return value.IsString() ? std::string (value.GetString(), value.GetStringLength()) : fallback;
  }
  static JsonValue
  to_json (const std::string &s, JsonAllocator &allocator)
  {
    return JsonValue (s.data(), s.size(), allocator);
  }
};

/// DerivesVector<T> - Check if `T` derives from std::vector<>.
template<class T, typename = void> struct DerivesVector : std::false_type {};
// Use void_t to prevent errors for T without vector's typedefs
template<class T> struct DerivesVector<T, std::void_t< typename T::value_type, typename T::allocator_type >> :
  std::is_base_of< std::vector<typename T::value_type, typename T::allocator_type>, T > {};

/// DerivesPair<T> - Check if `T` derives from std::pair<>.
template<class T, typename = void> struct DerivesPair : std::false_type {};
// Use void_t to prevent errors for T without pair's typedefs
template<class T> struct DerivesPair<T, std::void_t< typename T::first_type, typename T::second_type >> :
  std::is_base_of< std::pair<typename T::first_type, typename T::second_type>, T > {};

// std::vector
template<typename T>
struct Convert<T, REQUIRESv< DerivesVector<T>::value >> {
  static T
  from_json (const JsonValue &jarray)
  {
    T vec;
    if (jarray.IsArray())
      {
        vec.reserve (jarray.Size());
        for (size_t i = 0; i < jarray.Size(); i++)
          vec.emplace_back (Convert<typename T::value_type>::from_json (jarray[i]));
      }
    return vec;
  }
  static JsonValue
  to_json (const T &vec, JsonAllocator &allocator)
  {
    JsonValue jarray (rapidjson::kArrayType);
    jarray.Reserve (vec.size(), allocator);
    for (size_t i = 0; i < vec.size(); ++i)
      jarray.PushBack (Convert<typename T::value_type>::to_json (vec[i], allocator).Move(), allocator);
    return jarray;
  }
};

// std::pair
template<typename T>
struct Convert<T, REQUIRESv< DerivesPair<T>::value >> {
  static T
  from_json (const JsonValue &jarray)
  {
    T pair = {};
    if (jarray.IsArray() && jarray.Size() >= 2)
      {
        pair.first  = Convert<typename T::first_type >::from_json (jarray[0]);
        pair.second = Convert<typename T::second_type>::from_json (jarray[1]);
      }
    return pair;
  }
  static JsonValue
  to_json (const T &pair, JsonAllocator &allocator)
  {
    JsonValue jarray (rapidjson::kArrayType);
    jarray.Reserve (2, allocator);
    jarray.PushBack (Convert<typename T::first_type >::to_json (pair.first,  allocator).Move(), allocator);
    jarray.PushBack (Convert<typename T::second_type>::to_json (pair.second, allocator).Move(), allocator);
    return jarray;
  }
};

// reference types
template<typename T>
struct Convert<T&> : Convert<T> {};

// const reference types
template<typename T>
struct Convert<T const&> : Convert<T> {};

/// Convert JsonValue to C++ value
template<typename T> static inline auto
from_json (const JsonValue &value)
  -> decltype (Convert<T>::from_json (value))
{
  return Convert<T>::from_json (value);
}

/// Convert JsonValue to C++ value with fallback for failed conversions
template<typename T> static inline auto
from_json (const JsonValue &value, const T &fallback)
  -> decltype (Convert<T>::from_json (value, fallback))
{
  return Convert<T>::from_json (value, fallback);
}

/// Convert C++ value to JsonValue
template<typename T> static inline JsonValue
to_json (const T &value, JsonAllocator &allocator)
{
  return Convert<T>::to_json (value, allocator);
}

/// Convert C++ value to JsonValue
template<> inline JsonValue
to_json<const char*> (const char *const &value, JsonAllocator &allocator)
{
  return Convert<const char*>::to_json (value, allocator);
}

/// Convert C++ char array to JsonValue
template<size_t N> static inline auto
to_json (const char (&c)[N], JsonAllocator &allocator)
{
  return Convert<const char*>::to_json (c, N - 1, allocator);
}
template<size_t N> static inline auto
to_json (const char (&c)[N], size_t l, JsonAllocator &allocator)
{
  return Convert<const char*>::to_json (c, l, allocator);
}

/// Simple way to generate a string from a JsonValue
static inline std::string
jsonvalue_to_string (const JsonValue &value)
{
  rapidjson::StringBuffer buffer;
  StringBufferWriter writer (buffer);
  value.Accept (writer);
  const std::string output { buffer.GetString(), buffer.GetSize() };
  return output;
}

/// Generate a string from a simple JsonValue object with up to 4 members.
template<class T1, class T2 = bool, class T3 = bool, class T4 = bool> static inline std::string
jsonobject_to_string (const char *m1, T1 &&v1, const char *m2 = 0, T2 &&v2 = {},
                      const char *m3 = 0, T3 &&v3 = {}, const char *m4 = 0, T4 &&v4 = {})
{
  rapidjson::Document doc (rapidjson::kObjectType);
  auto &a = doc.GetAllocator();
  if (m1 && m1[0]) doc.AddMember (JsonValue (m1, a), to_json (v1, a), a);
  if (m2 && m2[0]) doc.AddMember (JsonValue (m2, a), to_json (v2, a), a);
  if (m3 && m3[0]) doc.AddMember (JsonValue (m3, a), to_json (v3, a), a);
  if (m4 && m4[0]) doc.AddMember (JsonValue (m4, a), to_json (v4, a), a);
  return jsonvalue_to_string (doc);
}

// == CallbackInfo ==
struct CallbackInfo;
using Closure = std::function<void (CallbackInfo&)>;

/// Context for calling C++ functions from Json
struct CallbackInfo final {
  explicit CallbackInfo (const JsonValue &args, JsonAllocator *allocator = nullptr) :
    args_ (args), doc_ (allocator)
  {}
  const JsonValue& ntharg       (size_t index) const { static JsonValue j0; return index < args_.Size() ? args_[index] : j0; }
  size_t           n_args       () const                { return args_.Size(); }
  Closure*         find_closure (const char *methodname);
  std::string      classname    (const std::string &fallback) const;
  JsonAllocator&   allocator    ()                      { return doc_.GetAllocator(); }
  void             set_result   (JsonValue &result)     { result_ = result; have_result_ = true; } // move-semantic!
  JsonValue&       get_result   ()                      { return result_; }
  bool             have_result  () const                { return have_result_; }
  rapidjson::Document& document ()                      { return doc_; }
private:
  const JsonValue &args_;
  JsonValue    result_;
  bool         have_result_ = false;
  rapidjson::Document doc_;
};

// == FunctionTraits ==
/// Template class providing return type and argument type information for functions
template<typename F>                   struct FunctionTraits;
template<typename R, typename ...Args> struct FunctionTraits<R (Args...)> {
  using ReturnType = R;
  using Arguments = std::tuple<Args...>;
};

// Member function pointer
template<typename C, typename R, typename ...Args>
struct FunctionTraits<R (C::*) (Args...)> : FunctionTraits<R (C&, Args...)> {
  // HAS_THIS = true
};

// Const member function pointer
template<typename C, typename R, typename ...Args>
struct FunctionTraits<R (C::*) (Args...) const> : FunctionTraits<R (const C&, Args...)> {
  // HAS_THIS = true
};

// Reference/const callables
template<typename F>
struct FunctionTraits<F&> : FunctionTraits<F> {};
template<typename F>
struct FunctionTraits<const F> : FunctionTraits<F> {};

// Member object pointer
template<typename C, typename R>
struct FunctionTraits<R (C::*)> : FunctionTraits<R (C&)> {
  template<typename D = C> using PointerType = R (D::*);
};

// == CallTraits ==
/// Template class providing conversion helpers for JsonValue to indexed C++ function argument
template<typename F>
struct CallTraits {
  using FuncType = typename std::decay<F>::type;
  using Arguments = typename FunctionTraits<FuncType>::Arguments;
  static constexpr const bool   HAS_THIS = std::is_member_function_pointer<FuncType>::value;
  static constexpr const size_t N_ARGS   = std::tuple_size<Arguments>::value - HAS_THIS;

  // Argument type via INDEX
  template<size_t INDEX, bool> struct TupleElement { using Type = typename std::tuple_element<INDEX, Arguments>::type; };
  template<size_t INDEX>       struct TupleElement<INDEX, false> { using Type = void; }; // void iff INDEX out of range
  template<size_t INDEX>       using  ArgType     = typename TupleElement<HAS_THIS + INDEX, (INDEX < N_ARGS)>::Type;
  template<size_t INDEX>       using  ConvertType = decltype (Convert<ArgType<INDEX>>::from_json (std::declval<JsonValue>()));

  template<size_t INDEX> static ConvertType<INDEX>
  arg_from_json (const CallbackInfo &args)
  {
    return Convert<ArgType<INDEX>>::from_json (args.ntharg (HAS_THIS + INDEX));
  }
  template<typename T, size_t ...INDICES> static typename FunctionTraits<F>::ReturnType
  call_unpacked (T &obj, const F &func, const CallbackInfo &args, std::index_sequence<INDICES...>)
  {
    return (obj.*func) (arg_from_json<INDICES> (args)...);
  }
};

// == call_from_json ==
template<typename T, typename F> static inline typename FunctionTraits<F>::ReturnType
call_from_json (T &obj, const F &func, const CallbackInfo &args)
{
  using CallTraits = CallTraits<F>;
  return CallTraits::call_unpacked (obj, func, args, std::make_index_sequence<CallTraits::N_ARGS>());
}

// == InstanceMap ==
/// Maps C++ shared_ptr instances to JSON-wrapped objects with unique IDs, supporting polymorphic upcasting.
class InstanceMap {
  friend class Scope;
  struct TypeidKey {
    const std::type_index tindex;
    void *ptr;
    bool
    operator< (const TypeidKey &other) const noexcept
    {
      return tindex < other.tindex || (tindex == other.tindex && ptr < other.ptr);
    }
  };
public:
  class Wrapper {
    virtual TypeidKey typeid_key     () = 0;
    virtual          ~Wrapper        () {}
    friend            class InstanceMap;
  public:
    virtual Closure*    lookup_closure (const char *method) = 0;
    virtual void        try_upcast     (const std::string &baseclass, void *sptrB) = 0;
    virtual std::string classname      () = 0;
  };
  using CreateWrapper = Wrapper* (*) (const std::shared_ptr<SharedBase> &sptr, size_t &basedepth);
  static std::vector<CreateWrapper>& wrapper_creators() { static std::vector<CreateWrapper> wrapper_creators_; return wrapper_creators_; }
  static void
  register_wrapper (CreateWrapper createwrapper)
  {
    wrapper_creators().push_back (createwrapper);
  }
  template<typename T>
  class InstanceWrapper : public Wrapper {
    std::shared_ptr<T> sptr_;
    virtual
    ~InstanceWrapper ()
    {
      // printf ("InstanceMap::Wrapper: %s: deleting %s wrapper: %p\n", __func__, rtti_typename<T>().c_str(), sptr_.get());
    }
  public:
    explicit  InstanceWrapper (const std::shared_ptr<T> &sptr) : sptr_ (sptr) {}
    Closure*  lookup_closure  (const char *method) override { return Class<T>::lookup_closure (method); }
    TypeidKey typeid_key      () override { return create_typeid_key (sptr_); }
    void      try_upcast      (const std::string &baseclass, void *sptrB) override
    { Class<T>::try_upcast (sptr_, baseclass, sptrB); }
    static TypeidKey
    create_typeid_key (const std::shared_ptr<T> &sptr)
    {
      return { typeid (T), sptr.get() };
    }
    std::string
    classname() override
    {
      return Class<T>::classname();
    }
  };
  using WrapperMap = std::unordered_map<size_t, Wrapper*>;
  using TypeidMap = std::map<TypeidKey, size_t>;
  using IdSet = std::set<size_t>;
  WrapperMap         wmap_;
  TypeidMap          typeid_map_;
  IdSet             *idset_ = nullptr;
  static size_t      next_counter() { static size_t counter_ = 0; return ++counter_; }
  bool
  delete_id (size_t thisid)
  {
    const auto w = wmap_.find (thisid);
    if (w != wmap_.end())
      {
        Wrapper *wrapper = w->second;
        wmap_.erase (w);
        const auto t = typeid_map_.find (wrapper->typeid_key());
        if (t != typeid_map_.end())
          typeid_map_.erase (t);
        delete wrapper;
        if (idset_)
          idset_->erase (thisid);
        return true;
      }
    return false;
  }
public:
  bool
  mark_unused()
  {
    if (idset_)
      return false;
    idset_ = new IdSet();
    return true;
  }
  size_t
  purge_unused (const std::vector<size_t> &unused)
  {
    IdSet preserve;
    if (idset_)
      {
        idset_->swap (preserve);
        delete idset_;
        idset_ = nullptr;
      }
    auto contains = [] (const auto &c, const auto &e) {
      return c.end() != c.find (e);
    };
    size_t preserved = 0;
    for (const size_t id : unused)
      if (!contains (preserve, id))
        delete_id (id);
      else
        preserved++;
    return preserved;
  }
  bool
  empty() const
  {
    return wmap_.empty();
  }
  size_t
  size() const
  {
    return wmap_.size();
  }
  void
  clear (const bool printdebug = false)
  {
    if (idset_)
      {
        delete idset_;
        idset_ = nullptr;
      }
    WrapperMap old;
    std::swap (old, wmap_);
    typeid_map_.clear();
    for (auto &pair : old)
      {
        Wrapper *wrapper = pair.second;
        if (printdebug)
          fprintf (stderr, "Jsonipc::~Wrapper: %s: $id=%zu\n", string_demangle_cxx (wrapper->typeid_key().tindex.name()).c_str(), pair.first);
        delete wrapper;
      }
  }
  virtual
  ~InstanceMap()
  {
    clear();
    JSONIPC_ASSERT_RETURN (wmap_.size() == 0); // deleters shouldn't re-add
    JSONIPC_ASSERT_RETURN (typeid_map_.size() == 0); // deleters shouldn't re-add
  }
  virtual JsonValue
  wrapper_to_json (Wrapper *wrapper, const size_t thisid, JsonAllocator &allocator)
  {
    if (!wrapper)
      return JsonValue(); // null
    JsonValue jobject (rapidjson::kObjectType);
    jobject.AddMember ("$id", thisid, allocator);
    jobject.AddMember ("$class", JsonValue (wrapper->classname().c_str(), allocator), allocator);
    return jobject;
  }
  template<typename T> static JsonValue
  scope_wrap_object (const std::shared_ptr<T> &sptr, JsonAllocator &allocator)
  {
    InstanceMap *imap = Scope::instance_map();
    size_t thisid = 0;
    Wrapper *wrapper = nullptr;
    if (sptr.get())
      {
        const TypeidKey tkey = InstanceWrapper<T>::create_typeid_key (sptr);
        auto it = imap->typeid_map_.find (tkey);
        if (it == imap->typeid_map_.end())
          {
            thisid = next_counter();
            if constexpr (std::is_base_of_v<SharedBase, T>) {
              std::vector<CreateWrapper> &wcreators = wrapper_creators(); //  using CreateWrapper = Wrapper* (*) (const std::shared_ptr<SharedBase> &sptr, size_t &basedepth);
              size_t basedepth = 0;
              for (size_t i = 0; i < wcreators.size(); i++) {
                Wrapper *w = wcreators[i] (sptr, basedepth);
                if (w) {
                  delete wrapper;
                  wrapper = w;
                }
              }
            }
            if (!wrapper)
              wrapper = new InstanceWrapper<T> (sptr);
            imap->wmap_[thisid] = wrapper;
            imap->typeid_map_[tkey] = thisid;
          }
        else
          {
            thisid = it->second;
            auto wt = imap->wmap_.find (thisid);
            wrapper = wt != imap->wmap_.end() ? wt->second : nullptr;
          }
      }
    if (imap->idset_)
      imap->idset_->insert (thisid);
    /* A note about TypeidKey:
     * Two tuples (TypeX,ptr0x123) and (TypeY,ptr0x123) holding the same pointer address can
     * occur if the RTII lookup to determine the actual Wrapper class fails, e.g. when
     * Class<MostDerived> is unregisterd. In this case, ptr0x123 can be wrapped multiple
     * times through different base classes.
     */
    return imap->wrapper_to_json (wrapper, thisid, allocator);
  }
  virtual Wrapper*
  wrapper_from_json (const JsonValue &value)
  {
    if (value.IsObject())
      {
        auto it = value.FindMember ("$id");
        if (it != value.MemberEnd())
          {
            const size_t thisid = Convert<size_t>::from_json (it->value);
            if (thisid)
              {
                auto tit = wmap_.find (thisid);
                if (tit != wmap_.end())
                  return tit->second;
              }
          }
      }
    return nullptr;
  }
  static Wrapper*
  scope_lookup_wrapper (const JsonValue &value)
  {
    InstanceMap *imap = Scope::instance_map();
    return imap ? imap->wrapper_from_json (value) : nullptr;
  }
  static bool
  scope_forget_id (size_t thisid)
  {
    InstanceMap *imap = Scope::instance_map();
    return imap->delete_id (thisid);
  }
};

inline Closure*
CallbackInfo::find_closure (const char *methodname)
{
  const JsonValue &value = ntharg (0);
  InstanceMap::Wrapper *iw = InstanceMap::scope_lookup_wrapper (value);
  return iw ? iw->lookup_closure (methodname) : nullptr;
}

inline std::string
CallbackInfo::classname (const std::string &fallback) const
{
  const JsonValue &value = ntharg (0);
  InstanceMap::Wrapper *iw = InstanceMap::scope_lookup_wrapper (value);
  return iw ? iw->classname() : fallback;
}

inline
Scope::Scope (InstanceMap &instance_map, ScopeLocalsP localsp) :
  instance_map_ (instance_map), localsp_ (localsp ? localsp : ScopeLocalsP (&scope_locals_, [] (ScopeLocals*) {}))
{
  auto &stack_ = stack();
  stack_.push_back (this);
}

// == DefaultConstant ==
using DefaultConstantVariant = std::variant<std::monostate, std::nullptr_t, uint64_t, int64_t, double, std::string>;
/// Wrapper for function argument default value constants
struct DefaultConstant : DefaultConstantVariant {
  DefaultConstant () = default;
  template<typename T, REQUIRES< std::is_integral<T>::value && std::is_unsigned<T>::value> = true>
  DefaultConstant (T a) : DefaultConstantVariant (uint64_t (a))         {}
  template<typename T, REQUIRES< std::is_integral<T>::value && !std::is_unsigned<T>::value> = true>
  DefaultConstant (T a) : DefaultConstantVariant (int64_t (a))          {}
  template<typename T, REQUIRES< std::is_convertible<T, double>::value && !std::is_integral<T>::value> = true>
  DefaultConstant (T a) : DefaultConstantVariant (double (a))           {}
  template<typename T, REQUIRES< std::is_convertible<T, std::string>::value && !std::is_same<T, std::nullptr_t>::value> = true>
  DefaultConstant (T a) : DefaultConstantVariant (std::string (a))      {}
  template<typename T, REQUIRES< std::is_same<T, std::nullptr_t>::value> = true>
  DefaultConstant (T a) : DefaultConstantVariant (a)                    {}
};
using DefaultsList = std::initializer_list<DefaultConstant>;

// == normalize_typename ==
/// Yield the Javascript identifier name by substituting ':+' with '.'
inline std::string
normalize_typename (const std::string &string)
{
  std::string normalized;
  auto is_identifier_char = [] (int ch) {
    return ( (ch >= 'A' && ch <= 'Z') ||
             (ch >= 'a' && ch <= 'z') ||
             (ch >= '0' && ch <= '9') ||
             ch == '_' || ch == '$' );
  };
  for (size_t i = 0; i < string.size() && string[i]; ++i)
    if (is_identifier_char (string[i]))
      normalized += string[i];
    else if (normalized.size() && normalized[normalized.size() - 1] != '.')
      normalized += '.';
  return normalized;
}

// == JavaScript Helpers ==
/// JS initializer from C++ type
template<class V> static inline unsigned
js_initializer_index ()
{
  using T = std::decay_t<V>;
  if constexpr (std::is_same<T,bool>::value)                            return 3;
  if constexpr (std::is_integral<T>::value)                             return 1;
  if constexpr (std::is_floating_point<T>::value)                       return 2;
  if constexpr (std::is_convertible<const char*const, T>::value)        return 4;
  if constexpr (std::is_convertible<const std::string, T>::value)       return 4;
  if constexpr (std::is_enum<T>::value)                                 return 4; // [4]='' [1]=0
  if constexpr (DerivesVector<T>::value)                                return 5;
  if constexpr (DerivesPair<T>::value)                                  return 5;
  if constexpr (!IsSharedPtr<T>::value && std::is_class<T>::value)      return 6;
  return 0;
}
static constexpr const char *const js_initializers[] = { "null", "0", "0.0", "false", "''", "[]", "{}" };

// == TypeScript Type Mapping ==
inline std::string
short_name (const std::string &full_name)
{
  const size_t last_colon = full_name.rfind ("::");
  return last_colon == std::string::npos ? full_name : full_name.substr (last_colon + 2);
}
template<typename T> struct typescript_name {
  static std::string name() { return short_name (rtti_typename<T>()); }
};
template<typename T> struct typescript_name<T&> : typescript_name<T> {};
template<typename T> struct typescript_name<const T&> : typescript_name<T> {};
template<typename T> struct typescript_name<std::shared_ptr<T>> : typescript_name<T> {};
template<typename T>
struct typescript_name<T*> {
  static std::string name() { return typescript_name<T>::name() + " | null"; }
};
template<typename T1, typename T2>
struct typescript_name<std::pair<T1, T2>> {
  static std::string name() { return "[" + typescript_name<T1>::name() + ", " + typescript_name<T2>::name() + "]"; }
};
template<typename T>
struct typescript_name<std::vector<T>> {
  static std::string name() { return typescript_name<T>::name() + "[]"; }
};
template<typename T>
struct typescript_name<std::map<std::string, T>> {
  static std::string name() { return "{ [key: string]: " + typescript_name<T>::name() + " }"; }
};
template<typename T>
struct typescript_name<std::unordered_map<std::string, T>> {
  static std::string name() { return "{ [key: string]: " + typescript_name<T>::name() + " }"; }
};
#define JSONIPC_MAP_TO_TYPESCRIPT(CXXTYPE,TSTYPE)                       \
  template<> struct Jsonipc::typescript_name< CXXTYPE >  { static std::string name() { return TSTYPE; } }

template<typename... Args> std::string
typescript_arg_list()
{
  std::string s;
  int i = 0;
  auto print_one_arg = [&] (const std::string &type_name) {
    if (i > 0) s += ", ";
    s += string_format ("arg%d: %s", ++i, type_name.c_str());
  };
  (print_one_arg (typescript_name<Args>::name()), ...); // C++17 fold expr
  return s;
}
template<typename... Args> std::string
typescript_arg_names_list()
{
  std::string s;
  int i = 0;
  auto append_arg_name = [&] (const std::string &type_name) {
    s += string_format (", arg%d", ++i);
  };
  (append_arg_name (typescript_name<Args>::name()), ...); // C++17 fold expr
  return s;
}
template<typename C, typename R, typename... Args> std::string
typescript_call_impl (const std::string &method_name)
{
  std::string s;
  s += string_format ("  %s (", method_name.c_str());
  s += typescript_arg_list<Args...>();
  s += string_format ("): Promise<%s>\n", typescript_name<R>::name().c_str());
  s += string_format ("  { return this.$rpc (\"%s\", [this%s]); }\n",
                      method_name.c_str(), typescript_arg_names_list<Args...>().c_str());
  return s;
}
template<typename T, typename Ret, typename... Args> std::string
typescript_call (const std::string &method_name, Ret (T::*func) (Args...))
{
  return typescript_call_impl<T, Ret, Args...> (method_name);
}
template<typename T, typename Ret, typename... Args> std::string // const overload
typescript_call (const std::string &method_name, Ret (T::*func) (Args...) const)
{
  return typescript_call_impl<T, Ret, Args...> (method_name);
}

// == BindingPrinter ==
class BindingPrinter {
  enum Kind { ANY, ENUM, VALUE, RECORD, FIELD, CLASS, METHOD };
  std::string b_;
  std::string open_enum_, open_record_, open_class_;
  std::vector<std::tuple<std::string, std::string, std::string, std::string>> record_fields_;
  size_t class_inherit_pos_ = 0;
  void
  close()
  {
    if (open_enum_.size())
      close_enum();
    if (open_record_.size())
      close_record();
    if (open_class_.size())
      close_class();
  }
  template<class, class = void> struct has_nested_T : std::false_type {}; // Check for nested ::T type
  template<typename U>          struct has_nested_T<U, std::void_t<typename U::T>> : std::true_type {};
  template<typename M>  struct typescript_call_from_type; // Typescript signature from member function pointer
  template<typename C, typename R, typename... Args>
  struct typescript_call_from_type<R (C::*)(Args...)> {
    static std::string
    generate (const std::string &method_name)
    {
      return typescript_call_impl<C, R, Args...> (method_name);
    }
  };
  template<typename C, typename R, typename... Args>
  struct typescript_call_from_type<R (C::*)(Args...) const> {
    static std::string
    generate (const std::string &method_name)
    {
      return typescript_call_impl<C, R, Args...>(method_name);
    }
  };
public:
  std::string finish() { close(); return b_; }
  template<typename T> void
  enum_type()
  {
    close();
    open_enum_ = rtti_typename<typename std::decay<T>::type>();
    b_ += "export const " + short_name (open_enum_) + " = { // " + open_enum_ + "\n";
  }
  template<typename T> void
  enum_value (const std::string &name, T v)
  {
    const std::string full_js_name = normalize_typename (open_enum_) + "." + name;
    using underlying = typename std::underlying_type<T>::type;
    b_ += "  " + name + ": \"" + full_js_name + "\", // " + std::to_string(static_cast<underlying>(v)) + "\n";
  }
  void
  close_enum()
  {
    b_ += "} as const;\n";
    const std::string shortname = short_name (open_enum_);
    b_ += "export type " + shortname + " = typeof " + shortname + "[keyof typeof " + shortname + "];\n";
    b_ += "Jsonipc.classes[\"" + open_enum_ + "\"] = " + shortname + ";\n\n";
    open_enum_.clear();
  }
  template<typename T> void
  record_type()
  {
    close();
    open_record_ = rtti_typename<typename std::decay<T>::type>();
    b_ += "export class " + short_name (open_record_) + " { // " + open_record_ + "\n";
  }
  template<typename T, typename A> void
  field_member (const std::string &name)
  {
    const std::string ts_type_name = typescript_name<A>::name();
    const std::string default_value = js_initializers[js_initializer_index<A>()];
    const std::string as_cast = std::is_enum<A>::value ? ts_type_name : "";
    record_fields_.emplace_back (name, ts_type_name, default_value, as_cast);
  }
  void
  close_record()
  {
    for (const auto &[field_name, ts_type_name, default_value, as_cast] : record_fields_)
      b_ += "  " + field_name + ": " + ts_type_name + ";\n";
    b_ += "  constructor (";
    for (size_t i = 0; i < record_fields_.size(); i++) {
      const auto &[field_name, ts_type_name, default_value, as_cast] = record_fields_[i];
      b_ += (i ? ", " : "") + field_name + ": " + ts_type_name + " = " + default_value;
      if (as_cast.size())
        b_ += " as " + as_cast;
    }
    b_ += ")\n  {\n";
    for (const auto &[field_name, ts_type_name, default_value, as_cast] : record_fields_)
      b_ += "    this." + field_name + " = " + field_name + ";\n";
    b_ += "  }\n";
    b_ += "};\n";
    const std::string shortname = short_name (open_record_);
    b_ += "Jsonipc.classes[\"" + open_record_ + "\"] = " + shortname + ";\n\n";
    record_fields_.clear();
    open_record_.clear();
  }
  template<typename T> void
  class_type()
  {
    close();
    open_class_ = rtti_typename<typename std::decay<T>::type>();
    const std::string shortname = short_name (open_class_);
    b_ += "export class " + shortname + " // " + open_class_ + "\n";
    class_inherit_pos_ = b_.size();
    b_ += "{\n";
    b_ += "  constructor ($id)\n";
    b_ += "  { super ($id); if (new.target === " + shortname + ") Jsonipc.ofreeze (this); }\n";
  }
  template<typename B> void
  inherit_type()
  {
    const std::string base_class_ = rtti_typename<typename std::decay<B>::type>();
    b_.insert (class_inherit_pos_, "  extends Jsonipc.classes[\"" + base_class_ + "\"]\n");
  }
  template<typename T, typename M> void
  method_member (const std::string &name)
  {
    b_ += typescript_call_from_type<M>::generate (name);
  }
  template<typename T, typename R, typename A> void
  field_accessor (const std::string &name)
  {
    const std::string ts_type = typescript_name<R>::name();
    b_ += "  get " + name + " (): " + ts_type + "\n";
    b_ += "  { return this.$get (\"" + name + "\", " + js_initializers[js_initializer_index<R>()] + ") as " + ts_type + "; }\n";
    b_ += "  set " + name + " (v: " + ts_type + ")\n";
    b_ += "  { this.$set (\"" + name + "\", v); }\n";
  }
  void
  close_class()
  {
    b_ += "};\n";
    const std::string shortname = short_name (open_class_);
    b_ += "Jsonipc.classes[\"" + open_class_ + "\"] = " + shortname + ";\n\n";
    open_class_.clear();
  }
};
inline BindingPrinter *g_binding_printer = nullptr;

// == TypeInfo ==
/// Base class for type registration (Enum, Serializable, Class) that optionally generates TypeScript bindings.
class TypeInfo {
protected:
  virtual ~TypeInfo() {}
  explicit TypeInfo() {}
};

// == Enum ==
/// Registers C++ enum values with string names for JSON serialization and TypeScript binding generation.
template<typename T>
struct Enum final : TypeInfo {
  static_assert (std::is_enum<T>::value, "");
  Enum ()
  {
    if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->enum_type<T> ();
  }
  using UnderlyingType = typename std::underlying_type<T>::type;
  Enum&
  set (T v, const char *valuename)
  {
    const std::string class_name = typename_of<T>();
    auto &entries_ = entries();
    auto normalized_typename = normalize_typename (class_name + "." + valuename);
    Entry e { normalized_typename, v };
    entries_.push_back (e);
    if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->enum_value<T> (valuename, v);
    return *this;
  }
  static bool
  has_names ()
  {
    return !entries().empty();
  }
  static const std::string&
  get_name (T v)
  {
    const auto &entries_ = entries();
    for (const auto &e : entries_)
      if (v == e.value)
        return e.name;
    static const std::string empty;
    return empty;
  }
  static T
  get_value (const std::string &name, T fallback)
  {
    auto c_isalnum = [] (char c) {
      return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    };
    const auto &entries_ = entries();
    for (const auto &e : entries_)
      if (name == e.name ||                                             // exact match, or
          (name.size() < e.name.size() &&                               // name starts at e.name word boundary
           !c_isalnum (e.name[e.name.size() - name.size() - 1]) &&      // and matches the tail of e.name
           e.name.compare (e.name.size() - name.size(), name.size(), name) == 0))
        return e.value;
    return fallback;
  }
  using EnumValueS = std::vector<std::pair<int64_t,std::string>>;
  static EnumValueS
  list_values ()
  {
    EnumValueS enumvalues;
    const auto &entries_ = entries();
    for (const auto &e : entries_)
      enumvalues.push_back ({ int64_t (e.value), e.name });
    return enumvalues;
  }
private:
  struct Entry { const std::string name; T value; };
  static std::vector<Entry>& entries() { static std::vector<Entry> entries_; return entries_; }
  template<typename U> static std::string
  typename_of()
  {
    using Type = typename std::decay<U>::type;
    return rtti_typename<Type>();
  }
};

// enum types
template<typename T>
struct Convert<T, REQUIRESv< std::is_enum<T>::value > > {
  using UnderlyingType = typename std::underlying_type<T>::type;
  static T
  from_json (const JsonValue &value, T fallback = T())
  {
    if (value.IsString())
      {
        using EnumType = Enum<T>;
        const std::string string = Convert<std::string>::from_json (value);
        return EnumType::get_value (string, fallback);
      }
    return T (Convert<UnderlyingType>::from_json (value, UnderlyingType (fallback)));
  }
  static JsonValue
  to_json (T evalue, JsonAllocator &allocator)
  {
    using EnumType = Enum<T>;
    if (EnumType::has_names())
      {
        const std::string &name = EnumType::get_name (evalue);
        if (!name.empty())
          return Convert<std::string>::to_json (name, allocator);
      }
    return Convert<UnderlyingType>::to_json (UnderlyingType (evalue), allocator);
  }
};

// == Serializable ==
/// Jsonipc wrapper type for objects that support field-wise serialization to/from JSON.
template<typename T>
struct Serializable final : TypeInfo {
  /// Allow object handles to be streamed to/from Javascript, needs a Scope for temporaries.
  Serializable()
  {
    make_serializable<T>();
    if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->record_type<T>();
  }
  /// Add a member object pointer
  template<typename A, REQUIRES< std::is_member_object_pointer<A>::value > = true> Serializable&
  set (const char *name, A attribute)
  {
    using SetterAttributeType = typename FunctionTraits<A>::ReturnType;
    Accessors accessors;
    accessors.setter = [attribute] (T &obj, const JsonValue &value) -> void      { obj.*attribute = from_json<SetterAttributeType> (value); };
    accessors.getter = [attribute] (const T &obj, JsonAllocator &a) -> JsonValue { return to_json (obj.*attribute, a); };
    AccessorMap &amap = accessormap();
    auto it = amap.find (name);
    if (it != amap.end())
      throw std::runtime_error ("duplicate attribute registration: " + std::string (name));
    amap.insert (std::make_pair<std::string, Accessors> (name, std::move (accessors)));
    const std::string class_name = rtti_typename<T>();
    if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->field_member<T,SetterAttributeType> (name);
    return *this;
  }
  static bool               is_serializable     ()                              { return serialize_from_json_() && serialize_to_json_(); }
  static JsonValue          serialize_to_json   (const T &o, JsonAllocator &a)  { return serialize_to_json_() (o, a); }
  static std::shared_ptr<T> serialize_from_json (const JsonValue &value,
                                                 const std::shared_ptr<T> &p = 0) { return serialize_from_json_() (value, p); }
private:
  struct Accessors {
    std::function<void      (T&,const JsonValue&)>      setter;
    std::function<JsonValue (const T&, JsonAllocator&)> getter;
  };
  using AccessorMap = std::map<std::string, Accessors>;
  static AccessorMap& accessormap() { static AccessorMap amap; return amap; }
  template<typename U> static void
  make_serializable()
  {
    // implement serialize_from_json by calling all setters
    SerializeFromJson sfj = [] (const JsonValue &value, const std::shared_ptr<T> &p) -> std::shared_ptr<T>  {
      std::shared_ptr<T> obj = p ? p : Scope::make_shared<T>();
      if (!obj)
        return obj;
      AccessorMap &amap = accessormap();
      for (const auto &field : value.GetObject())
        {
          const std::string field_name = field.name.GetString();
          auto it = amap.find (field_name);
          if (it == amap.end())
            continue;
          Accessors &accessors = it->second;
          accessors.setter (*obj, field.value);
        }
      return obj;
    };
    serialize_from_json_() = sfj;
    // implement serialize_to_json by calling all getters
    SerializeToJson stj = [] (const T &object, JsonAllocator &allocator) -> JsonValue {
      JsonValue jobject (rapidjson::kObjectType);               // serialized result
      AccessorMap &amap = accessormap();
      for (auto &it : amap)
        {
          const std::string field_name = it.first;
          Accessors &accessors = it.second;
          JsonValue result = accessors.getter (object, allocator);
          jobject.AddMember (JsonValue (field_name.c_str(), allocator), result, allocator);
        }
      return jobject;
    };
    serialize_to_json_() = stj;
  }
  using SerializeFromJson = std::function<std::shared_ptr<T> (const JsonValue&, const std::shared_ptr<T>&)>;
  using SerializeToJson = std::function<JsonValue (const T&, JsonAllocator&)>;
  static SerializeFromJson& serialize_from_json_ () { static SerializeFromJson impl; return impl; }
  static SerializeToJson&   serialize_to_json_   () { static SerializeToJson impl; return impl; }
};

// == Class ==
template<typename T>
struct Class final : TypeInfo {
  Class (bool internal = false)
  {
    auto create_wrapper = [] (const std::shared_ptr<SharedBase> &sptr, size_t &basedepth) -> InstanceMap::Wrapper*
    {
      /* This is an exhaustive search for the best (most derived) wrapper type for
       * an object. We currently use a linear search that can involve as many
       * dynamic casts as the inheritance depth of the registered wrappers.
       */
      const size_t class_depth = Class::base_depth();
      if (class_depth > basedepth) {
        std::shared_ptr<T> derived_sptr = std::dynamic_pointer_cast<T> (sptr);
        if (derived_sptr.get()) {
          basedepth = class_depth;
          return new InstanceMap::InstanceWrapper<T> (derived_sptr);
        }
      }
      return nullptr;
    };
    InstanceMap::register_wrapper (create_wrapper);
    if (!internal && JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->class_type<T>();
  }
  // Inherit base class `B`
  template<typename B> Class&
  inherit()
  {
    add_base<B>();
    if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->inherit_type<B>();
    return *this;
  }
  /// Add a member function pointer
  template<typename F, REQUIRES< std::is_member_function_pointer<F>::value > = true> Class&
  set (const char *name, const F &method)
  {
    add_member_function_closure (name, make_closure (method));
     if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->method_member<T,F> (name);
   return *this;
  }
  /// Add a member object accessors
  template<typename R, typename A, typename C, typename VB> Class&
  set (const char *name, R (C::*get) () const, VB (C::*set) (A))
  {
    static_assert (std::is_same_v<void, VB> || std::is_same_v<bool, VB>);
    JSONIPC_ASSERT_RETURN (get && set, *this);
    add_member_function_closure (std::string ("get/") + name, make_closure (get));
    add_member_function_closure (std::string ("set/") + name, make_closure (set));
    if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->field_accessor<T,R,A> (name);
    return *this;
  }
  template<typename F, REQUIRES< std::is_member_function_pointer<F>::value > = true> Class&
  set_d (const char *name, const F &method, const DefaultsList &dflts)
  {
    constexpr const size_t N_ARGS = CallTraits<F>::N_ARGS;
    JSONIPC_ASSERT_RETURN (dflts.size() <= N_ARGS, *this);
    add_member_function_closure (name, make_closure (method));
    if (JSONIPC_UNLIKELY (g_binding_printer))
      g_binding_printer->method_member<T,F> (name);
    return *this;
  }
  static std::string
  classname ()
  {
    return typename_of<T>();
  }
  static std::shared_ptr<T>
  object_from_json (const JsonValue &value)
  {
    InstanceMap::Wrapper *iw = InstanceMap::scope_lookup_wrapper (value);
    if (iw)
      {
        std::shared_ptr<T> base_sptr = nullptr;
        iw->try_upcast (classname(), &base_sptr);
        if (base_sptr)
          return base_sptr;
      }
    return nullptr;
  }
private:
  template<typename U> static std::string
  typename_of()
  {
    using Type = typename std::decay<U>::type;
    return rtti_typename<Type>();
  }
  template<typename F>
  using HasVoidReturn = std::is_same<void, typename FunctionTraits<F>::ReturnType>;
  template<typename F, REQUIRES< HasVoidReturn<F>::value > = true> Closure
  make_closure (const F &method)
  {
    return [method] (const CallbackInfo &cbi) {
      const bool HAS_THIS = true;
      if (HAS_THIS + CallTraits<F>::N_ARGS != cbi.n_args())
        throw Jsonipc::bad_invocation (-32602, "Invalid params: wrong number of arguments");
      std::shared_ptr<T> instance = object_from_json (cbi.ntharg (0));
      if (!instance)
        throw Jsonipc::bad_invocation (-32603, "Internal error: closure without this");
      call_from_json (*instance, method, cbi);
    };
  }
  template<typename F, REQUIRES< !HasVoidReturn<F>::value > = true> Closure
  make_closure (const F &method)
  {
    return [method] (CallbackInfo &cbi) {
      const bool HAS_THIS = true;
      if (HAS_THIS + CallTraits<F>::N_ARGS != cbi.n_args())
        throw Jsonipc::bad_invocation (-32602, "Invalid params: wrong number of arguments");
      std::shared_ptr<T> instance = object_from_json (cbi.ntharg (0));
      if (!instance)
        throw Jsonipc::bad_invocation (-32603, "Internal error: closure without this");
      JsonValue rv;
      rv = to_json (call_from_json (*instance, method, cbi), cbi.allocator());
      cbi.set_result (rv);
    };
  }
  void
  add_member_function_closure (const std::string &name, Closure &&closure)
  {
    MethodMap &mmap = methodmap();
    auto it = mmap.find (name);
    if (it != mmap.end())
      throw std::runtime_error ("duplicate method registration: " + name);
    mmap.insert (std::make_pair<std::string, Closure> (name.c_str(), std::move (closure)));
  }
  using MethodMap = std::map<std::string, Closure>;
  static MethodMap& methodmap() { static MethodMap methodmap_; return methodmap_; }
  struct BaseInfo {
    std::string basetypename;
    size_t    (*base_depth)     ();
    bool      (*upcast_impl)    (const std::shared_ptr<T>&, const std::string&, void*) = NULL;
    Closure*  (*lookup_closure) (const char*) = NULL;
  };
  using BaseVec   = std::vector<BaseInfo>;
  template<typename B> void
  add_base ()
  {
    BaseVec &bvec = basevec();
    BaseInfo binfo { typename_of<B>(), Class<B>::base_depth, &upcast_impl<B>, &Class<B>::lookup_closure, };
    for (const auto &it : bvec)
      if (it.basetypename == binfo.basetypename)
        throw std::runtime_error ("duplicate base registration: " + binfo.basetypename);
    bvec.push_back (binfo);
    Class<B> bclass (true); // internal=true; force registration for base_depth
  }
  static BaseVec&   basevec  () { static BaseVec basevec_;     return basevec_; }
  template<typename B> static bool
  upcast_impl (const std::shared_ptr<T> &sptr, const std::string &baseclass, void *sptrB)
  {
    std::shared_ptr<B> bptr = sptr;
    return Class<B>::try_upcast (bptr, baseclass, sptrB);
  }
public:
  static size_t
  base_depth ()
  {
    const BaseVec &bvec = basevec();
    size_t d = 0;
    for (const auto &binfo : bvec)
      {
        const size_t b = binfo.base_depth();
        if (b > d)
          d = b;
      }
    return d + 1;
  }
  static Closure*
  lookup_closure (const char *methodname)
  {
    MethodMap &mmap = methodmap();
    auto it = mmap.find (methodname);
    if (it != mmap.end())
      return &it->second;
    const BaseVec &bvec = basevec();
    for (const auto &base : bvec)
      {
        Closure *closure = base.lookup_closure (methodname);
        if (closure)
          return closure;
      }
    return nullptr;
  }
  static bool
  try_upcast (std::shared_ptr<T> &sptr, const std::string &baseclass, void *sptrB)
  {
    if (classname() == baseclass)
      {
        std::shared_ptr<T> *baseptrp = static_cast<std::shared_ptr<T>*> (sptrB);
        *baseptrp = sptr;
        return true;
      }
    const BaseVec &bvec = basevec();
    for (const auto &it : bvec)
      if (it.upcast_impl (sptr, baseclass, sptrB))
        return true;
    return false;
  }
};

/// Template class to identify wrappable classes
template<typename T, typename Enable = void>
struct IsWrappableClass;
template<typename T>
struct IsWrappableClass<T, REQUIRESv< std::is_class<T>::value &&
                                      !IsSharedPtr<T>::value &&
                                      !DerivesPair<T>::value &&
                                      !DerivesVector<T>::value >> : std::true_type {};
template<>
struct IsWrappableClass<std::string> : std::false_type {};
template<typename T>
struct IsWrappableClass<T, REQUIRESv< DerivesVector<T>::value >> : std::false_type {};
template<typename T>
struct IsWrappableClass<T, REQUIRESv< DerivesPair<T>::value >> : std::false_type {};

/// Convert wrapped Class shared pointer
template<typename T>
struct Convert<std::shared_ptr<T>, REQUIRESv< IsWrappableClass<T>::value >> {
  using ClassType = typename std::remove_cv<T>::type;
  static std::shared_ptr<T>
  from_json (const JsonValue &value)
  {
    if (Serializable<ClassType>::is_serializable() && value.IsObject())
      return Serializable<ClassType>::serialize_from_json (value);
    else
      return Class<ClassType>::object_from_json (value);
  }
  static JsonValue
  to_json (const std::shared_ptr<T> &sptr, JsonAllocator &allocator)
  {
    if (Serializable<ClassType>::is_serializable())
      return sptr ? Serializable<ClassType>::serialize_to_json (*sptr, allocator) : JsonValue (rapidjson::kObjectType);
    if (sptr)
      {
        // Wrap sptr, determine most derived wrapper via dynamic casts
        const std::string impltype = rtti_typename (*sptr);
        JsonValue result = InstanceMap::scope_wrap_object<ClassType> (sptr, allocator);
        return result;
      }
    return JsonValue(); // null
  }
};

/// Clear wrapped Class from lookup table
static inline void
forget_json_id (size_t id)
{
  InstanceMap::scope_forget_id (id);
}

/// Convert wrapped Class pointer
template<typename T>
struct Convert<T*, REQUIRESv< IsWrappableClass<T>::value >> {
  using ClassType = typename std::remove_cv<T>::type;
  static T*
  from_json (const JsonValue &value)
  {
    return &*Convert<std::shared_ptr<T>>::from_json (value);
  }
  static JsonValue
  to_json (const T *obj, JsonAllocator &allocator)
  {
    if (Serializable<ClassType>::is_serializable())
      return obj ? Serializable<ClassType>::serialize_to_json (*obj, allocator) : JsonValue (rapidjson::kObjectType);
    // Caveat: Jsonipc will only auto-convert to most-derived-type iff it is registered and when looking at a shared_ptr<BaseType>
    std::shared_ptr<T> sptr;
    if constexpr (Has_shared_from_this<T>::value)
      {
        if (obj)
          sptr = std::dynamic_pointer_cast<T> (const_cast<T&> (*obj).shared_from_this());
      }
    // dprintf (2, "shared_from_this: type<%d>=%s ptr=%p\n", Has_shared_from_this<T>::value, rtti_typename<T>().c_str(), sptr.get());
    return Convert<std::shared_ptr<T>>::to_json (sptr, allocator);
  }
};

/// Convert wrapped Class
template<typename T>
struct Convert<T, REQUIRESv< IsWrappableClass<T>::value >> {
  using ClassType = typename std::remove_cv<T>::type;
  static T&
  from_json (const JsonValue &value)
  {
    T *object = Convert<T*>::from_json (value);
    if (object)
      return *object;
    throw Jsonipc::bad_invocation (-32602, "Invalid params: attempt to cast null to reference type");
  }
  static JsonValue
  to_json (const T &object, JsonAllocator &allocator)
  {
    return Convert<T*>::to_json (&object, allocator);
  }
};

// == IpcDispatcher ==
struct IpcDispatcher {
  void
  add_method (const std::string &methodname, const Closure &closure)
  {
    extra_methods[methodname] = closure;
  }
  // Dispatch JSON message and return result. Requires a live Scope instance in the current thread.
  std::string
  dispatch_message (const std::string &message)
  {
    rapidjson::Document document;
    document.Parse<rapidjson_parse_flags> (message.data(), message.size());
    size_t id = 0;
    try {
      if (document.HasParseError())
        return create_error (id, -32700, "Parse error");
      const char *methodname = nullptr;
      const JsonValue *args = nullptr;
      for (const auto &m : document.GetObject())
        if (m.name == "id")
          id = from_json<size_t> (m.value, 0);
        else if (m.name == "method")
          methodname = from_json<const char*> (m.value);
        else if (m.name == "params" && m.value.IsArray())
          args = &m.value;
      if (!id || !methodname || !args || !args->IsArray())
        return create_error (id, -32600, "Invalid Request");
      CallbackInfo cbi (*args);
      Closure *closure = cbi.find_closure (methodname);
      if (!closure)
        {
          const auto it = extra_methods.find (methodname);
          if (it != extra_methods.end())
            closure = &it->second;
          else if (strcmp (methodname, "Jsonipc/handshake") == 0)
            {
              static Closure initialize = [] (CallbackInfo &cbi) { return jsonipc_initialize (cbi); };
              closure = &initialize;
            }
        }
      if (!closure)
        return create_error (id, -32601, "Method not found: " + cbi.classname ("<unknown-this>") + "['" + methodname + "']");
      (*closure) (cbi);
      return create_reply (id, cbi.get_result(), !cbi.have_result(), cbi.document());
    } catch (const Jsonipc::bad_invocation &exc) {
      return create_error (id, exc.code(), exc.what());
    }
  }
private:
  std::map<std::string, Closure> extra_methods;
  std::string
  create_reply (size_t id, JsonValue &result, bool skip_result, rapidjson::Document &d)
  {
    auto &a = d.GetAllocator();
    d.SetObject();
    d.AddMember ("id", id, a);
    d.AddMember ("result", result, a); // move-semantics!
    rapidjson::StringBuffer buffer;
    StringBufferWriter writer (buffer);
    d.Accept (writer);
    std::string output { buffer.GetString(), buffer.GetSize() };
    return output;
  }
  std::string
  create_error (size_t id, int errorcode, const std::string &message)
  {
    rapidjson::Document d (rapidjson::kObjectType);
    auto &a = d.GetAllocator();
    d.AddMember ("id", id ? JsonValue (id) : JsonValue(), a);
    JsonValue error (rapidjson::kObjectType);
    error.AddMember ("code", errorcode, a);
    error.AddMember ("message", JsonValue (message.c_str(), a).Move(), a);
    d.AddMember ("error", error, a); // moves error to null
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer (buffer);
    d.Accept (writer);
    std::string output { buffer.GetString(), buffer.GetSize() };
    return output;
  }
  static std::string*
  jsonipc_initialize (CallbackInfo &cbi)
  {
    cbi.set_result (to_json (0x00000001, cbi.allocator()).Move());
    return nullptr; // no error
  }
};

} // Jsonipc

JSONIPC_MAP_TO_TYPESCRIPT (void,          "void");
JSONIPC_MAP_TO_TYPESCRIPT (bool,          "boolean");
JSONIPC_MAP_TO_TYPESCRIPT (::int8_t,      "number");
JSONIPC_MAP_TO_TYPESCRIPT (::uint8_t,     "number");
JSONIPC_MAP_TO_TYPESCRIPT (::int32_t,     "number");
JSONIPC_MAP_TO_TYPESCRIPT (::uint32_t,    "number");
JSONIPC_MAP_TO_TYPESCRIPT (::int64_t,     "number");
JSONIPC_MAP_TO_TYPESCRIPT (::uint64_t,    "number");
JSONIPC_MAP_TO_TYPESCRIPT (float,         "number");
JSONIPC_MAP_TO_TYPESCRIPT (double,        "number");
JSONIPC_MAP_TO_TYPESCRIPT (const char*,   "string");
JSONIPC_MAP_TO_TYPESCRIPT (::std::string, "string");
