// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "crawler.hh"
#include "jsonipc/jsonipc.hh"
#include "path.hh"
#include "internal.hh"
#include <dirent.h>
#include <fcntl.h>      // AT_NO_AUTOMOUNT
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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
  return_unless (dir, rs);
  for (struct dirent *de = readdir (dir); de; de = readdir (dir))
    {
      if ((de->d_name[0] == '.' && de->d_name[1] == 0) ||
          (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == 0))
        continue;
      struct stat statbuf = { 0, };
      if (0 != fstatat (dirfd (dir), de->d_name, &statbuf, AT_NO_AUTOMOUNT))
        continue;
      if (!S_ISREG (statbuf.st_mode) && !S_ISDIR (statbuf.st_mode))
        continue;
      Resource r { S_ISDIR (statbuf.st_mode) ? ResourceType::FOLDER : ResourceType::FILE,
                   de->d_name, 0, 0 };
      r.size = S_ISDIR (statbuf.st_mode) ? -statbuf.st_size : statbuf.st_size;
      r.mtime = statbuf.st_mtim.tv_sec * 1000 + statbuf.st_mtim.tv_nsec / 1000000;
      rs.push_back (r);
    }
  closedir (dir);
  return rs;
}

Resource
FileCrawler::current_folder ()
{
  Resource r { ResourceType::FOLDER, cwd_, 0, 0 };
  struct stat statbuf = { 0, };
  if (lstat (cwd_.c_str(), &statbuf) == 0)
    {
      r.size = statbuf.st_size;
      r.mtime = statbuf.st_mtim.tv_sec * 1000 + statbuf.st_mtim.tv_nsec / 1000000;
    }
  return r;
}

} // Ase
