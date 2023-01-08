// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "object.hh"
#include "internal.hh"
#include "utils.hh"
#include "main.hh"

namespace Ase {

// == EventConnection ==
class EventConnection final {
  using EventDispatcher = EmittableImpl::EventDispatcher;
public:
  const String     selector_;
  EventHandler     handler_;
  EventDispatcher *dispatcher_ = nullptr;
  ~EventConnection();
  EventConnection (EventDispatcher &edispatcher, const String &eventselector, EventHandler handler) :
    selector_ (eventselector), handler_ (handler), dispatcher_ (&edispatcher)
  {}
  void disconnect ();
  bool connected  () const      { return nullptr != handler_; }
  void
  emit (const Event &event, const String &event_type, const String &detailedevent)
  {
    if (connected() && (selector_ == event_type || selector_ == detailedevent))
      handler_ (event);
  }
};

// == EventDispatcher ==
struct EmittableImpl::EventDispatcher {
  using EventConnectionP = std::shared_ptr<EventConnection>;
  std::vector<EventConnectionW> connections;
  uint                          in_emission = 0;
  bool                          needs_purging = false;
  void
  purge_connections ()
  {
    if (in_emission)
      {
        needs_purging = true;
        return;
      }
    needs_purging = false;
    for (size_t i = connections.size() - 1; i < connections.size(); i--)
      {
        EventConnectionP conp = connections[i].lock();
        if (!conp || !conp->connected())
          connections.erase (connections.begin() + i);
      }
  }
  void
  emit (const Event &event)
  {
    const String event_type = event.type();
    assert_return (event_type.empty() == false);
    const String event_detail = event.detail();
    const String detailedevent = event_detail.empty() ? event_type : event_type + ":" + event_detail;
    in_emission++;
    for (size_t i = 0; i < connections.size(); i++)
      {
        EventConnectionP conp = connections[i].lock();
        if (conp)
          conp->emit (event, event_type, detailedevent);
      }
    in_emission--;
    if (in_emission == 0 && needs_purging)
      purge_connections();
  }
  ~EventDispatcher()
  {
    assert_return (in_emission == 0);
    in_emission++; // block purge_connections() calls
    std::vector<EventConnectionW> old_connections;
    std::swap (old_connections, connections);
    for (auto &conw : old_connections)
      {
        EventConnectionP conp = conw.lock();
        if (conp)
          conp->disconnect();
      }
    in_emission--;
  }
};

void
EventConnection::disconnect ()
{
  const bool was_connected = connected();
  if (was_connected)
    {
      handler_ = nullptr;
      dispatcher_->purge_connections();
      dispatcher_ = nullptr;
    }
}

EventConnection::~EventConnection()
{
  handler_ = nullptr;
  dispatcher_ = nullptr;
}

bool
Emittable::Connection::connected () const
{
  const EventConnectionP &econp = *this;
  return econp && econp->connected();
}

void
Emittable::Connection::disconnect () const
{
  const EventConnectionP &econp = *this;
  if (econp)
    econp->disconnect();
}

// == CoalesceNotifies ==
static CoalesceNotifies *coalesce_notifies_head = nullptr;

CoalesceNotifies::CoalesceNotifies ()
{
  next_ = coalesce_notifies_head;
  coalesce_notifies_head = this;
}

void
CoalesceNotifies::flush_notifications ()
{
  while (!notifications_.empty())
    {
      NotificationSet notifications;
      notifications.swap (notifications_);
      for (const auto &ns : notifications)
        if (ns.emittable)
          ns.emittable->emit_event ("notify", ns.detail);
    }
}

CoalesceNotifies::~CoalesceNotifies ()
{
  CoalesceNotifies **ptrp = &coalesce_notifies_head;
  while (*ptrp != this && *ptrp)
    ptrp = &(*ptrp)->next_;
  assert_return (*ptrp);
  *ptrp = this->next_;
  flush_notifications();
}

size_t
CoalesceNotifies::NotificationHash::operator() (const Notification &notification) const
{
  size_t h = std::hash<void*>() (&*notification.emittable);
  h ^= std::hash<String>() (notification.detail);
  return h;
}

// == EmittableImpl ==
/// Emit `notify:detail`, multiple notifications maybe coalesced if a CoalesceNotifies instance exists.
void
EmittableImpl::emit_notify (const String &detail)
{
  EmittableP emittablep = this->weak_from_this().expired() ? nullptr : shared_ptr_cast<Emittable> (this);
  if (emittablep && coalesce_notifies_head)
    coalesce_notifies_head->notifications_.insert ({ emittablep, detail });
  else
    this->emit_event ("notify", detail);
}

EmittableImpl::~EmittableImpl()
{
  EventDispatcher *old = ed_;
  ed_ = nullptr;
  delete old;
}

Emittable::Connection
EmittableImpl::on_event (const String &eventselector, const EventHandler &eventhandler)
{
  return_unless (!eventselector.empty() && eventhandler, {});
  if (!ed_)
    ed_ = new EventDispatcher();
  EventConnectionP cptr = std::make_shared<EventConnection> (*ed_, eventselector, eventhandler);
  ed_->connections.push_back (cptr);
  return *static_cast<Connection*> (&cptr);
}

void
EmittableImpl::emit_event (const String &type, const String &detail, const ValueR fields)
{
  const char ident_chars[] =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (size_t i = 0; type[i]; i++)
    if (!strchr (ident_chars, type[i]))
      {
        warning ("invalid characters in Event type: %s", type);
        break;
      }
  for (size_t i = 0; detail[i]; i++)
    if (!strchr (ident_chars, detail[i]) and detail[i] != '_')
      {
        warning ("invalid characters in Event detail: %s:%s", type, detail);
        break;
      }
  return_unless (ed_!= nullptr);
  Event ev { type, detail };
  for (auto &&e : fields)
    if (e.name != "type" && e.name != "detail")
      ev.push_back (e);
  ed_->emit (ev);       // emit detailedevent="notify:detail" as type="notify" detail="detail"
}

// == Emittable ==
void
Emittable::js_trigger (const String &eventselector, JsTrigger trigger)
{
  return_unless (trigger);
  Emittable::Connection econ = on_event (eventselector, trigger);
  trigger.ondestroy ([econ] () { econ.disconnect(); }); // must avoid strong references
}

// == ObjectImpl ==
ObjectImpl::~ObjectImpl()
{}

// == Object ==
Object::~Object()
{}

} // Ase
