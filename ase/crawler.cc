// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "crawler.hh"
#include "jsonipc/jsonipc.hh"
#include "path.hh"
#include "platform.hh"
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

// == FileCrawler ==
JSONIPC_INHERIT (FileCrawler, ResourceCrawler);

FileCrawler::FileCrawler (const String &cwd, bool constraindir, bool constrainfile) :
  cwd_ ("/"), constraindir_ (constraindir), constrainfile_ (constrainfile)
{
  if (!cwd.empty())
    assign (cwd, false);
}

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
      Resource r { .type = is_dir ? ResourceType::FOLDER : ResourceType::FILE,
                   .label = de->d_name, .uri = cwdfile + de->d_name, .mtime = mtime };
      r.size = is_dir && size > 0 ? -size : size;
      rs.push_back (r);
    }
  closedir (dir);
  return rs;
}

String
FileCrawler::expand_dir (const String &which)
{
  if (which == ".")
    return Path::dir_terminate (cwd_);
  if (string_toupper (which) == "DEMO")
    return Path::dir_terminate (anklang_runpath (RPath::DEMODIR));
  String dir = Path::xdg_dir (which);
  if (!dir.empty())
    return Path::dir_terminate (dir);
  return "/";
}

Resource
FileCrawler::current_folder ()
{
  Resource r { .type = ResourceType::FOLDER, .label = Path::basename (cwd_),
               .uri = Path::dir_terminate (cwd_), };
  struct stat statbuf = { 0, };
  if (lstat (cwd_.c_str(), &statbuf) == 0)
    {
      r.size = statbuf.st_size;
      r.mtime = statbuf.st_mtim.tv_sec * 1000 + statbuf.st_mtim.tv_nsec / 1000000;
    }
  return r;
}

void
FileCrawler::assign (const String &path, bool notify)
{
  String dir = path;
  if (dir.find ("/") == dir.npos) // relative navigation features special expansions
    {
      dir = expand_dir (dir);
      if (dir.empty() || dir == "/") // failed to expand special word
        dir = path;
    }
  cwd_ = canonify (cwd_, dir.empty() ? "." : dir + "/", constraindir_, constrainfile_);
  while (cwd_.size() > 1 && cwd_.back() == '/')
    cwd_.resize (cwd_.size() - 1);
  if (notify)
    {
      emit_notify ("current");
      emit_notify ("entries");
    }
}

String
FileCrawler::canonify (const String &cwd, const String &fragment, bool constraindir, bool constrainfile)
{
  // expansions
  Fs::path p = Path::expand_tilde (fragment);
  // make absolute
  if (!p.is_absolute())
    {
      Fs::path root = cwd;
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

} // Ase

#include "testing.hh"

TEST_INTEGRITY (crawler_tests);
static void
crawler_tests()
{
  using namespace Ase;
  FileCrawlerP cp = FileCrawler::make_shared ("/dev", true, false);
  FileCrawler &c = *cp;
  String r, e;
  r = c.canonify ("", "/dev/N°…Diŕ/N°…Fílė", 1, 1); e = "/dev/";        TCMP (r, ==, e);
  r = c.canonify (".", ".", 1, 1); e = "/dev/";                         TCMP (r, ==, e);
  r = c.canonify ("", "/dev/N°…Diŕ/null", 1, 1); e = "/dev/null";       TCMP (r, ==, e);
  r = c.canonify ("", "/tmp/N°…Diŕ/N°…Fílė", 1, 0); e = "/tmp/N°…Fílė"; TCMP (r, ==, e);
  r = c.canonify ("", "/tmp/N°…Diŕ//.//", 0, 0); e = "/tmp/N°…Diŕ/";    TCMP (r, ==, e);
  r = c.canonify ("", "/tmp/N°…Diŕ//..//", 0, 0); e = "/tmp/";          TCMP (r, ==, e);
  r = c.canonify ("N°…Diŕ", "N°…Fílė", 1, 1); e = "/dev/";              TCMP (r, ==, e);
  r = c.canonify ("/N°…Diŕ", "N°…Fílė", 1, 1); e = "/";                 TCMP (r, ==, e);
}
