// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include "cxxaux.hh"

namespace Ase {

// Damerau-Levenshtein Distance with restricted transposition, memory requirement: 12 * max(|source|,|target|) + constant
float   damerau_levenshtein_restricted  (const std::string &source, const std::string &target,
                                         const float ci = 1,  // insertion
                                         const float cd = 1,  // deletion
                                         const float cs = 1,  // substitution
                                         const float ct = 1); // transposition

// Damerau-Levenshtein Distance with unrestricted transpositions, memory requirement: 4 * |source|*|target| + constant
float   damerau_levenshtein_distance    (const std::string &source, const std::string &target,
                                         const float ci = 1,  // insertion
                                         const float cd = 1,  // deletion
                                         const float cs = 1,  // substitution
                                         const float ct = 1); // transposition

} // Ase

