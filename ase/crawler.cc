// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "crawler.hh"
#include "jsonipc/jsonipc.hh"
#include "path.hh"
#include "platform.hh"
#include "unicode.hh"
#include "strings.hh"
#include "internal.hh"
#include <dirent.h>
#include <fcntl.h>      // AT_NO_AUTOMOUNT
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
namespace Fs = std::filesystem;

#define CDEBUG(...)     Ase::debug ("crawler", __VA_ARGS__)

namespace Ase {

// == ResourceCrawler ==
ResourceCrawler::ResourceCrawler() :
  folder (this, "folder"),
  entries (this, "entries")
{}

// == FileCrawler ==
FileCrawler::FileCrawler (const String &cwd, bool constraindir, bool constrainfile) :
  cwd_ ("/"), constraindir_ (constraindir), constrainfile_ (constrainfile)
{
  if (!cwd.empty())
    assign_ (encodefs (cwd), false, false);
}

/// List all entries in the current folder.
ResourceS
FileCrawler::list_entries ()
{
  ResourceS rs;
  DIR *dir = opendir (cwd_.c_str());
  if (!dir)
    CDEBUG ("%s: opendir('%s'): %s", __func__, cwd_, strerror (errno));
  return_unless (dir, rs);
  String cwdfile = cwd_ + "/";
  for (struct dirent *de = readdir (dir); de; de = readdir (dir))
    {
      bool is_dir; // = de->d_type == DT_DIR;
      bool is_reg; // = de->d_type == DT_REG;
      ssize_t size = -1;
      int64 mtime = 0;
      if (true) // (de->d_type == DT_LNK || de->d_type == DT_UNKNOWN)
        {
          struct stat statbuf = { 0, };
          if (0 != fstatat (dirfd (dir), de->d_name, &statbuf, AT_NO_AUTOMOUNT))
            continue;
          is_dir = S_ISDIR (statbuf.st_mode);
          is_reg = S_ISREG (statbuf.st_mode);
          size = statbuf.st_size;
          mtime = statbuf.st_mtim.tv_sec * 1000 + statbuf.st_mtim.tv_nsec / 1000000;
        }
      if (!is_reg && !is_dir)
        continue;
      Resource r {
        .type = is_dir ? ResourceType::FOLDER : ResourceType::FILE,
        .label = displayfs (encodefs (de->d_name)),
        .uri = encodefs (cwdfile + de->d_name) + (is_dir ? "/" : ""),
        .mtime = mtime };
      r.size = is_dir && size > 0 ? -size : size;
      rs.push_back (r);
    }
  closedir (dir);
  return rs;
}

/// Expand directory name (file system encoding) to absolute folder path.
String
FileCrawler::expand_fsdir (const String &fsdir)
{
  if (fsdir == ".")
    return Path::dir_terminate (cwd_);
  if (string_toupper (fsdir) == "DEMO")
    return Path::dir_terminate (anklang_runpath (RPath::DEMODIR));
  String dir = Path::xdg_dir (fsdir);
  if (!dir.empty())
    return Path::dir_terminate (dir);
  return "/";
}

/// Return the current folder.
Resource
FileCrawler::current_folder ()
{
  Resource r {
    .type = ResourceType::FOLDER,
    .label = displayfs (encodefs (Path::basename (cwd_))),
    .uri = encodefs (Path::dir_terminate (cwd_)),
    .mtime = 0 };
  struct stat statbuf = { 0, };
  if (lstat (cwd_.c_str(), &statbuf) == 0)
    {
      r.size = statbuf.st_size;
      r.mtime = statbuf.st_mtim.tv_sec * 1000 + statbuf.st_mtim.tv_nsec / 1000000;
    }
  return r;
}

bool
FileCrawler::folder_ (const Resource *n, Resource *q)
{
  if (n)
    assign_ (n->uri, false);
  if (q)
    *q = current_folder();
  return true;
}

bool
FileCrawler::entries_ (const ResourceS *n, ResourceS *q)
{
  if (n)
    ; // assignment not supported
  if (q)
    *q = list_entries();
  return true;
}

/// Open a new folder and make it the current folder.
FileCrawler::String2
FileCrawler::assign_ (const String &utf8path, bool existingfile, bool notify)
{
  String dir = decodefs (utf8path);
  // tilde + uppercase features some special directory expansions
  if (dir[0] == '~' &&
      dir.find ("/") == dir.npos &&
      string_isupper (dir))
    {
      if (dir == "~DEMO")
        dir = Path::dir_terminate (anklang_runpath (RPath::DEMODIR));
      else
        dir = Path::xdg_dir (&dir[1]);
      if (dir.empty() || dir == "/") // failed to expand special word
        dir = decodefs (utf8path);
    }
  // ~USER expansion
  Fs::path p = Path::expand_tilde (dir);
  // make absolute
  if (!p.is_absolute())
    {
      Fs::path root = cwd_;
      p = root / p;
    }
  // normalize, remove /// /./ ..
  p = p.lexically_normal();
  // return existing file or dir with slash
  String filename = "";
  if (Path::check (p, "d"))             // isdir?
    cwd_ = Path::dir_terminate (p);
  else if (Path::check (p, "e")) {      // exists?
    filename = p.filename();
    cwd_ = p.parent_path();
  } else if (constraindir_) {           // find existing dir
    while (p.string().size() > 1 &&
           !Path::check (p, "d")) {
      filename = !existingfile || Path::check (p, "e") ? p.filename() : "";
      p = p.parent_path();
    }
    cwd_ = p;
  } else {
    filename = !existingfile || Path::check (p, "e") ? p.filename() : "";
    cwd_ = p.parent_path();
  }
  // strip terminating slashes from cwd_
  while (cwd_.size() > 1 && cwd_.back() == '/')
    cwd_.resize (cwd_.size() - 1);
  if (notify)
    {
      folder.notify();
      entries.notify();
      emit_notify ("current");
      emit_notify ("entries");
    }
  return { cwd_, filename };
}

/// Canonify a path (file system encoding), optionally constraining it to a directory or file.
String
FileCrawler::canonify_fspath (const String &fscwd, const String &fsfragment, bool constraindir, bool constrainfile)
{
  // expansions
  Fs::path p = Path::expand_tilde (fsfragment);
  // make absolute
  if (!p.is_absolute())
    {
      Fs::path root = fscwd;
      if (!root.is_absolute())
        root = cwd_ / root;
      p = root / p;
    }
  // normalize, remove /// /./ ..
  p = p.lexically_normal();
  // return existing file or dir with slash
  if (Path::check (p, "d"))     // isdir?
    return Path::dir_terminate (p);
  if (Path::check (p, "e"))     // exists?
    return p;
  // force existing directory
  Fs::path d = p.parent_path(), f = p.filename();
  if (constraindir)
    while (d.relative_path() != "" && !Path::check (d, "d"))
      d = d.parent_path();
  p = d / f;
  // force existing or empty file
  if (constrainfile && !Path::check (p, "e"))
    p = d;
  // return dirs with slash
  if (Path::check (p, "d"))
    return Path::dir_terminate (p);
  return p;
}

/// Canonify a UTF-8 path name, optionally constraining it to a folder or file.
Resource
FileCrawler::canonify (const String &utf8cwd, const String &utf8fragment, bool constraindir, bool constrainfile)
{
  const String utf8path = encodefs (canonify_fspath (decodefs (utf8cwd), decodefs (utf8fragment), constraindir, constrainfile));
  Resource r {
    .type = string_endswith (utf8path, "/") ? ResourceType::FOLDER : ResourceType::FILE,
    .label = displayfs (utf8path),
    .uri = utf8path,
    .mtime = 0 };
  return r;
}

} // Ase

#include "testing.hh"

TEST_INTEGRITY (crawler_tests);
/// Test the filecrawler functions.
static void
crawler_tests()
{
  using namespace Ase;
  FileCrawlerP cp = FileCrawler::make_shared ("/dev", true, false);
  FileCrawler &c = *cp;
  String r, e;
  r = c.canonify_fspath ("", "/dev/N°…Diŕ/N°…Fílė", 1, 1); e = "/dev/";        TCMP (r, ==, e);
  r = c.canonify_fspath (".", ".", 1, 1); e = "/dev/";                         TCMP (r, ==, e);
  r = c.canonify_fspath ("", "/dev/N°…Diŕ/null", 1, 1); e = "/dev/null";       TCMP (r, ==, e);
  r = c.canonify_fspath ("", "/tmp/N°…Diŕ/N°…Fílė", 1, 0); e = "/tmp/N°…Fílė"; TCMP (r, ==, e);
  r = c.canonify_fspath ("", "/tmp/N°…Diŕ//.//", 0, 0); e = "/tmp/N°…Diŕ/";    TCMP (r, ==, e);
  r = c.canonify_fspath ("", "/tmp/N°…Diŕ//..//", 0, 0); e = "/tmp/";          TCMP (r, ==, e);
  r = c.canonify_fspath ("N°…Diŕ", "N°…Fílė", 1, 1); e = "/dev/";              TCMP (r, ==, e);
  r = c.canonify_fspath ("/N°…Diŕ", "N°…Fílė", 1, 1); e = "/";                 TCMP (r, ==, e);
}
