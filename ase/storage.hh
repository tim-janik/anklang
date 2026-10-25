// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/defs.hh>

namespace Ase {

class StorageWriter {
  class Impl;
  std::shared_ptr<StorageWriter::Impl> impl_;
public:
  explicit StorageWriter      ();
  virtual ~StorageWriter      ();
  // Writer API
  Error    open_for_writing   (const String &filename);
  Error    open_with_mimetype (const String &filename, const String &mimetype);
  Error    store_file_data    (const String &filename, const String &buffer);
  Error    store_file         (const String &filename, const String &ondiskpath, bool maycompress = true, bool follow_links = false);
  Error    close              ();
  Error    remove_opened      ();
};

class StorageReader {
  class Impl;
  std::shared_ptr<StorageReader::Impl> impl_;
public:
  explicit StorageReader      ();
  virtual ~StorageReader      ();
  // Reader API
  Error    open_for_reading   (const String &filename);
  void     search_dir         (const String &dirname);
  bool     has_file           (const String &filename);
  StringS  list_files         ();
  String   stringread         (const String &filename, ssize_t maxlength = -1);
  String   read_link          (const String &filename);
  Error    close              ();
};

class StreamReader {
public:
  virtual                ~StreamReader ();
  virtual String          name  () const = 0;
  virtual ssize_t         read  (void *buffer, size_t len) = 0;
  virtual bool            close () = 0;
  static constexpr size_t buffer_size = 131072; ///< Recommended buffer size.
};

StreamReaderP stream_reader_from_file  (const String &file);
StreamReaderP stream_reader_zip_member (const String &archive, const String &member);

class StreamWriter {
public:
  virtual                ~StreamWriter ();
  virtual String          name  () const = 0;
  virtual ssize_t         write (const void *buffer, size_t len) = 0;
  virtual bool            close () = 0;
  static constexpr size_t buffer_size = 131072; ///< Recommended buffer size.
};

StreamWriterP stream_writer_create_file (const String &filename, int mode = 0644);

String anklang_cachedir_create      ();
void   anklang_cachedir_cleanup     (const String &cachedir);
void   anklang_cachedir_clean_stale ();

} // Ase

