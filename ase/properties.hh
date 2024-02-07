// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROPERTIES_HH__
#define __ASE_PROPERTIES_HH__

#include <ase/parameter.hh>
#include <ase/memory.hh>
#include <ase/jsonapi.hh>

namespace Ase {

/// Class for preference parameters (global settings)
class Preference : public ParameterProperty {
  /*ctor*/   Preference (ParameterC parameter);
public:
  using DelCb = std::function<void()>;
  using StringValueF = std::function<void(const String&, const Value&)>;
  virtual   ~Preference ();
  /*ctor*/   Preference (const Param&, const StringValueF& = nullptr);
  String     gets       () const               { return const_cast<Preference*> (this)->get_value().as_string(); }
  bool       getb       () const               { return const_cast<Preference*> (this)->get_value().as_int(); }
  int64      getn       () const               { return const_cast<Preference*> (this)->get_value().as_int(); }
  uint64     getu       () const               { return const_cast<Preference*> (this)->get_value().as_int(); }
  double     getd       () const               { return const_cast<Preference*> (this)->get_value().as_double(); }
  bool       set        (const Value &value)   { return set_value (value); }
  bool       set        (const String &string) { return set_value (string); }
  Value      get_value  () const override;
  bool       set_value  (const Value &v) override;
  static Value       get    (const String &ident);
  static PreferenceP find   (const String &ident);
  static CStringS    list   ();
  static DelCb       listen (const std::function<void(const CStringS&)>&);
  static void save_preferences ();
  static void load_preferences (bool autosave);
private:
  DelCb sigh_;
  Connection *connection_ = nullptr;
  ASE_DEFINE_MAKE_SHARED (Preference);
};

/// Function type for Property value getters.
using PropertyGetter = std::function<void (Value&)>;

/// Function type for Property value setters.
using PropertySetter = std::function<bool (const Value&)>;

/// Function type to list Choice Property values.
using PropertyLister = std::function<ChoiceS (const ParameterProperty&)>;

/// Structured initializer for PropertyImpl
struct Prop {
  PropertyGetter getter;        ///< Lambda implementing the Property value getter.
  PropertySetter setter;        ///< Lambda implementing the Property value setter.
  Param          param;         ///< Parameter meta data for this Property.
  PropertyLister lister;        ///< Lambda providing a list of possible Property value choices.
};

/// Property implementation for GadgetImpl, using lambdas as accessors.
class PropertyImpl : public ParameterProperty {
  PropertyGetter getter_; PropertySetter setter_; PropertyLister lister_;
  PropertyImpl (const Param&, const PropertyGetter&, const PropertySetter&, const PropertyLister&);
public:
  ASE_DEFINE_MAKE_SHARED (PropertyImpl);
  Value   get_value () const override           { Value v; getter_ (v); return v; }
  bool    set_value (const Value &v) override   { return setter_ (v); }
  ChoiceS choices   () const override           { return lister_ ? lister_ (*this) : parameter_->choices(); }
};

/// Helper to simplify property registrations.
struct PropertyBag {
  using RegisterF = std::function<void(const Prop&,CString)>;
  explicit PropertyBag (const RegisterF &f) : add_ (f) {}
  void     operator+= (const Prop&) const;
  CString  group;
private:
  RegisterF add_;
};

/// Value getter for enumeration types.
template<typename Enum> std::function<void(Value&)>
make_enum_getter (Enum *v)
{
  using EnumType = Jsonipc::Enum<Enum>;
  return [v] (Value &val) {
    if (EnumType::has_names())
      {
        const String &name = EnumType::get_name (*v);
        if (!name.empty())
          {
            val = name;
            return;
          }
      }
    val = int64_t (*v);
  };
}

/// Value setter for enumeration types.
template<typename Enum> std::function<bool(const Value&)>
make_enum_setter (Enum *v)
{
  using EnumType = Jsonipc::Enum<Enum>;
  return [v] (const Value &val) {
    Enum e = *v;
    if (val.index() == Value::STRING)
      e = EnumType::get_value (val.as_string(), e);
    else if (val.index() == Value::INT64)
      e = Enum (val.as_int());
    ASE_RETURN_UNLESS (e != *v, false);
    *v = e;
    return true;
  };
}

template<typename T> concept IsEnum = std::is_enum_v<T>;

/// Helper to list Jsonipc::Enum<> type values as Choice.
template<typename Enum> requires IsEnum<Enum>
ChoiceS
enum_lister (const ParameterProperty&)
{
  using EnumType = Jsonipc::Enum<Enum>;
  ChoiceS choices;
  for (const auto &evalue : EnumType::list_values())
    {
      Choice choice (evalue.second, evalue.second);
      choices.push_back (choice);
    }
  return choices;
}

} // Ase

#endif // __ASE_PROPERTIES_HH__
