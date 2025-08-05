// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CRAWLER_HH__
#define __ASE_CRAWLER_HH__

#include <ase/gadget.hh>

namespace Ase {

/// Class implementing a file system crawler.
class FileCrawler final : public ObjectImpl, public virtual ResourceCrawler {
  String cwd_;
  const uint constraindir_ : 1;
  [[maybe_unused]]
  const uint constrainfile_ : 1;
  FileCrawler (const String &cwd, bool constraindir = false, bool constrainfile = false);
protected:
  String2   assign_         (const String &utf8path, bool existingfile, bool notify = true);
  bool      folder_         (const Resource *n, Resource *q) override;
  bool      entries_        (const ResourceS *n, ResourceS *q) override;
public:
  ASE_DEFINE_MAKE_SHARED (FileCrawler);
  ResourceS list_entries    () override;
  Resource  current_folder  () override;
  String2   assign          (const String &utf8path,
                             bool existingfile) override { return assign_ (utf8path, existingfile, true); }
  Resource  canonify        (const String &utf8cwd, const String &utf8fragment, bool constraindir, bool constrainfile) override;
  String    canonify_fspath (const String &fspath, const String &fsfragment, bool constraindir, bool constrainfile);
  String    expand_fsdir    (const String &fsdir);
};

} // Ase

#endif // __ASE_CRAWLER_HH__
