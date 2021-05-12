// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_OBJECT_HH__
#define __ASE_OBJECT_HH__

#include <ase/api.hh>

namespace Ase {

/// Implementation type for classes with Event subscription.
class EmittableImpl : public virtual Emittable {
  struct EventDispatcher;
  EventDispatcher *ed_ = nullptr;
  friend class EventConnection;
protected:
  virtual ~EmittableImpl ();
public:
  Connection on_event   (const String &eventselector, const EventHandler &eventhandler) override;
  void       emit_event (const String &type, const String &detail, ValueR fields = {}) override;
};

/// Implementation type for classes with Property interfaces.
class ObjectImpl : public EmittableImpl, public virtual Object {
protected:
  virtual      ~ObjectImpl () = 0;
public:
};
using ObjectImplP = std::shared_ptr<ObjectImpl>;

} // Ase

#endif // __ASE_OBJECT_HH__
