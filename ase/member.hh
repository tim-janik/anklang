// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/parameter.hh>

namespace Ase {

struct MemberDetails {
  static constexpr uint64_t READABLE = 1, WRITABLE = 2, STORAGE = 4, GUI = 8,
    RW_STORAGE = READABLE + WRITABLE + STORAGE, FLAGS_DEFAULT = RW_STORAGE + GUI;
  uint64_t flags = FLAGS_DEFAULT;
  StringS  infos;
  bool     is_unset   () const { return flags == FLAGS_DEFAULT && infos.empty(); }
  bool     operator== (const MemberDetails &m) const { return flags == m.flags && infos == m.infos; }
  String
  info (const String &key) const
  {
    for (const auto &kv : infos)
      if (kv.size() > key.size() && kv[key.size()] == '=' &&
          kv.compare (0, key.size(), key) == 0)
        return kv.substr (key.size() + 1);
    return "";
  }
  const MemberDetails&
  assign1 (const MemberDetails *newmeta)
  { // assign once only
    MemberDetails &old_meta = *this;
    if (newmeta) {
      ASE_ASSERT (old_meta.is_unset() || old_meta == *newmeta);
      old_meta = *newmeta;
    }
    return *this;
  }
};

namespace Lib {         // Implementation internals
/// Match function pointers
template<typename F>                   struct FunctionTraits;
template<typename R, typename ...Args> struct FunctionTraits<R (Args...)> {
  using ReturnType = R;
  using Arguments = std::tuple<Args...>;
};
/// Match member function pointer
template<typename C, typename R, typename ...Args>
struct FunctionTraits<R (C::*) (Args...)> : FunctionTraits<R (C&, Args...)> {
  using ClassType = C;
  using ReturnType = R;
};
/// Match const member function pointer
template<typename C, typename R, typename ...Args>
struct FunctionTraits<R (C::*) (Args...) const> : FunctionTraits<R (const C&, Args...)> {
  using ClassType = C;
  using ReturnType = R;
  using Arguments = std::tuple<Args...>;
};
/// Member function traits
template<auto Handler>
struct MemberFunctionTraits {
  using FuncType = typename std::decay<decltype (Handler)>::type;
  using Arguments = typename FunctionTraits<FuncType>::Arguments;
  using ClassType = typename FunctionTraits<decltype (Handler)>::ClassType;
  using ReturnType = typename FunctionTraits<decltype (Handler)>::ReturnType;
};
/// Resolve (or assign) `host->Member` offset distance (may be 0).
template<class C, class M> C*
host_member_offset (ptrdiff_t *hmoffsetp, const M *member, C *host)
{
  if (host) {
    const ptrdiff_t member_offset = ptrdiff_t (member) - ptrdiff_t (host);
    ASE_ASSERT (*hmoffsetp == -1 || *hmoffsetp == member_offset);
    *hmoffsetp = member_offset;
  } else {
    ASE_ASSERT (*hmoffsetp >= 0);
    const ptrdiff_t host_offset = ptrdiff_t (member) - *hmoffsetp;
    host = reinterpret_cast<C*> (host_offset);
  }
  return host;
}

bool kvpairs_assign  (StringS &kvs, const String &key_value_pair);

} // Lib

/// Implement C++ member field API with a 0-sized class from setter and getter, maybe combined with `[[no_unique_address]]`.
template<auto getter, auto setter = nullptr>
class Member {
public:
  using GetterTraits = Lib::MemberFunctionTraits<getter>;
  using Class = typename GetterTraits::ClassType;
  using G = typename std::decay<typename GetterTraits::ReturnType>::type;
  // Recommended: static_assert (std::is_same_v<typename GetterTraits::FuncType, G (Class::*) () const>);
  using SetterTraits = Lib::MemberFunctionTraits<setter>;
  using S = typename std::decay<typename SetterTraits::ReturnType>::type;
  static_assert (std::is_same_v<Class, typename SetterTraits::ClassType>);
  using T = typename std::decay<typename std::tuple_element<1, typename SetterTraits::Arguments>::type>::type;
  // Recommended: static_assert (std::is_same_v<typename SetterTraits::FuncType, void (Class::*) (const T&)>);
  static_assert (std::is_convertible_v<T, G>);
  /// Resolve (or assign) `host->Member` distance (may be 0).
  static Class* host_ (const Member *m, Class *o = nullptr) { static ptrdiff_t d = -1; return Lib::host_member_offset (&d, m, o); }
  /// Retrieve or assign property meta infos.
  static const MemberDetails& meta_ (const MemberDetails *n = nullptr) { static MemberDetails m; return m.assign1 (n); }
public:
  Member (Class *o, const String &n = "", const StringS &s = {}) : Member (o, n, {}, {}, MemberDetails::FLAGS_DEFAULT, s, false) {}
  Member (Class *o, const String &n, const ParamInitialVal &iv, const StringS &s = {}) : Member (o, n, iv, {}, MemberDetails::FLAGS_DEFAULT, s) {}
  Member (Class *o, const String &n, const ParamInitialVal &iv, const ParamExtraVals &ev, const StringS &s = {}) : Member (o, n, iv, ev, MemberDetails::FLAGS_DEFAULT, s) {}
  Member (Class *o, const String &n, const ParamExtraVals &ev, const StringS &s = {}) : Member (o, n, {}, ev, MemberDetails::FLAGS_DEFAULT, s, false) {}
  Member (Class *o, const String &n, const ParamInitialVal &iv, const ParamExtraVals &ev, uint64_t hints, const StringS &s = {}, bool init = true)
  {
    ASE_ASSERT (o);
    MemberDetails meta;
    meta.infos = s;
    if (!n.empty())
      Lib::kvpairs_assign (meta.infos, "ident=" + n);
    meta_ (&meta);
    host_ (this, o);
    constexpr bool has_register_parameter = requires (Class *o, Member *m) { o->_register_parameter (o, m, ev); };
    if constexpr (has_register_parameter)
                   o->_register_parameter (o, this, ev);
    if (!init) return;
    std::visit ([&] (auto &&ival) {
      using T = std::decay_t<decltype (ival)>;
      if constexpr (std::is_convertible_v<T, value_type>)
        set (ival);
      else
        ASE_ASSERT_RETURN (!"initializing");
    }, iv);
  }
  using value_type = T;
  T                     get        () const             { return (host_ (this)->*getter) (); }
  bool                  set        (const T &value)     { if constexpr (!std::is_same_v<S,void>) return !!(host_ (this)->*setter) (value);
                                                          else return (host_ (this)->*setter) (value), true; }
  T                     operator() () const             { return get(); }
  bool                  operator() (const T &value)     { return set (value); }
  /**/                  operator T () const             { return get (); }
  bool                  operator=  (const T &value)     { return set (value); }
  void                  notify     () const             { host_ (this)->emit_notify (info ("ident")); }
  static constexpr bool is_unique_per_member = true; // typeid will uniquely identify a member, due to <setter> arg
  static uint64_t       hints    ()                        { return meta_().flags; }
  static const StringS& infos    ()                        { return meta_().infos; }
  static String         info     (const String &key)       { return meta_().info (key); }
};

/// Member accessor class based on a single accessor, maybe combined with `[[no_unique_address]]`.
template<auto accessor>
class Member<accessor,nullptr> {
public:
  using SetterTraits = Lib::MemberFunctionTraits<accessor>;
  using R = typename std::decay<typename SetterTraits::ReturnType>::type;
  static_assert (std::is_same_v<R, void> || std::is_same_v<R, bool>);
  using Class = typename SetterTraits::ClassType;
  using T = typename std::decay<std::remove_pointer_t<typename std::tuple_element<1, typename SetterTraits::Arguments>::type>>::type;
  static_assert (std::is_same_v<typename SetterTraits::FuncType, R (Class::*) (const T*, T*)>);
  /// Resolve (or assign) `host->Member` distance (may be 0).
  static Class* host_ (const Member *m, Class *o = nullptr) { static ptrdiff_t d = -1; return Lib::host_member_offset (&d, m, o); }
  /// Retrieve or assign property meta infos.
  static const MemberDetails& meta_ (const MemberDetails *n = nullptr) { static MemberDetails m; return m.assign1 (n); }
public:
  Member (Class *o, const String &n = "", const StringS &s = {}) : Member (o, n, {}, MemberDetails::FLAGS_DEFAULT, s) {}
  Member (Class *o, const String &n, const ParamExtraVals &ev, const StringS &s = {}) : Member (o, n, ev, MemberDetails::FLAGS_DEFAULT, s) {}
  Member (Class *o, const String &n, const ParamExtraVals &ev, uint64_t hints, const StringS &s)
  {
    ASE_ASSERT (o);
    MemberDetails meta;
    meta.infos = s;
    if (!n.empty())
      Lib::kvpairs_assign (meta.infos, "ident=" + n);
    meta_ (&meta);
    host_ (this, o);
    constexpr bool has_register_parameter = requires (Class *o, Member *m) { o->_register_parameter (o, m, ev); };
    if constexpr (has_register_parameter)
      o->_register_parameter (o, this, ev);
  }
  using value_type = T;
  T                     get        () const             { T t {}; (host_ (this)->*accessor) (nullptr, &t); return t; }
  bool                  set        (const T &value)     { if constexpr (!std::is_same_v<R,void>) return !!(host_ (this)->*accessor) (&value, nullptr);
                                                          else return (host_ (this)->*accessor) (&value, nullptr), true; }
  T                     operator() () const             { return get(); }
  bool                  operator() (const T &value)     { return set (value); }
  /**/                  operator T () const             { return get (); }
  bool                  operator=  (const T &value)     { return set (value); }
  void                  notify     () const             { host_ (this)->emit_notify (info ("ident")); }
  static constexpr bool is_unique_per_member = true; // typeid will uniquely identify a member, due to <accessor> arg
  static uint64_t       hints ()                        { return meta_().flags; }
  static const StringS& infos ()                        { return meta_().infos; }
  static String         info  (const String &key)       { return meta_().info (key); }
};

} // Ase
