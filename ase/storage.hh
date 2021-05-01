// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_STORAGE_HH__
#define __ASE_STORAGE_HH__

#include <ase/cxxaux.hh>

namespace Ase {

class Storage {
  class Impl;
  std::shared_ptr<Storage::Impl> impl_;
public:
  explicit Storage           ();
  virtual ~Storage           ();
  // Writer API
  int      store_file_fd     (const String &filename);
  bool     store_file_buffer (const String &filename, const String &buffer, int64_t epoch_seconds = 0);
  bool     rm_file           (const String &filename);
  bool     set_mimetype_ase  ();
  bool     export_as         (const String &filename);
  // Reader API
  bool     import_from       (const String &filename);
  bool     has_file          (const String &filename);
  String   move_to_temporary (const String &filename);
  String   fetch_file_buffer (const String &filename, ssize_t maxlength = -1);
  String   fetch_file        (const String &filename); // yields abspath
  static bool match_ase_header (const String &data);
};

String anklang_cachedir_create  ();
void   anklang_cachedir_cleanup ();
String anklang_cachedir_current ();

} // Ase

#endif // __ASE_STORAGE_HH__
