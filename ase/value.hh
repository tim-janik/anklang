// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_VALUE_HH__
#define __ASE_VALUE_HH__

#include <ase/defs.hh>
#include <variant>

namespace Ase {

struct ValueS : std::vector<ValueP> {
  using ValuePVector = std::vector<ValueP>;
  using ValuePVector::push_back;
  void   push_back (const Value &v)     { push_back (std::make_shared<Value> (v));}
  void   push_back (Value &&v)          { push_back (std::make_shared<Value> (std::move (v)));}
  String repr      () const;
  ValueS ();
  ValueS (std::initializer_list<Value> il);
  static const ValueS empty_array;
  friend bool operator== (const ValuePVector&, const ValuePVector&);
  friend bool operator!= (const ValuePVector &a, const ValuePVector &b) { return !(a == b); }
};

struct ValueField {
  std::string name;
  ValueP value;
  ValueField ();
  ValueField (const String &nam, const Value &val);
  ValueField (const String &nam, Value &&val);
  ValueField (const String &nam, ValueP val);
  bool operator== (const ValueField &other) const;
  bool operator!= (const ValueField &other) const { return !(other == *this); }
};

struct ValueR : std::vector<ValueField> {
  using ValueFieldVector = std::vector<ValueField>;
  using ValueFieldVector::operator[];
  Value&       operator[] (const String &name);
  const Value& operator[] (const String &name) const;
  ValueP       valuep     (const String &name, bool front);
  ValueP       peek       (const String &name) const;
  String       repr       () const;
  ValueR ();
  ValueR (std::initializer_list<ValueField> il);
  static const ValueR empty_record;
};

/// Variant type to hold different types of values.
using ValueVariant = std::variant<std::monostate,bool,int64,double,String,ValueS,ValueR,InstanceP>;

template<size_t I, size_t J> using IDX = typename ::std::enable_if<I == J, bool>::type;
template<class T, class C>   using TYP = typename ::std::enable_if<std::is_same<T, C>::value, bool>::type;

/// Value type used to interface with various property types.
struct Value : ValueVariant {
  using ValueVariant::ValueVariant;
  enum Type { NONE, BOOL, INT64, DOUBLE, STRING, ARRAY, RECORD, INSTANCE };
  constexpr Type index     () const    { return Type (ValueVariant::index()); }
  size_t         count     () const;
  int64          as_int    () const;
  double         as_double () const;
  String         as_string () const;
  const ValueS&  as_array  () const;
  const ValueR&  as_record () const;
  String         repr      () const;
  StringS        keys      () const;
  bool           has       (const String &key) const;
  void           filter    (const std::function<bool (const ValueField&)> &pred);
  bool           is_numeric (bool boolisnumeric = true) const;
  void operator= (bool v)               { ValueVariant::operator= (v); }
  void operator= (int64 v)              { ValueVariant::operator= (v); }
  void operator= (int32 v)              { ValueVariant::operator= (int64 (v)); }
  void operator= (uint32 v)             { ValueVariant::operator= (int64 (v)); }
  void operator= (double v)             { ValueVariant::operator= (v); }
  void operator= (const char *v)        { ValueVariant::operator= (String (v)); }
  void operator= (const String &v)      { ValueVariant::operator= (v); }
  void operator= (const ValueS &v)      { ValueVariant::operator= (v); }
  void operator= (ValueS &&v)           { ValueVariant::operator= (std::move (v)); }
  void operator= (const ValueR &v)      { ValueVariant::operator= (v); }
  void operator= (ValueR &&v)           { ValueVariant::operator= (std::move (v)); }
  void operator= (const InstanceP &v)   { ValueVariant::operator= (v); }
  Value&       operator[] (size_t i);
  const Value& operator[] (size_t i) const;
  Value&       operator[] (const String &name);
  const Value& operator[] (const String &name) const;
  Value ()                      {}
  Value (bool v)                { this->operator= (v); }
  Value (int64 v)               { this->operator= (v); }
  Value (int32 v)               { this->operator= (int64 (v)); }
  Value (uint32 v)              { this->operator= (int64 (v)); }
  Value (double v)              { this->operator= (v); }
  Value (const char *v)         { this->operator= (String (v)); }
  Value (const String &v)       { this->operator= (v); }
  bool operator== (const Value &other) const;
  bool operator!= (const Value &other) const { return !(other == *this); }
  static const Value empty_value;
};

// == EnumInfo ==
/// Get auxiallary enum information.
struct EnumInfo {
  String label;
  String blurb;
  template<class E> static EnumInfo value_info (E value);
  static                   EnumInfo value_info (const std::type_info &enumtype, int64 value);
  template<class E> static bool     impl       (EnumInfo (*enuminfo) (E));
private:
  static void impl (const std::type_info&, const std::function<EnumInfo(int64)>&);
};

// == Event ==
/// Structure for callback based notifications.
struct Event : ValueR {
  String type() const   { return (*this)["type"].as_string(); }
  String detail() const { return (*this)["detail"].as_string(); }
  using ValueR::operator[];
  Event ();
  Event (const String &type, const String &detail, std::initializer_list<ValueField> il = {});
};

// == JsTrigger ==
/// Callback mechanism for Jsonapi/Jsonipc.
class JsTrigger {
  class Impl;
  std::shared_ptr<Impl> p_;
  void             call          (ValueS &&args) const;
  using VoidArgsFunc = std::function<void (ValueS)>;
  using VoidFunc = std::function<void()>;
public:
  static JsTrigger create        (const String &id, const VoidArgsFunc &f);
  String           id            () const;
  void             ondestroy     (const VoidFunc &vf);
  void             destroy       ();
  explicit         operator bool () const noexcept;
  template<class ...A> void
  operator() (const A &...a) const
  {
    ValueS args;
    (args.push_back (a), ...);  // C++17 fold expression
    call (std::move (args));
  }
};

// == Implementations ==
inline bool
Value::operator== (const Value &other) const
{
  const ValueVariant &v1 = *this, &v2 = other;
  return v1 == v2;
}

inline bool
operator== (const std::vector<ValueP> &v1, const std::vector<ValueP> &v2)
{
  if (v1.size() != v2.size())
    return false;
  for (size_t i = 0; i < v1.size(); i++)
    {
      const ValueP p1 = v1[i], p2 = v2[i];
      if (p1 && p2)
        {
          if (*p1 != *p2)
            return false;
        }
      else if (p1 != p2)
        return false;
    }
  return true;
}

inline bool
ValueField::operator== (const ValueField &other) const
{
  if (other.name != name)
    return false;
  else if (value && other.value)
    return *value == *other.value;
  else
    return value == other.value;
}

template<class E> inline EnumInfo
EnumInfo::value_info (E value)
{
  return value_info (typeid (E), int64 (value));
}

template<class E> inline bool
EnumInfo::impl (EnumInfo (*enuminfo) (E))
{
  ASE_ASSERT_RETURN (enuminfo != nullptr, true);
  impl (typeid (E), [enuminfo] (int64 v) { return enuminfo (E (v)); });
  return false;
}

} // Ase

#endif // __ASE_VALUE_HH__
