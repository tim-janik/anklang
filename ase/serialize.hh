// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_SERIALIZE_HH__
#define __ASE_SERIALIZE_HH__

#include <ase/value.hh>
#include <ase/strings.hh>
#include <ase/jsonapi.hh>

namespace Ase {

class Writ;
using WritP = std::shared_ptr<Writ>;

// == WritLink ==
class WritLink {
  Serializable **spp_;
  friend class WritNode;
public:
  explicit WritLink (Serializable **spp);
};

// == WritNode ==
/// One entry in a Writ serialization document.
class WritNode {
  Writ  &writ_;
  ValueP valuep_;
  Value &value_;
  friend class Writ;
  ValueP       dummy            ();
  WritNode     recfield         (const String &fieldname, bool front);
public:
  /*ctor*/     WritNode         (Writ &writ, ValueP vp = std::make_shared<Value> (Value::empty_value));
  WritNode     operator[]       (const String &fieldname)  { return recfield (fieldname, false); }
  WritNode     front            (const String &fieldname)  { return recfield (fieldname, true); }
  bool         in_load          () const;           ///< Return `true` during deserialization
  bool         in_save          () const;           ///< Return `true` during serialization
  bool         skip_emptystring () const;           ///< Omit empty strings during in_save()
  bool         skip_zero        () const;           ///< Omit zero integers or floats during in_save()
  Writ&        writ             ();                 ///< Access the Writ of this node.
  Value&       value            ();                 ///< Access the Value of this node.
  void         purge_value      ();                 ///< Clean up defaults in Value.
  Value::Type  index            () const                   { return value_.index(); }
  size_t       count            () const                   { return value_.count(); }
  int64        as_int           () const                   { return value_.as_int(); }
  double       as_double        () const                   { return value_.as_double(); }
  String       as_string        () const                   { return value_.as_string(); }
  WritNodeS    to_nodes         ();
  WritNode     push             ();
  String       repr             () const                   { return value_.repr(); }
  StringS      keys             () const                   { return value_.keys(); }
  bool         has              (const String &key) const  { return value_.has (key); }
  bool         loadable         (const String &key) const;  ///< True if `in_load() && has (key)`.
  template<typename T> bool operator<< (const T &v);
  template<typename T> bool operator>> (T &v);
  template<typename T> bool operator&  (T &v);
  bool                      operator&  (const WritLink &l);
  template<class T, class E = void>
  bool     serialize (T&, const String& = "", const StringS& = StringS());
  template<class T>
  bool     serialize (std::vector<T> &vec, const String& = "", const StringS& = StringS());
  bool     serialize (ValueS &vec, const String& = "", const StringS& = StringS());
  bool     serialize (ValueR &rec, const String& = "", const StringS& = StringS());
  bool     serialize (Value &val, const String& = "", const StringS& = StringS());
  bool     serialize (Serializable &sobj);
  template<class ...T>
  bool     serialize (std::tuple<T...> &tup, const String& = "", const StringS& = StringS());
};

// == Writ ==
/// Document containing all information needed to serialize and deserialize a Value.
class Writ {
  WritNode root_;
  bool in_load_ = false, in_save_ = false, skip_zero_ = false, skip_emptystring_ = false, relaxed_ = false;
  ValueP dummy_;
  struct InstanceMap : Jsonipc::InstanceMap {
    Jsonipc::JsonValue wrapper_to_json   (Wrapper*, size_t, const std::string&, Jsonipc::JsonAllocator&) override;
    Wrapper*           wrapper_from_json (const Jsonipc::JsonValue&) override;
  }      instance_map_;
  struct LinkEntry { ValueP value; Serializable *sp = nullptr; int64 id = 0; };
  struct LinkPtr   { Serializable **spp = nullptr; int64 id = 0; };
  std::vector<LinkEntry> links_;
  int64                  link_counter_ = 0;
  std::vector<LinkPtr>   linkptrs_;
  ValueP                 dummy        ();
  void                   reset        (int mode);
  void                   prepare_link (Serializable &serializable, ValueP valuep);
  int64                  use_link     (Serializable &serializable);
  void                   insert_links ();
  void                   collect_link (int64 id, Serializable &serializable);
  Serializable*          resolve_link (int64 id);
  void                   assign_links ();
  friend class WritNode;
public:
  enum Flags { RELAXED = 1, SKIP_ZERO = 2, SKIP_EMPTYSTRING = 4, SKIP_DEFAULTS = SKIP_ZERO | SKIP_EMPTYSTRING };
  friend Flags operator| (Flags a, Flags b) { return Flags (uint64_t (a) | b); }
  explicit               Writ         (Flags flags = Flags (0));
  template<class T> void save         (T &source);
  template<class T> bool load         (T &target);
  String                 to_json      ();
  bool                   from_json    (const String &jsonstring);
  bool                   in_load      () const { return in_load_; } ///< Return `true` during deserialization
  bool                   in_save      () const { return in_save_; } ///< Return `true` during serialization
  static void blank_enum            (const String &enumname);
  static void not_serializable      (const String &classname);
  static bool typedata_is_loadable  (const StringS &typedata, const String &fieldname);
  static bool typedata_is_storable  (const StringS &typedata, const String &fieldname);
  static bool typedata_find_minimum (const StringS &typedata, const std::string &fieldname, long double *limit);
  static bool typedata_find_maximum (const StringS &typedata, const std::string &fieldname, long double *limit);
};

// == JSON API ==
/// Create JSON string from `source`.
template<class T> String json_stringify (const T &source, Writ::Flags flags = Writ::Flags (0));

/// Parse a well formed JSON string and assign contents to `target`.
template<class T> bool   json_parse     (const String &jsonstring, T &target);

/// Parse a well formed JSON string and return the resulting value.
template<class T> T      json_parse     (const String &jsonstring);

// == WritConverter ==
/// Template to specialize string conversion for various data types.
template<typename T, typename = void>
struct WritConverter {
  static_assert (!sizeof (T), "ENOIMPL: type serialization unimplemented");
  // bool save_value (WritNode node, const T&, const StringS&, const String&);
  // bool load_value (WritNode node, T&, const StringS&, const String&);
};

// == Implementations ==
/// Has_serialize_f<T> - Check if `serialize(T&,WritNode&)` is provided for `T`.
template<class, class = void> struct
Has_serialize_f : std::false_type {};
template<class T> struct
Has_serialize_f<T, void_t< decltype (serialize (std::declval<T&>(), std::declval<WritNode&>())) > > : std::true_type {};

template<class T> inline void
Writ::save (T &source)
{
  reset (2);
  Jsonipc::Scope scope (instance_map_);
  if constexpr (std::is_base_of<Serializable, T>::value)
    {
      Serializable &serializable = source;
      root_ & serializable;
    }
  else
    root_ & source;
  insert_links();
}

template<class T> inline bool
Writ::load (T &target)
{
  ASE_ASSERT_RETURN (in_load(), false);
  Jsonipc::Scope scope (instance_map_);
  if constexpr (std::is_base_of<Serializable, T>::value)
    {
      Serializable &serializable = target;
      root_ & serializable;
    }
  else
    root_ & target;
  assign_links();
  return true;
}

inline ValueP
WritNode::dummy ()
{
  return writ_.dummy();
}

inline bool
WritNode::in_load () const
{
  return writ_.in_load();
}

inline bool
WritNode::in_save () const
{
  return writ_.in_save();
}

inline bool
WritNode::skip_emptystring() const
{
  return writ_.skip_emptystring_;
}

inline bool
WritNode::skip_zero() const
{
  return writ_.skip_zero_;
}

inline Value&
WritNode::value ()
{
  return value_;
}

inline Writ&
WritNode::writ ()
{
  return writ_;
}

inline bool
WritNode::loadable (const String &key) const
{
  return in_load() && has (key);
}

/// Serialization operator
template<typename T> inline bool
WritNode::operator& (T &v)
{
  static_assert (!std::is_const<T>::value, "serializable type <T> may not be const");
  return serialize (v, "");
}

template<typename T> inline bool
WritNode::operator<< (const T &v)
{
  ASE_ASSERT_RETURN (in_save(), false);
  return serialize (const_cast<T&> (v), "");
}

template<typename T> inline bool
WritNode::operator>> (T &v)
{
  ASE_ASSERT_RETURN (in_load(), false);
  static_assert (!std::is_const<T>::value, "serializable type <T> may not be const");
  return serialize (v, "");
}

template<typename T, typename> inline bool
WritNode::serialize (T &typval, const String &fieldname, const StringS &typedata)
{
  if (in_save())
    return WritConverter<T>::save_value (*this, typval, typedata, fieldname);
  if (in_load())
    return WritConverter<T>::load_value (*this, typval, typedata, fieldname);
  return false;
}

template<> inline bool
WritNode::serialize (String &string, const String &fieldname, const StringS &typedata)
{
  if (in_save() && Writ::typedata_is_storable (typedata, fieldname))
    {
      value_ = string;
      return true;
    }
  if (in_load() && Writ::typedata_is_loadable (typedata, fieldname))
    {
      string = value_.as_string();
      return true;
    }
  return false;
}

template<class T> inline bool
WritNode::serialize (std::vector<T> &vec, const String &fieldname, const StringS &typedata)
{
  if (in_save() && Writ::typedata_is_storable (typedata, fieldname))
    {
      value_ = ValueS();
      ValueS &array = std::get<ValueS> (value_);
      array.reserve (vec.size());
      for (auto &el : vec)
        {
          array.push_back (Value());
          WritNode nth_node (writ_, array.back());
          nth_node & el;
        }
      return true;
    }
  if (in_load() && Writ::typedata_is_loadable (typedata, fieldname) &&
      value_.index() == Value::ARRAY)
    {
      ValueS &array = std::get<ValueS> (value_);
      vec.reserve (vec.size() + array.size());
      for (size_t i = 0; i < array.size(); i++)
        {
          vec.resize (vec.size() + 1);
          WritNode nth_node (writ_, array[i]);
          nth_node & vec.back();
        }
      return true;
    }
  return false;
}

template<class ...T> inline bool
WritNode::serialize (std::tuple<T...> &tuple, const String &fieldname, const StringS &typedata)
{
  using Tuple = std::tuple<T...>;
  if (in_save() && Writ::typedata_is_storable (typedata, fieldname))
    {
      value_ = ValueS();
      ValueS &array = std::get<ValueS> (value_);
      array.reserve (std::tuple_size_v<Tuple>);
      auto save_arg = [&] (auto arg) {
        array.push_back (Value());
        WritNode nth_node (writ_, array.back());
        nth_node & arg;
      };
      std::apply ([&] (auto &&...parampack) {
        (save_arg (parampack), ...); // fold expression to unpack parampack
      }, tuple);
      return true;
    }
  if (in_load() && Writ::typedata_is_loadable (typedata, fieldname) &&
      value_.index() == Value::ARRAY)
    {
      ValueS &array = std::get<ValueS> (value_);
      size_t idx = 0;
      auto load_arg = [&] (auto &arg) {
        if (idx >= array.size())
          return;
        WritNode nth_node (writ_, array[idx++]);
        nth_node & arg;
      };
      std::apply ([&] (auto &...parampack) {
        (load_arg (parampack), ...); // fold expression to unpack parampack
      }, tuple);
      return true;
    }
  return false;
}

inline bool
WritNode::serialize (Value &val, const String &fieldname, const StringS &typedata)
{
  if (in_save() && Writ::typedata_is_storable (typedata, fieldname))
    {
      value_ = val;
      return true;
    }
  if (in_load() && Writ::typedata_is_loadable (typedata, fieldname))
    {
      val = value_;
      return true;
    }
  return false;
}

inline bool
WritNode::serialize (ValueS &vec, const String &fieldname, const StringS &typedata)
{
  if (in_save() && Writ::typedata_is_storable (typedata, fieldname))
    {
      value_ = vec;
      return true;
    }
  if (in_load() && Writ::typedata_is_loadable (typedata, fieldname) &&
      value_.index() == Value::ARRAY)
    {
      vec = std::get<ValueS> (value_);
      return true;
    }
  return false;
}

inline bool
WritNode::serialize (ValueR &rec, const String &fieldname, const StringS &typedata)
{
  if (in_save() && Writ::typedata_is_storable (typedata, fieldname))
    {
      value_ = rec;
      return true;
    }
  if (in_load() && Writ::typedata_is_loadable (typedata, fieldname) &&
      value_.index() == Value::RECORD)
    {
      rec = std::get<ValueR> (value_);
      return true;
    }
  return false;
}

// == WritConverter int + float ==
template<typename T>
struct WritConverter<T, REQUIRESv< std::is_integral<T>::value || std::is_floating_point<T>::value > >
{
  static bool ASE_NOINLINE
  save_value (WritNode node, T i, const StringS &typedata, const String &fieldname)
  {
    node.value() = i;
    return true;
  }
  static bool ASE_NOINLINE
  load_value (WritNode node, T &v, const StringS &typedata, const String &fieldname)
  {
    if constexpr (!std::is_const<T>::value) // ignore loading of const types
     {
       T tmp = {};
       if constexpr (std::is_integral<T>::value)
                      tmp = node.value().as_int();
       else
         tmp = node.value().as_double();
       bool valid = true;
       long double limit = 0;
       if (Writ::typedata_find_minimum (typedata, fieldname, &limit))
         valid = valid && tmp >= limit;
       if (Writ::typedata_find_maximum (typedata, fieldname, &limit))
         valid = valid && tmp <= limit;
       if (valid)
         {
           v = tmp;
           return true;
         }
     }
    return false;
  }
};

// == WritConverter Jsonipc::Enum ==
template<typename T>
struct WritConverter<T, REQUIRESv< std::is_enum<T>::value > >
{
  static void
  check ()
  {
    if (!Jsonipc::Enum<T>::has_names())
      Writ::blank_enum (typeid_name<T>());
  }
  static bool
  save_value (WritNode node, T i, const StringS &typedata, const String &fieldname)
  {
    check();
    // node.value() = int64 (i);
    node.value() = Jsonipc::Enum<T>::get_name (i);
    return true;
  }
  static bool
  load_value (WritNode node, T &v, const StringS &typedata, const String &fieldname)
  {
    check();
    // v = T (node.value().as_int());
    if (node.value().index() == Value::STRING)
      {
        v = Jsonipc::Enum<T>::get_value (node.value().as_string(), T{});
        return true;
      }
    return false;
  }
};

// == WritConverter Jsonipc::Serializable ==
template<typename T>
struct WritConverter<T, REQUIRESv< !std::is_base_of<Serializable,T>::value &&
                                   !Jsonipc::DerivesVector<T>::value &&
                                   !Jsonipc::DerivesSharedPtr<T>::value &&
                                   std::is_class<T>::value > >
{
  static bool
  save_value (WritNode node, T &obj, const StringS &typedata, const String &fieldname)
  {
    if (!Jsonipc::Serializable<T>::is_serializable())
      {
        if constexpr (Has_serialize_f<T>::value)
          {
            ASE_ASSERT_RETURN (node.value().index() == Value::NONE, false);
            node.value() = ValueR::empty_record;        // linking not supported
            serialize (obj, node);
            node.purge_value();
            return true;
          }
        Writ::not_serializable (typeid_name<T>());
        return false;
      }
    rapidjson::Document document (rapidjson::kNullType);
    Jsonipc::JsonValue &docroot = document;
    docroot = Jsonipc::Serializable<T>::serialize_to_json (obj, document.GetAllocator()); // move semantics!
    node.value() = Jsonipc::from_json<Value> (docroot);
    node.purge_value();
    return true;
  }
  static bool
  load_value (WritNode node, T &obj, const StringS &typedata, const String &fieldname)
  {
    if (!Jsonipc::Serializable<T>::is_serializable())
      {
        if constexpr (Has_serialize_f<T>::value)
          {
            if (node.value().index() == Value::RECORD)
              serialize (obj, node);                    // linking not supported
            return true;
          }
        Writ::not_serializable (typeid_name<T>());
        return false;
      }
    rapidjson::Document document (rapidjson::kNullType);
    Jsonipc::JsonValue &docroot = document;
    docroot = Jsonipc::to_json<Value> (node.value(), document.GetAllocator()); // move semantics!
    std::shared_ptr<T> target { &obj, [] (T*) {} }; // dummy shared_ptr with NOP deleter
    if (Jsonipc::Serializable<T>::serialize_from_json (docroot, target))
      return true;
    return false;
  }
};

// == WritConverter Ase::Serializable ==
template<typename T>
struct WritConverter<T, REQUIRESv< std::is_base_of<Serializable,T>::value > >
{
  static bool
  save_value (WritNode node, Serializable &serializable, const StringS &typedata, const String &fieldname)
  {
    return node.serialize (serializable);
  }
  static bool
  load_value (WritNode node, Serializable &serializable, const StringS &typedata, const String &fieldname)
  {
    if (node.value().index() == Value::RECORD)
      {
        node.serialize (serializable);
        return true;
      }
    return false;
  }
};

// == JSON ==
template<class T> inline String
json_stringify (const T &source, Writ::Flags flags)
{
  Writ writ (flags);
  writ.save (const_cast<T&> (source));
  return writ.to_json();
}

template<class T> inline bool
json_parse (const String &jsonstring, T &target)
{
  Writ writ;
  if (writ.from_json (jsonstring) && writ.load (target))
    return true;
  return false;
}

template<class T> inline T
json_parse (const String &jsonstring)
{
  Writ writ;
  if (writ.from_json (jsonstring))
    {
      T target = {};
      if (writ.load (target))
        return target;
    }
  return {};
}

} // Ase

#endif // __ASE_SERIALIZE_HH__
