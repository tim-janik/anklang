// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_INIFILE_HH__
#define __ASE_INIFILE_HH__

#include <ase/blob.hh>

namespace Ase {

// == IniFile ==
/// Class to parse INI configuration file sections and values.
class IniFile {
  typedef std::map<String,StringS> SectionMap;
  SectionMap                    sections_;
  void          load_ini        (const String &inputname, const String &data);
  //bool        set             (const String &section, const String &key, const String &value, const String &locale = "");
  //bool        del             (const String &section, const String &key, const String &locale = "*");
  //bool        value           (const String &dotpath, const String &value);
  const StringS& section   (const String &name) const;
public:
  explicit      IniFile         (const String &name, const String &inidata); ///< Load INI file from immediate `data`.
  explicit      IniFile         (Blob blob);                    ///< Load INI file from Blob.
  explicit      IniFile         (const IniFile &source);        ///< Copy constructor.
  IniFile&      operator=       (const IniFile &source);        ///< Assignment operator.
  //String      get             (const String &section, const String &key, const String &locale = "") const;
  bool          has_sections    () const;                       ///< Checks if IniFile is non-empty.
  StringS       sections        () const;                       ///< List all sections.
  bool          has_section     (const String &section) const;  ///< Check presence of a section.
  StringS       attributes      (const String &section) const;  ///< List all attributes available in `section`.
  bool          has_attribute   (const String &section, const String &key) const; ///< Return if `section` contains `key`.
  bool          has_raw_value   (const String &dotpath,
                                 String *valuep = NULL) const;  ///< Check and possibly retrieve raw value if present.
  String        raw_value       (const String &dotpath) const;  ///< Retrieve raw (uncooked) value of section.attribute[locale].
  StringS       raw_values      () const;                       ///< List all section.attribute=value pairs.
  String        value_as_string (const String &dotpath) const;  ///< Retrieve value of section.attribute[locale].
  bool          has_value       (const String &dotpath,
                                 String *valuep = NULL) const;  ///< Check and possibly retrieve value if present.
  static String cook_string     (const String &input_string);   ///< Unquote contents of `input_string`
};


// == IniWriter ==
/// Class to write INI configuration file sections and values.
class IniWriter {
  struct Section {
    String name;
    StringS entries;
  };
  std::vector<Section> sections_;
  Section*      find_section    (String name, bool create);
  size_t        find_entry      (Section &section, String name, bool create);
public:
  void          set             (String key, String value);
  String        output          ();
};

} // Ase

#endif // __ASE_INIFILE_HH__
