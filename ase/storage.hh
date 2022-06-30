// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_STORAGE_HH__
#define __ASE_STORAGE_HH__

#include <ase/defs.hh>

namespace Ase {

class Storage {
protected:
  virtual ~Storage () = 0;
public:
  enum StorageFlags {
    NONE      = 0,
    AUTO_ZSTD = 1,
  };
};

class StorageWriter : public Storage {
  class Impl;
  std::shared_ptr<StorageWriter::Impl> impl_;
public:
  explicit StorageWriter      (StorageFlags = StorageFlags::NONE);
  virtual ~StorageWriter      ();
  // Writer API
  Error    open_for_writing   (const String &filename);
  Error    open_with_mimetype (const String &filename, const String &mimetype);
  Error    store_file_data    (const String &filename, const String &buffer, bool alwayscompress = false);
  Error    store_file         (const String &filename, const String &ondiskpath);
  Error    close              ();
  Error    remove_opened      ();
};

class StorageReader : public Storage {
  class Impl;
  std::shared_ptr<StorageReader::Impl> impl_;
public:
  explicit StorageReader      (StorageFlags = StorageFlags::NONE);
  virtual ~StorageReader      ();
  // Reader API
  Error    open_for_reading   (const String &filename);
  void     search_dir         (const String &dirname);
  bool     has_file           (const String &filename);
  StringS  list_files         ();
  String   stringread         (const String &filename, ssize_t maxlength = -1);
  Error    close              ();
};

String anklang_cachedir_create      ();
void   anklang_cachedir_cleanup     (const String &cachedir);
void   anklang_cachedir_clean_stale ();

} // Ase

#endif // __ASE_STORAGE_HH__
