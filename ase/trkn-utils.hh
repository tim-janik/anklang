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

/// Ase handle object for tracktion engine objects
class SelectableHandle : public virtual VirtualBase, private tracktion::SelectableListener {
  SelectableWeakref<tracktion::Selectable> selectable_;
  static SelectableHandle* find_base_handle (tracktion::Selectable &selectable_obj);
  void discard_selectable ();
  void selectableObjectAboutToBeDeleted (tracktion::Selectable *object) override;
  void selectableObjectChanged (tracktion::Selectable *object) override;
public:
  SelectableHandle (tracktion::Selectable &selectable_obj);
  virtual ~SelectableHandle();
  virtual void discarded ();
  template<typename AseType> static AseType*
  find_selectable_handle (tracktion::Selectable &selectable_obj)
  {
    SelectableHandle *handle = find_base_handle (selectable_obj);
    return dynamic_cast<AseType*> (handle);
  }
};

} // Ase
