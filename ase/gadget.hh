// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_GADGET_HH__
#define __ASE_GADGET_HH__

#include <ase/object.hh>
#include <ase/utils.hh>

namespace Ase {

/// Base type for classes that have a Property.
class GadgetImpl : public ObjectImpl, public CustomDataContainer, public virtual Gadget, public virtual Serializable {
protected:
  virtual       ~GadgetImpl        ();
  virtual String fallback_name     () const;
  void           serialize         (WritNode &xs) override;
public:
  String         type_nick         () const override;
  String         name              () const override;
  void           name              (String newname) override;
  StringS        list_properties   () override;
  PropertyP      access_property   (String ident) override;
  PropertyS      access_properties () override;
};

} // Ase

#endif // __ASE_GADGET_HH__
