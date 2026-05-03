// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/object.hh>
#include <ase/utils.hh>
#include <ase/properties.hh>

namespace Ase {

/// Base type for classes that have a Property.
class GadgetImpl : public ObjectImpl, public CustomDataContainer, public virtual Gadget {
  GadgetImpl *parent_ = nullptr;
  uint64_t    gadget_flags_ = 0;
  ValueR      session_data_;
protected:
  PropertyImplS props_;
  enum : uint64_t { GADGET_DESTROYED = 0x1, DEVICE_ACTIVE = 0x2, MASTER_TRACK = 0x4 };
  uint64_t       gadget_flags      () const     { return gadget_flags_; }
  uint64_t       gadget_flags      (uint64_t setbits, uint64_t mask = ~uint64_t (0));
  static String  canonify_key      (const String &input);
  virtual       ~GadgetImpl        ();
  virtual String fallback_name     () const;
  virtual void   create_properties ();
public:
  String         name              () const override;
  void           name              (const std::string &n) override;
  void           _set_parent       (GadgetImpl *parent) override;
  GadgetImpl*    _parent           () const override    { return parent_; }
  String         type_nick         () const override;
  PropertyS      access_properties () override;
  bool           set_data          (const String &key, const Value &v) override;
  Value          get_data          (const String &key) const override;
  void           remove_self       () override;
  template<class O, class M> void _register_parameter (O*, M*, const Param::ExtraVals&) const;
  using MemberAccessF = std::function<bool(GadgetImpl*,const Value*,Value*)>;
  using MemberInfosP = const StringS& (*) ();
  using MemberClassT = bool (*) (const SharedBase&);
private:
  static bool requires_accessor (const char *ot, const char *mt, ptrdiff_t offset);
  static void register_accessor (const char *ot, const char *mt, ptrdiff_t offset, MemberClassT,
                                 const Param::ExtraVals&, MemberAccessF&&, MemberInfosP, uint64_t);
};

template<class O, class M> void
GadgetImpl::_register_parameter (O *obj, M *memb, const Param::ExtraVals &ev) const
{
  static_assert (M::is_unique_per_member); // allows indexing per typeid, instead of per instance
  GadgetImpl *gadget = obj;
  ASE_ASSERT_RETURN (this == gadget);
  const auto object_typeid_name = typeid_name<O>();
  const auto member_typeid_name = typeid_name<M>();
  const ptrdiff_t offset = ptrdiff_t (memb) - ptrdiff_t (obj);
  if (!requires_accessor (object_typeid_name, member_typeid_name, offset))
    return;
  const MemberClassT classtest = [] (const SharedBase &b) -> bool { return !!dynamic_cast<const O*> (&b); };
  using value_type = decltype (memb->get());
  auto accessor = [offset] (GadgetImpl *g, const Value *in, Value *out) {
    O &o = dynamic_cast<O&> (*g);
    const ptrdiff_t maddr = ptrdiff_t (&o) + offset;
    M *m = reinterpret_cast<M*> (maddr);
    bool r = false;
    if (in) {
      value_type v = {};
      if constexpr (std::is_integral_v<value_type>)
        v = in->as_int();
      else if constexpr (std::is_floating_point_v<value_type>)
        v = in->as_double();
      else if constexpr (std::is_assignable_v<value_type&,String>)
        v = in->as_string();
      else if constexpr (std::is_assignable_v<value_type&,const ValueS>)
        v = in->as_array();
      else if constexpr (std::is_assignable_v<value_type&,const ValueR>)
        v = in->as_record();
      else
        static_assert (sizeof (value_type) < 0, "unhandled <value_type>");
      r = m->set (v);
    }
    if (out)
      *out = Value (m->get());
    return r;
  };
  register_accessor (object_typeid_name, member_typeid_name, offset, classtest, ev, accessor, &memb->infos, memb->hints());
}

} // Ase

