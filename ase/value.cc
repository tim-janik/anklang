// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "value.hh"
#include "api.hh"
#include "jsonipc/jsonipc.hh"
#include "strings.hh"
#include <cmath>

namespace Ase {

// == Value ==
const Value Value::empty_value;

/// Number of elements in a RECORD or ARRAY Value.
size_t
Value::count () const
{
  switch (index())
    {
    case ARRAY:  { const auto &a = std::get<ValueS> (*this); return a.size(); }
    case RECORD: { const auto &r = std::get<ValueR> (*this); return r.size(); }
    default: ;
    }
  return 0;
}

/// Check for a named field in a RECORD.
bool
Value::has (const String &key) const
{
  if (index() == RECORD)
    for (const auto &field : std::get<ValueR> (*this))
      if (key == field.name)
        return true;
  return false;
}

/// List the field names of a RECORD Value.
StringS
Value::keys () const
{
  StringS kk;
  if (index() == RECORD)
    for (const auto &field : std::get<ValueR> (*this))
      if (field.value)
        kk.push_back (field.name);
  return kk;
}

/// Convert Value to int64 or return 0.
int64
Value::as_int () const
{
  switch (index())
    {
    case BOOL:   return std::get<bool> (*this);
    case INT64:  return std::get<int64> (*this);
    case DOUBLE: return int64 (std::get<double> (*this));
    case STRING: return string_to_int (std::get<String> (*this));
    case ARRAY:  return count();
    case RECORD: return count();
    case INSTANCE: return !!std::get<InstanceP> (*this);
    case NONE: ; // std::monostate
    }
  return 0; // as bool: false
}

/// Convert Value to double or return 0.
double
Value::as_double () const
{
  switch (index())
    {
    case BOOL:   return std::get<bool> (*this);
    case INT64:  return std::get<int64> (*this);
    case DOUBLE: return std::get<double> (*this);
    case STRING: return string_to_double (std::get<String> (*this));
    case ARRAY:  return count();
    case RECORD: return count();
    case INSTANCE: return !!std::get<InstanceP> (*this);
    case NONE: ; // std::monostate
    }
  return 0; // as bool: false
}

/// Convert Value to a string, not very useful for RECORD or ARRAY.
String
Value::as_string () const
{
  switch (index())
    {
    case BOOL:   return std::get<bool> (*this) ? "true" : "false";
    case INT64:  return string_from_int (std::get<int64> (*this));
    case DOUBLE: return string_from_double (std::get<double> (*this));
    case STRING: return std::get<String> (*this);
    case ARRAY:  return count() ? "[...]" : "[]";
    case RECORD: return count() ? "{...}" : "{}";
    case INSTANCE: {
      InstanceP ip = std::get<InstanceP> (*this);
      return ip ? Jsonipc::rtti_typename (*ip) + "{}" : "(Instance*) nullptr";
    }
    case NONE: ; // std::monostate
    }
  return "";
  static_assert (std::is_same<decltype (std::get<NONE>      (std::declval<Value>())), std::monostate&&>::value);
  static_assert (std::is_same<decltype (std::get<BOOL>      (std::declval<Value>())), bool&&>::value);
  static_assert (std::is_same<decltype (std::get<bool>      (std::declval<Value>())), bool&&>::value);
  static_assert (std::is_same<decltype (std::get<INT64>     (std::declval<Value>())), int64&&>::value);
  static_assert (std::is_same<decltype (std::get<int64>     (std::declval<Value>())), int64&&>::value);
  static_assert (std::is_same<decltype (std::get<DOUBLE>    (std::declval<Value>())), double&&>::value);
  static_assert (std::is_same<decltype (std::get<double>    (std::declval<Value>())), double&&>::value);
  static_assert (std::is_same<decltype (std::get<STRING>    (std::declval<Value>())), String&&>::value);
  static_assert (std::is_same<decltype (std::get<String>    (std::declval<Value>())), String&&>::value);
  static_assert (std::is_same<decltype (std::get<ARRAY>     (std::declval<Value>())), ValueS&&>::value);
  static_assert (std::is_same<decltype (std::get<ValueS>    (std::declval<Value>())), ValueS&&>::value);
  static_assert (std::is_same<decltype (std::get<ARRAY>     (std::declval<Value&>())), ValueS&>::value);
  static_assert (std::is_same<decltype (std::get<ValueS>    (std::declval<Value&>())), ValueS&>::value);
  static_assert (std::is_same<decltype (std::get<RECORD>    (std::declval<Value>())), ValueR&&>::value);
  static_assert (std::is_same<decltype (std::get<ValueR>    (std::declval<Value>())), ValueR&&>::value);
  static_assert (std::is_same<decltype (std::get<RECORD>    (std::declval<Value&>())), ValueR&>::value);
  static_assert (std::is_same<decltype (std::get<ValueR>    (std::declval<Value&>())), ValueR&>::value);
  static_assert (std::is_same<decltype (std::get<INSTANCE>  (std::declval<Value>())), InstanceP&&>::value);
  static_assert (std::is_same<decltype (std::get<InstanceP> (std::declval<Value>())), InstanceP&&>::value);
}

/// Retrive a non-empty array if Value contains a non-empty array.
const ValueS&
Value::as_array () const
{
  if (index() == ARRAY)
    return std::get<ValueS> (*this);
  return ValueS::empty_array;
}

const ValueR&
Value::as_record () const
{
  if (index() == RECORD)
    return std::get<ValueR> (*this);
  return ValueR::empty_record;
}

Value&
Value::operator[] (size_t i)
{
  if (index() == ARRAY)
    {
      ValueS &a = std::get<ValueS> (*this);
      if (i < a.size())
        {
          if (!a[i])
            a[i] = std::make_shared<Value>();
          return *a[i];
        }
    }
  if (index() == RECORD)
    {
      ValueR &r = std::get<ValueR> (*this);
      if (i < r.size())
        {
          if (!r[i].value)
            r[i].value = std::make_shared<Value>();
          return *r[i].value;
        }
    }
  throw std::runtime_error ("Invalid Ase::Value index");
}

const Value&
Value::operator[] (size_t i) const
{
  if (index() == ARRAY)
    {
      const ValueS &a = std::get<ValueS> (*this);
      if (i < a.size() && a[i])
        return *a[i];
    }
  if (index() == RECORD)
    {
      const ValueR &r = std::get<ValueR> (*this);
      if (i < r.size() && r[i].value)
        return *r[i].value;
    }
  return empty_value;
}

Value&
Value::operator[] (const String &name)
{
  if (index() == RECORD)
    return std::get<ValueR> (*this)[name];
  throw std::runtime_error ("Invalid Ase::Value index");
}

const Value&
Value::operator[] (const String &name) const
{
  if (index() == RECORD)
    std::get<ValueR> (*this)[name];
  return empty_value;
}

static String
value_array_to_string (const ValueS &vec)
{
  String s;
  for (auto const &valuep : vec)
    {
      const Value &value = valuep ? *valuep : Value::empty_value;
      if (!s.empty())
        s += ",";
      s += value.repr();
    }
  s = "[" + s + "]";
  return s;
}

static String
value_record_to_string (const ValueR &vec)
{
  String s;
  for (auto const &field : vec)
    {
      const Value &value = field.value ? *field.value : Value::empty_value;
      if (!s.empty())
        s += ",";
      s += string_to_cquote (field.name) + ":";
      s += value.repr();
    }
  s = "{" + s + "}";
  return s;
}

/// Convert Value to a string representation, useful for debugging.
String
Value::repr() const
{
  String s;
  switch (index())
    {
    case BOOL:          s += std::get<bool> (*this) ? "true" : "false";                 break;
    case INT64:         s += string_format ("%d", std::get<int64> (*this));             break;
    case DOUBLE:        s += string_format ("%.17g", std::get<double> (*this));         break;
    case STRING:        s += string_to_cquote (std::get<String> (*this));               break;
    case ARRAY:         s += value_array_to_string (std::get<ValueS>(*this));           break;
    case RECORD:        s += value_record_to_string (std::get<ValueR>(*this));          break;
    case INSTANCE:      s += as_string();                                               break;
    case NONE:          s += "null"; /* std::monostate */                               break;
    }
  return s;
}

/// Recursively purge/remove RECORD elements iff to `pred (recordfield) == true`.
void
Value::purge_r (const std::function<bool (const ValueField&)> &pred)
{
  if (index() == Value::ARRAY)
    for (auto &vp : std::get<ValueS> (*this))
      if (vp)
        vp->purge_r (pred);
  if (index() == Value::RECORD)
    {
      ValueR &rec = std::get<ValueR> (*this);
      for (size_t i = rec.size(); i > 0; i--)
        if (pred (rec[i - 1]))
          rec.erase (rec.begin() + i - 1);
        else if (rec[i - 1].value)
          rec[i - 1].value->purge_r (pred);
    }
}

// == ValueS ==
const ValueS ValueS::empty_array;

ValueS::ValueS()
{}

ValueS::ValueS (std::initializer_list<Value> il)
{
  reserve (il.size());
  for (auto &&e : il)
    push_back (std::move (e));
}

String
ValueS::repr() const
{
  return value_array_to_string (*this);
}

// == Event ==
Event::Event ()
{}

Event::Event (const String &type, const String &detail, std::initializer_list<ValueField> il)
{
  reserve (2 + il.size());
  push_back ({ "type", type });
  push_back ({ "detail", detail });
  for (auto &&e : il)
    push_back (std::move (e));
}

// == ValueR ==
const ValueR ValueR::empty_record;

ValueR::ValueR()
{}

ValueR::ValueR (std::initializer_list<ValueField> il)
{
  reserve (il.size());
  for (auto &&e : il)
    push_back (std::move (e));
}

String
ValueR::repr() const
{
  return value_record_to_string (*this);
}

ValueP
ValueR::peek (const String &name) const
{
  for (size_t i = 0; i < size(); i++)
    if (name == (*this)[i].name)
      return (*this)[i].value;
  return nullptr;
}

ValueP
ValueR::valuep (const String &name, bool front)
{
  for (size_t i = 0; i < size(); i++)
    if (name == (*this)[i].name)
      {
        if (!(*this)[i].value)
          (*this)[i].value = std::make_shared<Value>();
        return (*this)[i].value;
      }
  if (front)
    {
      insert (begin(), { name, Value() });
      return begin()->value;
    }
  else
    {
      push_back ({ name, Value() });
      return back().value;
    }
}

Value&
ValueR::operator[] (const String &name)
{
  return *valuep (name, false);
}

const Value&
ValueR::operator[] (const String &name) const
{
  for (size_t i = 0; i < size(); i++)
    if ((*this)[i].value && name == (*this)[i].name)
      return *(*this)[i].value;
  return Value::empty_value;
}

// == ValueField ==
ValueField::ValueField() :
  value (std::make_shared<Value>())
{}

ValueField::ValueField (const String &nam, const Value &val) :
  name (nam), value (std::make_shared<Value> (val))
{}

ValueField::ValueField (const String &nam, Value &&val) :
  name (nam), value (std::make_shared<Value> (std::move (val)))
{}

ValueField::ValueField (const String &nam, ValueP val) :
  name (nam), value (val ? val : std::make_shared<Value>())
{}

// == EnumInfo ==
static std::mutex enuminfo_mutex;
static std::vector<std::pair<const std::type_info*, std::function<EnumInfo(int64)>>> enuminfo_funcs;

/// Find enum info for `value`, MT-Safe.
EnumInfo
EnumInfo::value_info (const std::type_info &enumtype, int64 value)
{
  std::function<EnumInfo(int64)> f;
  {
    std::lock_guard<std::mutex> locker (enuminfo_mutex);
    for (const auto &pair : enuminfo_funcs)
      if (enumtype == *pair.first)
        f = pair.second;
  }
  EnumInfo info;
  if (f)
    info = f (value);
  return info;
}

/// Find enum info for `value`, MT-Safe.
void
EnumInfo::impl (const std::type_info &enumtype, const std::function<EnumInfo(int64)> &fun)
{
  std::lock_guard<std::mutex> locker (enuminfo_mutex);
  enuminfo_funcs.push_back ({ &enumtype, fun });
}

} // Ase
