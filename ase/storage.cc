// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "storage.hh"
#include "path.hh"
#include "utils.hh"
#include "api.hh"
#include "compress.hh"
#include "minizip.h"
#include "internal.hh"
#include <stdlib.h>     // mkdtemp
#include <sys/stat.h>   // mkdir
#include <unistd.h>     // rmdir
#include <fcntl.h>      // O_EXCL
#include <filesystem>

#define SDEBUG(...)     Ase::debug ("storage", __VA_ARGS__)
#define return_with_errno(ERRNO, RETVAL)        ({ errno = ERRNO; return RETVAL; })

namespace Ase {

/// Create process specific string for a `.pid` guard file.
static String
pid_string (int pid)
{
  static String boot_id = []() {
    auto id = string_strip (Path::stringread ("/proc/sys/kernel/random/boot_id"));
    if (id.empty())
      id = string_strip (Path::stringread ("/etc/machine-id"));
    if (id.empty())
      id = string_format ("%08x", gethostid());
    return id;
  } ();
  String text = string_format ("%u %s ", pid, boot_id);
  String exename;
  if (exename.empty() && Path::check ("/proc/self/exe", "r"))
    {
      const ssize_t max_size = 8100;
      char exepath[max_size + 1 + 1] = { 0, };
      ssize_t exepath_size = -1;
      exepath_size = readlink (string_format ("/proc/%u/exe", pid).c_str(), exepath, max_size);
      if (exepath_size > 0)
        exename = exepath;
    }
  else if (exename.empty() && Path::check ("/proc/self/comm", "r"))
    exename = Path::stringread (string_format ("/proc/%u/comm", pid));
  else
    {
      if (getpgid (pid) >= 0 || errno != ESRCH)
        exename = string_format ("%u", pid); // assume process `pid` exists
    }
  text += exename;
  text += "\n";
  return text;
}

/// Prefix for temporary cache directories, also used for pruning of stale directories.
static String
tmpdir_prefix ()
{
  return string_format ("anklang-%x", getuid());
}

/// Find base directory for the creation of temporary caches.
static String
anklang_cachedir_base (bool createbase = false)
{
  // try ~/.cache/anklang/
  String basedir = Ase::Path::cache_home() + "/anklang";
  if (Ase::Path::check (basedir, "dw"))
    return basedir;
  else if (createbase) // !Ase::Path::check (basedir, "dw")
    {
      int err = mkdir (basedir.c_str(), 0700);
      SDEBUG ("mkdir: %s: %s", basedir, strerror (err ? errno : 0));
      if (Ase::Path::check (basedir, "dw"))
        return basedir;
    }
  // try /tmp/
  basedir = std::filesystem::temp_directory_path().string();
  if (Ase::Path::check (basedir, "dw")) // sets errno
    return basedir;
  return "";
}

/// Recursively delete directory tree.
static void
rmrf_dir (const String &dir)
{
  std::error_code ec;
  std::filesystem::remove_all (dir, ec);
  SDEBUG ("rm-rf: %s: %s", dir, ec.message());
}

static std::vector<String> cachedirs_list;
static std::mutex               cachedirs_mutex;

/// Clean temporary caches of this process.
static void
atexit_clean_cachedirs()
{
  std::lock_guard<std::mutex> locker (cachedirs_mutex);
  while (cachedirs_list.size())
    {
      const String dir = cachedirs_list.back();
      cachedirs_list.pop_back();
      rmrf_dir (dir);
    }
}

/// Create exclusive cache directory for this process' runtime.
String
anklang_cachedir_create()
{
  String cachedir = anklang_cachedir_base (true); // sets errno
  if (cachedir.empty())
    return "";
  cachedir += "/" + tmpdir_prefix() + "XXXXXX";
  char *tmpchars = cachedir.data();
  char *result = mkdtemp (tmpchars);
  SDEBUG ("mkdtemp: %s: %s", tmpchars, strerror (result ? 0 : errno));
  if (result)
    {
      String guardfile = cachedir + "/guard.pid";
      String guardstring = pid_string (getpid());
      const int guardfd = open (guardfile.c_str(), O_EXCL | O_CREAT, 0600);
      if (guardfd >= 0 && Path::stringwrite (guardfile, guardstring))
        {
          close (guardfd);
          std::lock_guard<std::mutex> locker (cachedirs_mutex);
          cachedirs_list.push_back (cachedir);
          static bool needatexit = true;
          if (needatexit)
            needatexit = std::atexit (atexit_clean_cachedirs);
          SDEBUG ("create: %s: %s", guardfile, strerror (0));
          return cachedir;
        }
      SDEBUG ("create: %s: %s", guardfile, strerror (errno));
      const int err = errno;
      close (guardfd);
      rmrf_dir (cachedir);
      errno = err;
    }
  return ""; // errno is set
}

/// Retrieve (or create) the temporary cache directory for this runtime.
String
anklang_cachedir_current ()
{
  static String current_cachedir = anklang_cachedir_create();
  if (current_cachedir.empty())
    fatal_error ("failed to create temporary cache directory: %s", strerror (errno));
  return current_cachedir;
}

/// Clean stale cache directories from past runtimes, may be called from any thread.
void
anklang_cachedir_cleanup()
{
  const String cachedir = anklang_cachedir_base (false);
  const String tmpprefix = tmpdir_prefix();
  if (!cachedir.empty())
    for (auto &direntry : std::filesystem::directory_iterator (cachedir))
      if (direntry.is_directory())
        {
          const String dirname = direntry.path().filename();
          if (dirname.size() == tmpprefix.size() + 6 && string_startswith (dirname, tmpprefix))
            {
              const String guardfile = direntry.path() / "guard.pid";
              if (Path::check (guardfile, "frw"))
                {
                  String guardstring = Path::stringread (guardfile, 3 * 4096);
                  const int guardpid = string_to_int (guardstring);
                  if (guardpid > 0 && guardstring == pid_string (guardpid))
                    {
                      SDEBUG ("skip: %s", guardfile);
                      continue;
                    }
                  rmrf_dir (direntry.path().string());
                }
            }
        }
}

// == Storage ==
Storage::~Storage ()
{}

// == StorageWriter ==
class StorageWriter::Impl {
public:
  void *writer = nullptr;
  String zipname;
  int flags = 0;
  ~Impl()
  {
    if (writer)
      warning ("Ase::StorageWriter: ZIP file left open: %s", zipname);
    close();
  }
  Error
  close()
  {
    return_unless (writer != nullptr, Error::NONE);
    int mzerr = mz_zip_writer_close (writer);
    const int saved_errno = errno;
    if (mzerr != MZ_OK && !zipname.empty())
      unlink (zipname.c_str());
    mz_zip_writer_delete (&writer);
    writer = nullptr;
    errno = saved_errno;
    return mzerr == MZ_OK ? Error::NONE : ase_error_from_errno (errno);
  }
  Error
  remove_opened()
  {
    if (writer)
      {
        close();
        if (!zipname.empty())
          unlink (zipname.c_str());
      }
    return Error::NONE;
  }
  Error
  open_for_writing (const String &filename)
  {
    assert_return (writer == nullptr, Error::INTERNAL);
    zipname = filename;
    mz_zip_writer_create (&writer);
    mz_zip_writer_set_zip_cd (writer, false);
    mz_zip_writer_set_password (writer, nullptr);
    mz_zip_writer_set_store_links (writer, false);
    mz_zip_writer_set_follow_links (writer, true);
    mz_zip_writer_set_compress_level (writer, MZ_COMPRESS_LEVEL_BEST);
    mz_zip_writer_set_compress_method (writer, MZ_COMPRESS_METHOD_DEFLATE);
    int mzerr = mz_zip_writer_open_file (writer, zipname.c_str(), 0, false);
    if (mzerr != MZ_OK)
      {
        const int saved_errno = errno;
        mz_zip_writer_delete (&writer);
        writer = nullptr;
        unlink (filename.c_str());
        return ase_error_from_errno (saved_errno);
      }
    return Error::NONE;
  }
  Error
  store_file_data (const String &filename, const String &buffer, bool compress, int64_t epoch_seconds)
  {
    assert_return (mz_zip_writer_is_open (writer) == MZ_OK, Error::INTERNAL);
    const uint32 attrib = S_IFREG | 0664;
    const time_t fdate = epoch_seconds;
    mz_zip_file file_info = {
      .version_madeby = MZ_VERSION_MADEBY,
      .flag = MZ_ZIP_FLAG_UTF8,
      .compression_method = uint16_t (compress ? MZ_COMPRESS_METHOD_DEFLATE : MZ_COMPRESS_METHOD_STORE),
      .modified_date = fdate,
      .accessed_date = fdate,
      .creation_date = 0,
      .uncompressed_size = ssize_t (buffer.size()),
      .external_fa = attrib,
      .filename = filename.c_str(),
      .zip64 = buffer.size() > 4294967295, // match libmagic's ZIP-with-mimetype
    };
    int32_t mzerr = MZ_OK;
    if (MZ_HOST_SYSTEM (file_info.version_madeby) != MZ_HOST_SYSTEM_MSDOS &&
        MZ_HOST_SYSTEM (file_info.version_madeby) != MZ_HOST_SYSTEM_WINDOWS_NTFS)
      {
        uint32 target_attrib = 0;
        mzerr = mz_zip_attrib_convert (MZ_HOST_SYSTEM (file_info.version_madeby), attrib, MZ_HOST_SYSTEM_MSDOS, &target_attrib);
        file_info.external_fa = target_attrib; // MSDOS attrib
        file_info.external_fa |= attrib << 16; // OS attrib
      }
    if (mzerr == MZ_OK)
      mzerr = mz_zip_writer_add_buffer (writer, (void*) buffer.data(), buffer.size(), &file_info);
    return mzerr == MZ_OK ? Error::NONE : ase_error_from_errno (errno);
  }
};

StorageWriter::StorageWriter (StorageFlags sflags) :
  impl_ (std::make_shared<StorageWriter::Impl>())
{
  impl_->flags = sflags;
}

StorageWriter::~StorageWriter ()
{}

Error
StorageWriter::open_for_writing (const String &filename)
{
  assert_return (impl_, Error::INTERNAL);
  return impl_->open_for_writing (filename);
}

Error
StorageWriter::open_with_mimetype (const String &filename, const String &mimetype)
{
  Error err = open_for_writing (filename);
  if (err == Error::NONE)
    {
      const int64_t ase_project_start = 844503962;
      err = impl_->store_file_data ("mimetype", mimetype, false, ase_project_start);
      if (err != Error::NONE && impl_)
        impl_->remove_opened();
    }
  return err;
}

Error
StorageWriter::store_file_data (const String &filename, const String &buffer, bool alwayscompress)
{
  assert_return (impl_, Error::INTERNAL);
  const bool compressed = is_compressed (buffer);
  if (!compressed && (alwayscompress || impl_->flags & AUTO_ZSTD))
    {
      const String cdata = zstd_compress (buffer);
      if (alwayscompress || cdata.size() + 128 <= buffer.size())
        return impl_->store_file_data (filename + ".zst", cdata, false, time (nullptr));
    }
  return impl_->store_file_data (filename, buffer,
                                 compressed ? false : true,
                                 time (nullptr));
}

Error
StorageWriter::close ()
{
  assert_return (impl_, Error::INTERNAL);
  return impl_->close();
}

Error
StorageWriter::remove_opened ()
{
  assert_return (impl_, Error::INTERNAL);
  return impl_->remove_opened();
}

// == StorageReader ==
class StorageReader::Impl {
public:
  void *reader = nullptr;
  String zipname;
  int flags = 0;
  ~Impl()
  {
    close();
  }
  Error
  close()
  {
    return_unless (reader != nullptr, Error::NONE);
    int mzerr = mz_zip_reader_close (reader);
    const int saved_errno = errno;
    mz_zip_reader_delete (&reader);
    reader = nullptr;
    errno = saved_errno;
    return mzerr == MZ_OK ? Error::NONE : ase_error_from_errno (errno);
  }
  Error
  open_for_reading (const String &filename)
  {
    assert_return (reader == nullptr, Error::INTERNAL);
    zipname = filename;
    // setup reader and open file
    mz_zip_reader_create (&reader);
    // mz_zip_reader_set_pattern (reader, pattern, 1);
    mz_zip_reader_set_password (reader, nullptr);
    mz_zip_reader_set_encoding (reader, MZ_ENCODING_UTF8);
    errno = ELIBBAD;
    int err = mz_zip_reader_open_file (reader, zipname.c_str());
    if (err != MZ_OK)
      {
        const int saved_errno = errno;
        mz_zip_reader_delete (&reader);
        reader = nullptr;
        return ase_error_from_errno (saved_errno);
      }
    return Error::NONE;
  }
  StringS
  list_files()
  {
    errno = EINVAL;
    StringS list;
    assert_return (reader != nullptr, list);
    // check and extract file entries
    int err = mz_zip_reader_goto_first_entry (reader);
    while (err == MZ_OK)
      {
        mz_zip_file *file_info = nullptr;
        if (MZ_OK == mz_zip_reader_entry_get_info (reader, &file_info) && file_info->filename && file_info->filename[0])
          {
            if (!strchr (file_info->filename, '/') && // see: https://github.com/nmoinvaz/minizip/issues/433
                !strchr (file_info->filename, '\\'))
              list.push_back (file_info->filename);
          }
        err = mz_zip_reader_goto_next_entry (reader); // yields MZ_END_OF_LIST
      }
    return list;
  }
  bool
  has_file (const String &filename)
  {
    return_unless (mz_zip_reader_is_open (reader) == MZ_OK, false);
    String fname = Path::normalize (filename);
    if (MZ_OK == mz_zip_reader_locate_entry (reader, fname.c_str(), false))
      return true;
    if (flags & AUTO_ZSTD)
      {
        fname += ".zst";
        if (MZ_OK == mz_zip_reader_locate_entry (reader, fname.c_str(), false))
          return true;
      }
    return false;
  }
  String
  stringread (const String &filename)
  {
    errno = EINVAL;
    assert_return (mz_zip_reader_is_open (reader) == MZ_OK, {});
    String fname = Path::normalize (filename);
    bool uncompress;
    if (MZ_OK == mz_zip_reader_locate_entry (reader, fname.c_str(), false))
      uncompress = false;
    else if (flags & AUTO_ZSTD)
      {
        fname += ".zst";
        if (MZ_OK == mz_zip_reader_locate_entry (reader, fname.c_str(), false))
          uncompress = true;
        else
          return_with_errno (ENOENT, {});
      }
    else
      return_with_errno (ENOENT, {});
    ssize_t len = mz_zip_reader_entry_save_buffer_length (reader);
    if (len >= 0)
      {
        String buffer (len, 0);
        if (MZ_OK == mz_zip_reader_entry_save_buffer (reader, &buffer[0], buffer.size()))
          {
            errno = 0;
            return uncompress ? zstd_uncompress (buffer) : buffer;
          }
      }
    return_with_errno (ENOENT, {});
  }
};

StorageReader::StorageReader (StorageFlags sflags) :
  impl_ (std::make_shared<StorageReader::Impl>())
{
  impl_->flags = sflags;
}

StorageReader::~StorageReader ()
{}

Error
StorageReader::open_for_reading (const String &filename)
{
  assert_return (impl_, Error::INTERNAL);
  return impl_->open_for_reading (filename);
}

StringS
StorageReader::list_files ()
{
  assert_return (impl_, {});
  return impl_->list_files();
}

Error
StorageReader::close ()
{
  assert_return (impl_, Error::INTERNAL);
  return impl_->close();
}

bool
StorageReader::has_file (const String &filename)
{
  assert_return (impl_, false);
  return impl_->has_file (filename);
}

String
StorageReader::stringread (const String &filename, ssize_t maxlength)
{
  errno = EINVAL;
  assert_return (impl_, {});
  String data = impl_->stringread (filename);
  if (maxlength >= 0 && maxlength < int64_t (data.size()))
    data.resize (maxlength);
  return data;
}

void
StorageReader::search_dir (const String &dirname)
{
  // TODO: implement directory search for fallbacks
}

} // Ase
