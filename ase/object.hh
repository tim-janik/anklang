// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_OBJECT_HH__
#define __ASE_OBJECT_HH__

#include <ase/api.hh>
#include <unordered_set>

namespace Ase {

/// Implementation type for classes with Event subscription.
class EmittableImpl : public virtual Emittable {
  struct EventDispatcher;
  EventDispatcher *ed_ = nullptr;
  friend class EventConnection;
protected:
  virtual ~EmittableImpl ();
public:
  ASE_USE_RESULT
  Connection on_event     (const String &eventselector, const EventHandler &eventhandler) override;
  void       emit_event   (const String &type, const String &detail, const ValueR fields = {}) override;
  void       emit_notify  (const String &detail);
};

class CoalesceNotifies {
  struct Notification {
    EmittableP emittable; String detail;
    bool operator== (const Notification &b) const { return emittable == b.emittable && detail == b.detail; }
  };
  struct NotificationHash { size_t operator() (const Notification&) const; };
  using NotificationSet = std::unordered_set<Notification,NotificationHash>;
  NotificationSet notifications_;
  CoalesceNotifies *next_ = nullptr;
public:
  explicit  CoalesceNotifies    ();
  void      flush_notifications ();
  /*dtor*/ ~CoalesceNotifies    ();
  friend class EmittableImpl;
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
