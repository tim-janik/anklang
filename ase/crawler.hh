// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CRAWLER_HH__
#define __ASE_CRAWLER_HH__

#include <ase/gadget.hh>

namespace Ase {

class FileCrawler : public ObjectImpl, public virtual ResourceCrawler {
  String cwd_;
  FileCrawler (const String &cwd);
public:
  ASE_DEFINE_MAKE_SHARED (FileCrawler);
  ResourceS list_entries   () override;
  Resource  current_folder () override;
};

} // Ase

#endif // __ASE_CRAWLER_HH__
