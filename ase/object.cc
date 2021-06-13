// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "object.hh"
#include "internal.hh"
#include "utils.hh"

namespace Ase {

// == EventConnection ==
class EventConnection final {
  using EventDispatcher = EmittableImpl::EventDispatcher;
public:
  const String     selector_;
  EventHandler     handler_;
  EventDispatcher &o_;
  EventConnection (EventDispatcher &edispatcher, const String &eventselector, EventHandler handler) :
    selector_ (eventselector), handler_ (handler), o_ (edispatcher)
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
  std::vector<EventConnectionP> connections;
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
      if (!connections[i]->connected())
        connections.erase (connections.begin() + i);
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
      connections[i]->emit (event, event_type, detailedevent);
    in_emission--;
    if (in_emission == 0 && needs_purging)
      purge_connections();
  }
  ~EventDispatcher()
  {
    assert_return (in_emission == 0);
    in_emission++; // block purge_connections() calls
    for (auto &conp : connections)
      conp->disconnect();
    in_emission--;
  }
};

void
EventConnection::disconnect ()
{
  const bool was_connected = connected();
  handler_ = nullptr;
  if (was_connected)
    o_.purge_connections();
}

bool
Emittable::Connection::connected () const
{
  std::shared_ptr<EventConnection> con = this->lock();
  return con && con->connected();
}

void
Emittable::Connection::disconnect () const
{
  std::shared_ptr<EventConnection> con = this->lock();
  if (con)
    con->disconnect();
}

// == EmittableImpl ==
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
  ed_->connections.push_back (std::make_shared<EventConnection> (*ed_, eventselector, eventhandler));
  EventConnectionW wptr = ed_->connections.back();
  return *static_cast<Connection*> (&wptr);
}

void
EmittableImpl::emit_event (const String &type, const String &detail, ValueR fields)
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
