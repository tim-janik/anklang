// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_GADGET_HH__
#define __ASE_GADGET_HH__

#include <ase/object.hh>
#include <ase/utils.hh>

namespace Ase {

/// Base type for classes that have a Property.
class GadgetImpl : public ObjectImpl, public CustomDataContainer, public virtual Gadget, public virtual Serializable {
  GadgetImpl *parent_ = nullptr;
  uint64_t    gadget_flags_ = 0;
  ValueR      session_data_;
protected:
  enum : uint64_t { GADGET_DESTROYED = 0x1, DEVICE_ACTIVE = 0x2 };
  uint64_t       gadget_flags      () const     { return gadget_flags_; }
  uint64_t       gadget_flags      (uint64_t setbits, uint64_t mask = ~uint64_t (0));
  static String  canonify_key      (const String &input);
  virtual       ~GadgetImpl        ();
  virtual String fallback_name     () const;
  void           serialize         (WritNode &xs) override;
public:
  void           _set_parent       (GadgetImpl *parent) override;
  GadgetImpl*    _parent           () const override    { return parent_; }
  String         type_nick         () const override;
  String         name              () const override;
  void           name              (String newname) override;
  PropertyS      access_properties () override;
  bool           set_data          (const String &key, const Value &v) override;
  Value          get_data          (const String &key) const override;
};

} // Ase

#endif // __ASE_GADGET_HH__
