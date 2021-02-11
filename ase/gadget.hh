// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_GADGET_HH__
#define __ASE_GADGET_HH__

#include <ase/object.hh>

namespace Ase {

/// Base type for classes that have a Property.
class GadgetImpl : public virtual Gadget, public virtual ObjectImpl {
protected:
  virtual    ~GadgetImpl        ();
public:
  std::string type_nick         () const override;
  std::string name              () const override;
  void        name              (std::string newname) override;
  StringS     list_properties   () override;
  PropertyP   access_property   (String ident) override;
  PropertyS   access_properties () override;
};

} // Ase

#endif // __ASE_GADGET_HH__
