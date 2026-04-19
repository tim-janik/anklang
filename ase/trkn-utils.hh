// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

#pragma once
#include <ase/trkn.hh>
#include <unordered_map>
#include <shared_mutex>

namespace Ase {

/// Wrapper for juce::WeakReference<tracktion::Selectable>
class SelectableBaseref final {
  union { void *const align_; char mem_[sizeof (void*)]; };
  void                   set (tracktion::Selectable*) noexcept;
  void                   set (const SelectableBaseref&) noexcept;
public:
  tracktion::Selectable* get () const noexcept;
  /*dtor*/              ~SelectableBaseref();
  explicit               SelectableBaseref();
  SelectableBaseref (tracktion::Selectable *obj) : SelectableBaseref()    { set (obj); }
  SelectableBaseref (const SelectableBaseref &ref) : SelectableBaseref()  { set (ref); }
  SelectableBaseref& operator= (tracktion::Selectable *obj)     { set (obj); return *this; }
  SelectableBaseref& operator= (const SelectableBaseref &ref)   { set (ref); return *this; }
};

/// Mimick tracktion::engine::SafeSelectable<> for tracktion::Selectable descendants
template<typename SelectableType>
class SelectableWeakref {
  SelectableBaseref base_;
public:
  explicit           SelectableWeakref () = default;
  explicit           SelectableWeakref (SelectableType *selectable) : base_ (selectable) {}
  /*copy*/           SelectableWeakref (const SelectableWeakref &o) noexcept : base_ (o.base_) {}
  SelectableType*    get        () const noexcept                       { return dynamic_cast<SelectableType*> (base_.get()); }
  SelectableType*    operator-> () const noexcept                       { return get(); }
  SelectableWeakref& operator=  (const SelectableWeakref &ref)          { base_ = ref.base_; return *this; }
  SelectableWeakref& operator=  (SelectableType *obj)                   { base_ = obj; return *this; }
  bool               operator== (SelectableType *obj) const noexcept    { return base_.get() == obj; }
  bool               operator!= (SelectableType *obj) const noexcept    { return base_.get() != obj; }
  bool               operator== (std::nullptr_t) const noexcept         { return base_.get() == nullptr; }
  bool               operator!= (std::nullptr_t) const noexcept         { return base_.get() != nullptr; }
  bool               operator== (const SelectableWeakref &ref) const noexcept { return base_.get() == ref.base_.get(); }
  bool               operator!= (const SelectableWeakref &ref) const noexcept { return base_.get() != ref.base_.get(); }
};

/// Helper: register AseImpl with a tracktion Selectable via ase_obj_
void register_ase_obj (VirtualBase *ase_impl, tracktion::Selectable &selectable);
/// Helper: unregister AseImpl from a tracktion Selectable (selectable may be nullptr)
void unregister_ase_obj (VirtualBase *ase_impl, tracktion::Selectable *selectable);
/// Helper: lookup Ase::VirtualBase from tracktion Selectable via ase_obj_
VirtualBase* find_ase_obj_virtual_base (tracktion::Selectable *selectable);
/// Helper: lookup AseType from tracktion Selectable via ase_obj_
template<typename AseType> inline AseType*
find_ase_obj (tracktion::Selectable &selectable)
{
  return dynamic_cast<AseType*> (find_ase_obj_virtual_base (&selectable));
}
/// Helper: lookup AseType from tracktion Selectable* via ase_obj_
template<typename AseType> inline AseType*
find_ase_obj (tracktion::Selectable *selectable)
{
  return selectable ? find_ase_obj<AseType> (*selectable) : nullptr;
}

} // Ase
