// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROPERTIES_HH__
#define __ASE_PROPERTIES_HH__

#include <ase/api.hh>
#include <ase/object.hh>
#include <ase/jsonapi.hh>

namespace Ase {

/// Abstract base type for Property implementations with Parameter meta data.
class ParameterProperty : public EmittableImpl, public virtual Property {
protected:
  ParameterC parameter_;
  StringS    metadata       () const override     { return parameter_->metadata(); }
public:
  String     ident          () const override     { return parameter_->cident; }
  String     label          () const override     { return parameter_->label(); }
  String     nick           () const override     { return parameter_->nick(); }
  String     unit           () const override     { return parameter_->unit(); }
  double     get_min        () const override     { return std::get<0> (parameter_->range()); }
  double     get_max        () const override     { return std::get<1> (parameter_->range()); }
  double     get_step       () const override     { return std::get<2> (parameter_->range()); }
  bool       is_numeric     () const override     { return parameter_->is_numeric(); }
  ChoiceS    choices        () const override     { return parameter_->choices(); }
  void       reset          () override           { value (parameter_->initial()); }
  double     get_normalized () const override     { return !is_numeric() ? 0 : parameter_->normalize (value_as_double()); }
  bool       set_normalized (double v) override   { return is_numeric() && value (parameter_->rescale (v)); }
  String     get_text       () const override     { return parameter_->value_to_text (value()); }
  bool       set_text       (String txt) override { value (parameter_->value_from_text (txt)); return !txt.empty(); }
  Value      value          () const override = 0;
  bool       value          (const Value &v) override = 0;
  double     get_double     () const              { return !is_numeric() ? 0 : value().as_double(); }
  ParameterC parameter      () const              { return parameter_; }
  Value      initial        () const              { return parameter_->initial(); }
  MinMaxStep range          () const              { return parameter_->range(); }
  double     value_as_double () const             { return value().as_double(); }
};

/// Class for preference parameters (global settings)
class Preference : public ParameterProperty {
  /*ctor*/   Preference (ParameterC parameter);
public:
  using DelCb = std::function<void()>;
  using StringValueF = std::function<void(const String&, const Value&)>;
  virtual   ~Preference ();
  /*ctor*/   Preference (const Param&, const StringValueF& = nullptr);
  String     gets       () const               { return const_cast<Preference*> (this)->value().as_string(); }
  bool       getb       () const               { return const_cast<Preference*> (this)->value().as_int(); }
  int64      getn       () const               { return const_cast<Preference*> (this)->value().as_int(); }
  uint64     getu       () const               { return const_cast<Preference*> (this)->value().as_int(); }
  double     getd       () const               { return const_cast<Preference*> (this)->value().as_double(); }
  bool       set        (const Value &v)       { return value (v); }
  bool       set        (const String &s)      { return value (s); }
  Value      value      () const override;
  bool       value      (const Value &v) override;
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

/// Property implementation for GadgetImpl, using lambdas as accessors.
class PropertyImpl : public ParameterProperty {
  PropertyGetter getter_; PropertySetter setter_; PropertyLister lister_;
  PropertyImpl (const Param&, const PropertyGetter&, const PropertySetter&, const PropertyLister&);
public:
  ASE_DEFINE_MAKE_SHARED (PropertyImpl);
  Value   value () const override           { Value v; getter_ (v); return v; }
  bool    value (const Value &v) override   { return setter_ (v); }
  ChoiceS choices   () const override           { return lister_ ? lister_ (*this) : parameter_->choices(); }
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
