// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_REGEX_HH__
#define __ASE_REGEX_HH__

#include <ase/cxxaux.hh>

namespace Ase {

/// Some std::regex wrappers to simplify usage and reduce compilation time
namespace Re {

struct MatchObject {
  explicit  operator bool() const       { return have_match_; }
  static MatchObject create (bool b)    { return MatchObject (b); }
private:
  explicit MatchObject (bool b) : have_match_ (b) {}
  const bool have_match_;
};

MatchObject search (const std::string &regex, const std::string &input);
std::string sub    (const std::string &regex, const std::string &subst, const std::string &input);

} // Re
} // Ase

#endif // __ASE_REGEX_HH__
