// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "trkn-utils.hh"
#include "platform.hh"
#include "main.hh"
#include "logging.hh"
#include "internal.hh"

namespace te = tracktion::engine;

namespace Ase {

// == SelectableBaseref ==
using JuceWeakReference_Selectable = juce::WeakReference<tracktion::Selectable>;

template<size_t N> static JuceWeakReference_Selectable&
juce_weak_reference_selectable (const char (&mem)[N])
{
  static_assert (N == sizeof (JuceWeakReference_Selectable));
  return *reinterpret_cast<JuceWeakReference_Selectable*> (const_cast<char*> (mem));
}

SelectableBaseref::SelectableBaseref()
{
  static_assert (sizeof (mem_) == sizeof (JuceWeakReference_Selectable));
  JuceWeakReference_Selectable *const weakref = new (mem_) JuceWeakReference_Selectable (nullptr);
  assert_return (weakref != nullptr);
}

SelectableBaseref::~SelectableBaseref()
{
  JuceWeakReference_Selectable &weakref = juce_weak_reference_selectable (mem_);
  weakref.~JuceWeakReference_Selectable();
}

tracktion::Selectable*
SelectableBaseref::get () const noexcept
{
  JuceWeakReference_Selectable &weakref = juce_weak_reference_selectable (mem_);
  return weakref.get();
}

void
SelectableBaseref::set (tracktion::Selectable *selectable) noexcept
{
  JuceWeakReference_Selectable &weakref = juce_weak_reference_selectable (mem_);
  weakref = selectable;
}

void
SelectableBaseref::set (const SelectableBaseref &other) noexcept
{
  JuceWeakReference_Selectable &weakref = juce_weak_reference_selectable (mem_);
  const JuceWeakReference_Selectable &otherref = juce_weak_reference_selectable (other.mem_);
  weakref = otherref;
}

// == ase_obj_ helpers ==

void
register_ase_obj (VirtualBase *ase_impl, tracktion::Selectable &selectable)
{
  assert_return (ase_impl != nullptr);
  return_unless (selectable.ase_obj_ == nullptr);
  selectable.ase_obj_ = ase_impl;
}

void
unregister_ase_obj (VirtualBase *ase_impl, tracktion::Selectable *selectable)
{
  if (selectable && selectable->ase_obj_ == ase_impl)
    selectable->ase_obj_ = nullptr;
}

VirtualBase*
find_ase_obj_virtual_base (tracktion::Selectable *selectable)
{
  return selectable ? selectable->ase_obj_ : nullptr;
}

} // Ase
