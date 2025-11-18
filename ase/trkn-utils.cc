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

// == SelectableHandle ==
using SelectableHandleMap = std::unordered_map<tracktion::Selectable*, SelectableHandle*>;

static SelectableHandleMap&
selectable_handle_map()
{
  static SelectableHandleMap *map = [] { return new SelectableHandleMap(); } ();
  return *map;
}

SelectableHandle*
SelectableHandle::find_base_handle (tracktion::Selectable &selectable_obj)
{
  SelectableHandleMap &map = selectable_handle_map();
  auto it = map.find (&selectable_obj);
  return it != map.end() ? it->second : nullptr;
}

SelectableHandle::SelectableHandle (tracktion::Selectable &selectable_obj)
{
  SelectableHandleMap &map = selectable_handle_map();
  if (tracktion::Selectable::isSelectableValid (&selectable_obj)) {
    selectable_ = &selectable_obj;
    selectable_obj.addListener (this);
    map[&selectable_obj] = this;
  }
}

SelectableHandle::~SelectableHandle()
{
  discard_selectable();
}

void
SelectableHandle::discard_selectable()
{
  tracktion::Selectable *selectable = selectable_.get();
  return_unless (selectable != nullptr);
  SelectableHandleMap &map = selectable_handle_map();
  auto it = map.find (selectable);
  if (it != map.end()) {
    map.erase (it);
    if (tracktion::Selectable::isSelectableValid (selectable))
      selectable->removeListener (this);
  }
  selectable_ = nullptr;
}

void
SelectableHandle::selectableObjectAboutToBeDeleted (tracktion::Selectable *selectable)
{
  if (this_thread_is_main()) {
    warning ("TODO: verify this branch is reached");
    discard_selectable();
  } else {
    // not in main thread, `this` could be in dtor in main thread
    const SelectableHandle *handle = this; // save to re-check association
    main_jobs += [selectable, handle] {
      warning ("TODO: verify this lambda is reached");
      // main thread
      SelectableHandleMap &map = selectable_handle_map();
      auto it = map.find (selectable);
      if (it != map.end() &&  // selectable_ still valid
          it->second == handle)                 // prevent ABA
        const_cast<SelectableHandle*> (handle)->discard_selectable();
    };
  }
}

void
SelectableHandle::selectableObjectChanged (tracktion::Selectable *object)
{}

void
SelectableHandle::discarded ()
{}

} // Ase
