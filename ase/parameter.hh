// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/defs.hh>
#include <ase/memory.hh>
#include <ase/value.hh>

namespace Ase {

/// Min, max, stepping for double ranges.
using MinMaxStep = std::tuple<double,double,double>;

/// Handler to generate all possible parameter choices dynamically.
using ChoicesFunc = std::function<ChoiceS(const CString&)>;

/// Initial value for parameters.
using ParamInitialVal = std::variant<bool,int8_t,uint8_t,int16_t,uint16_t,int32_t,uint32_t,int64_t,uint64_t,float,double,const char*,std::string>;

/// Helper to specify parameter ranges.
struct ParamExtraVals : std::variant<MinMaxStep,ChoiceS,ChoicesFunc> {
  ParamExtraVals () = default;
  ParamExtraVals (double vmin, double vmax, double step = 0);
  ParamExtraVals (const MinMaxStep&);
  ParamExtraVals (const std::initializer_list<Choice>&);
  ParamExtraVals (const ChoiceS&);
  ParamExtraVals (const ChoicesFunc&);
};

/// Structured initializer for Parameter
struct Param {
  using InitialVal = ParamInitialVal;
  using ExtraVals = ParamExtraVals;
  String     ident;       ///< Identifier used for serialization (can be derived from untranslated label).
  String     label;       ///< Preferred user interface name.
  String     nick;        ///< Abbreviated user interface name, usually not more than 6 characters.
  InitialVal initial = 0; ///< Initial value for float, int, choice types.
  String     unit;        ///< Units of the values within range.
  ExtraVals  extras;      ///< Min, max, stepping for double ranges or array of choices to select from.
  String     hints;       ///< Hints for parameter handling.
  StringS    metadata;    ///< Array of "key=value" pairs.
  String     fetch        (const String &key) const;
  void       store        (const String &key, const String &v);
  static inline const String STORAGE = ":r:w:S:";
  static inline const String STANDARD = ":r:w:S:G:";
};

/// Structure to provide information about properties or preferences.
struct Parameter {
  CString       cident;
  bool          has         (const String &key) const;
  String        fetch       (const String &key) const;
  void          store       (const String &key, const String &value);
  String        ident       () const   { return cident; }
  String        label       () const   { return fetch ("label"); }
  String        nick        () const;
  String        unit        () const   { return fetch ("unit"); }
  String        hints       () const   { return fetch ("hints"); }
  String        blurb       () const   { return fetch ("blurb"); }
  String        descr       () const   { return fetch ("descr"); }
  String        group       () const   { return fetch ("group"); }
  Value         initial     () const   { return initial_; }
  bool          has_hint    (const String &hint) const;
  ChoiceS       choices     () const;
  const StringS metadata    () const   { return metadata_; }
  MinMaxStep    range       () const;   ///< Min, max, stepping for double ranges.
  bool          is_numeric  () const;
  bool          is_choice   () const   { return has_hint ("choice"); }
  bool          is_text     () const   { return has_hint ("text"); }
  double        normalize   (double val) const;
  double        rescale     (double t) const;
  Value         constrain   (const Value &value) const;
  double        dconstrain  (const Value &value) const;
  void          initialsync (const Value &v);
  /*ctor*/      Parameter   () = default;
  /*ctor*/      Parameter   (const Param&);
  /*copy*/      Parameter   (const Parameter&) = default;
  Parameter&    operator=   (const Parameter&) = default;
  // helpers
  String    value_to_text   (const Value &value) const;
  Value     value_from_text (const String &text) const;
  static String construct_hints (const String &hints, const String &more, double pmin = 0, double pmax = 0);
  static size_t match_choice    (const ChoiceS &choices, const String &text);
private:
  using ExtrasV = std::variant<MinMaxStep,ChoiceS,ChoicesFunc>;
  StringS       metadata_;
  ExtrasV       extras_;
  Value         initial_ = 0;
};
using ParameterC = std::shared_ptr<const Parameter>;

/// Parameter list construction helper.
struct ParameterMap final : std::map<uint32_t,ParameterC> {
  /// Group to be applied to all newly inserted Parameter objects.
  String group;
  /// Helper for new Parameter creation from Param initializer.
  struct Entry {
    ParameterMap &map;
    const uint32_t id;
    void operator= (const Param&);
  };
  /// Slot subscription for new Parameter creation.
  Entry operator[] (uint32_t id);
};

/// Find a suitable 3-letter abbreviation for a Parameter without nick.
String parameter_guess_nick (const String &parameter_label);

} // Ase

