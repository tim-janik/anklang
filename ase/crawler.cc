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

FileCrawler::FileCrawler (const String &cwd)
{
  cwd_ = Path::abspath (cwd);
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
      bool is_dir = de->d_type == DT_DIR;
      bool is_reg = de->d_type == DT_REG;
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
FileCrawler::get_dir (const String &which)
{
  if (which == ".")
    return cwd_;
  if (string_tolower (which) == "demo")
    return Ase::runpath (RPath::DEMODIR);
  if (string_tolower (which) == "home")
    return Ase::runpath (RPath::HOMEDIR);
  return "/";
}

Resource
FileCrawler::current_folder ()
{
  const String cwdir = string_endswith (cwd_, "/") ? cwd_ : cwd_ + '/';
  Resource r { .type = ResourceType::FOLDER, .label = Path::basename (cwd_), .uri = cwdir, };
  struct stat statbuf = { 0, };
  if (lstat (cwd_.c_str(), &statbuf) == 0)
    {
      r.size = statbuf.st_size;
      r.mtime = statbuf.st_mtim.tv_sec * 1000 + statbuf.st_mtim.tv_nsec / 1000000;
    }
  return r;
}

void
FileCrawler::go_down (const String &name)
{
  if (!name.empty())
    {
      cwd_ = Path::abspath (Path::join (cwd_, name));
      emit_event ("notify", "current");
      emit_event ("notify", "entries");
    }
}

void
FileCrawler::go_up ()
{
  const char *path = cwd_.c_str();
  const char *slash = strrchr (path, ASE_DIRCHAR);
  if (slash > path)
    {
      cwd_ = cwd_.substr (0, slash - path);
      emit_event ("notify", "current");
      emit_event ("notify", "entries");
    }
}

void
FileCrawler::assign (const String &path)
{
  cwd_ = canonify (path, "e");
  while (cwd_.size() > 1 && cwd_.back() == '/')
    cwd_.resize (cwd_.size() - 1);
  emit_event ("notify", "current");
  emit_event ("notify", "entries");
}

String
FileCrawler::asdir (const String &dirname)
{
  String p = Fs::path (dirname).lexically_normal(); // remove .. and /./ etc
  if (Path::check (p, "d"))
    return p;
  return ""; // not an existing directory
}

String
FileCrawler::canonify (const String &path, const String &checks)
{
  String p = path; // string_startswith (path, "file://") ? path.substr (6) : path;
  if (!Path::isabs (p))
    p = Path::abspath (p, cwd_);
  p = Fs::path (p).lexically_normal(); // remove .. and /./ etc
  if (Path::check (p, "d")) // canonify directory with trailing slash
    return Fs::path (p + "/").lexically_normal();
  if (Path::check (p, "e")) // exists?
    return p;
  String d = Path::dirname (p); // dir component MUST exist
  while (d != "/" && !Path::check (d, "e"))
    {
      p = d;
      d = Path::dirname (p);
    }
  const bool must_exist = checks.find ('e') != String::npos;
  return must_exist ? d : p;
}

} // Ase
