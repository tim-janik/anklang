// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PATH_HH__
#define __ASE_PATH_HH__

#include <ase/cxxaux.hh>

#ifdef  _WIN32 // includes _WIN64
#define ASE_UNIX_PATHS                  0       ///< Equals 1 _WIN32 and _WIN64 and 0 on Unix.
#define ASE_DOS_PATHS                   1       ///< Equals 0 _WIN32 and _WIN64 and 1 on Unix.
#define ASE_DIRCHAR                     '/'
#define ASE_DIRCHAR2                    '\\'
#define ASE_DIRSEPARATORS               "/\\"
#define ASE_SEARCHPATH_SEPARATOR        ';'
#define ASE_LIBEXT                      ".dll"
#else   // !_WIN32
#define ASE_UNIX_PATHS                  1       ///< Equals 0 _WIN32 and _WIN64 and 1 on Unix.
#define ASE_DOS_PATHS                   0       ///< Equals 1 _WIN32 and _WIN64 and 0 on Unix.
#define ASE_DIRCHAR                     '/'     ///< Platform directory separator character, '/' on Unix-like systems, a '\\' on _WIN32.
#define ASE_DIRCHAR2                    '/'     ///< Secondary directory separator character, '/' on Unix-like systems.
#define ASE_DIRSEPARATORS               "/"     ///< List of platform directory separator characters, "/" on Unix-like systems, "/\\" on _WIN32.
#define ASE_SEARCHPATH_SEPARATOR        ':'     ///< Platform searchpath separator, ':' on Unix-like systems, ';' on _WIN32.
#define ASE_LIBEXT                      ".so"   ///< Dynamic library filename extension on this platform.
#endif  // !_WIN32

namespace Ase {

/// The Path namespace provides functions for file path manipulation and testing.
namespace Path {

String       dirname             (const String &path);
String       basename            (const String &path);
String       realpath            (const String &path);
String       abspath             (const String &path, const String &incwd = "");
bool         isabs               (const String &path);
bool         isdirname           (const String &path);
bool         mkdirs              (const String &dirpath, uint mode = 0750);
StringPair   split_extension     (const std::string &filepath, bool lastdot = false);
String       expand_tilde        (const String &path);
String       user_home           (const String &username = "");
String       xdg_dir             (const String &xdgdir = "");
String       data_home           ();
String       config_home         ();
String       config_names        ();
void         config_names        (const String &names);
String       cache_home          ();
String       runtime_dir         ();
String       config_dirs         ();
String       data_dirs           ();
String       skip_root           (const String &path);
template<class ...S>
String       join                (String path, const S &...more);
bool         check               (const String &file, const String &mode);
bool         equals              (const String &file1, const String &file2);
char*        memread             (const String &filename, size_t *lengthp, ssize_t maxlength = -1);
void         memfree             (char         *memread_mem);
bool         memwrite            (const String &filename, size_t len, const uint8 *bytes);
String       stringread          (const String &filename, ssize_t maxlength = -1);
bool         stringwrite         (const String &filename, const String &data, bool mkdirs = false);
String       cwd                 ();
String       vpath_find          (const String &file, const String &mode = "e");
String       simplify_abspath    (const std::string &abspath_expression);
bool         searchpath_contains (const String &searchpath, const String &element);
String       searchpath_find     (const String &searchpath, const String &file, const String &mode = "e");
StringS      searchpath_list     (const String &searchpath, const String &mode = "e");
String       searchpath_multiply (const String &searchpath, const String &postfixes);
StringS      searchpath_split    (const String &searchpath);
String       searchpath_join     (const StringS &string_vector);
template<class ...S>
String       searchpath_join     (String path, const S &...more);

// == implementations ==
String join_with (const String &head, char joiner, const String &tail);

template<class ...S> inline String
join (String path, const S &...more)
{
  ((path = join_with (path, ASE_DIRCHAR, more)), ...); // C++17 fold expression
  return path;
}

template<class ...S> inline String
searchpath_join (String path, const S &...more)
{
  ((path = join_with (path, ASE_SEARCHPATH_SEPARATOR, more)), ...); // C++17 fold expression
  return path;
}

} // Path
} // Ase

#endif  // __ASE_PATH_HH__
