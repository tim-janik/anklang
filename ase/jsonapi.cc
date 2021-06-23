// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "jsonapi.hh"
#include "server.hh"
#include "main.hh"
#include "internal.hh"

namespace Ase {

static String subprotocol_authentication;

void
jsonapi_require_auth (const String &subprotocol)
{
  subprotocol_authentication = subprotocol;
}

// == JsonapiConnection ==
class JsonapiConnection;
using JsonapiConnectionP = std::shared_ptr<JsonapiConnection>;
static JsonapiConnectionP current_message_conection;

class JsonapiConnection : public WebSocketConnection, public CustomDataContainer {
  Jsonipc::InstanceMap imap_;
  void
  log (const String &message) override
  {
    if (main_config.jsipc)
      printerr ("%s: %s\n", nickname(), message);
  }
  int
  validate() override
  {
    using namespace AnsiColors;
    const auto C1 = color (BOLD), C0 = color (BOLD_OFF);
    const Info info = get_info();
    if (info.subs.size() == 0 && subprotocol_authentication.empty())
      return 0; // OK
    else if (info.subs.size() == 1 && subprotocol_authentication == info.subs[0])
      return 0; // pick first and only
    log (string_format ("%sREJECT:%s  %s:%d/ %s", C1, C0, info.remote, info.rport, info.ua));
    return -1; // reject
  }
  void
  opened() override
  {
    using namespace AnsiColors;
    const auto C1 = color (BOLD), C0 = color (BOLD_OFF);
    const Info info = get_info();
    log (string_format ("%sACCEPT:%s  %s:%d/ %s", C1, C0, info.remote, info.rport, info.ua));
  }
  void
  closed() override
  {
    using namespace AnsiColors;
    const auto C1 = color (BOLD), C0 = color (BOLD_OFF);
    log (string_format ("%sCLOSED%s", C1, C0));
    trigger_destroy_hooks();
  }
  void
  message (const String &message) override
  {
    JsonapiConnectionP conp = std::dynamic_pointer_cast<JsonapiConnection> (shared_from_this());
    assert_return (conp);
    String reply;
    // operator+=() works synchronously
    main_jobs += [&message, &reply, conp, this] () {
      current_message_conection = conp;
      reply = handle_jsonipc (message);
      current_message_conection = nullptr;
    };
    // when queueing asynchronously, we have to use WebSocketConnectionP
    if (!reply.empty())
      send_text (reply);
  }
  String handle_jsonipc (const std::string &message);
  std::vector<JsTrigger> triggers_; // HINT: use unordered_map if this becomes slow
public:
  explicit JsonapiConnection (WebSocketConnection::Internals &internals) :
    WebSocketConnection (internals)
  {}
  ~JsonapiConnection()
  {
    trigger_destroy_hooks();
  }
  JsTrigger
  trigger_lookup (const String &id)
  {
    for (auto it = triggers_.begin(); it != triggers_.end(); it++)
      if (id == it->id())
        return *it;
    return {};
  }
  void
  trigger_remove (const String &id)
  {
    trigger_lookup (id).destroy();
  }
  void
  trigger_create (const String &id)
  {
    using namespace Jsonipc;
    JsonapiConnectionP jsonapi_connection_p = std::dynamic_pointer_cast<JsonapiConnection> (shared_from_this());
    assert_return (jsonapi_connection_p);
    std::weak_ptr<JsonapiConnection> selfw = jsonapi_connection_p;
    // marshal remote trigger
    auto trigger_remote = [selfw,id] (ValueS &&args)    // weak_ref avoids cycles
    {
      JsonapiConnectionP selfp = selfw.lock();
      return_unless (selfp);
      const String msg = jsonobject_to_string ("method", id /*"JsonapiTrigger/_%%%"*/, "params", args);
      if (main_config.jsipc)
        selfp->log (string_format ("⬰ %s", msg));
      selfp->send_text (msg);
    };
    JsTrigger trigger = JsTrigger::create (id, trigger_remote);
    triggers_.push_back (trigger);
    // marshall remote destroy notification and erase triggers_ entry
    auto erase_trigger = [selfw, id] ()               // weak_ref avoids cycles
    {
      std::shared_ptr<JsonapiConnection> selfp = selfw.lock();
      return_unless (selfp);
      if (selfp->is_open())
        {
          ValueS args { id };
          const String msg = jsonobject_to_string ("method", "JsonapiTrigger/killed", "params", args);
          if (main_config.jsipc)
            selfp->log (string_format ("↚ %s", msg));
          selfp->send_text (msg);
        }
      Aux::erase_first (selfp->triggers_, [id] (auto &t) { return id == t.id(); });
    };
    trigger.ondestroy (erase_trigger);
  }
  void
  trigger_destroy_hooks()
  {
    std::vector<JsTrigger> old;
    old.swap (triggers_); // speed up erase_trigger() searches
    for (auto &trigger : old)
      trigger.destroy();
    custom_data_destroy();
  }
};

WebSocketConnectionP
jsonapi_make_connection (WebSocketConnection::Internals &internals)
{
  return std::make_shared<JsonapiConnection> (internals);
}

#define ERROR500(WHAT)                                          \
  Jsonipc::bad_invocation (-32500,                              \
                           __FILE__ ":"                         \
                           ASE_CPP_STRINGIFY (__LINE__) ": "    \
                           "Internal Server Error: "            \
                           WHAT)
#define assert_500(c) (__builtin_expect (static_cast<bool> (c), 1) ? (void) 0 : throw ERROR500 (#c) )

static Jsonipc::IpcDispatcher*
make_dispatcher()
{
  using namespace Jsonipc;
  static IpcDispatcher *dispatcher = [] () {
    dispatcher = new IpcDispatcher();
    dispatcher->add_method ("Jsonipc.initialize",
                            [] (CallbackInfo &cbi)
                            {
                              assert_500 (current_message_conection);
                              Server &server = ASE_SERVER;
                              std::shared_ptr<Server> serverp = shared_ptr_cast<Server> (&server);
                              cbi.set_result (to_json (serverp, cbi.allocator()).Move());
                            });
    dispatcher->add_method ("JsonapiTrigger/create",
                            [] (CallbackInfo &cbi)
                            {
                              assert_500 (current_message_conection);
                              const String triggerid = cbi.n_args() == 1 ? from_json<String> (cbi.ntharg (0)) : "";
                              if (triggerid.compare (0, 16, "JsonapiTrigger/_") != 0)
                                throw Jsonipc::bad_invocation (-32602, "Invalid params");
                              current_message_conection->trigger_create (triggerid);
                            });
    dispatcher->add_method ("JsonapiTrigger/remove",
                            [] (CallbackInfo &cbi)
                            {
                              assert_500 (current_message_conection);
                              const String triggerid = cbi.n_args() == 1 ? from_json<String> (cbi.ntharg (0)) : "";
                              if (triggerid.compare (0, 16, "JsonapiTrigger/_") != 0)
                                throw Jsonipc::bad_invocation (-32602, "Invalid params");
                              current_message_conection->trigger_remove (triggerid);
                            });
    return dispatcher;
  } ();
  return dispatcher;
}

String
JsonapiConnection::handle_jsonipc (const std::string &message)
{
  const bool clog = main_config.jsipc;
  if (clog)
    log (string_format ("→ %s", message));
  Jsonipc::Scope message_scope (imap_, Jsonipc::Scope::PURGE_TEMPORARIES);
  const std::string reply = make_dispatcher()->dispatch_message (message);
  if (clog)
    {
      const char *errorat = strstr (reply.c_str(), "\"error\":{");
      if (errorat && errorat > reply.c_str() && (errorat[-1] == ',' || errorat[-1] == '{'))
        {
          using namespace AnsiColors;
          auto R1 = color (BOLD) + color (FG_RED), R0 = color (FG_DEFAULT) + color (BOLD_OFF);
          log (string_format ("%s←%s %s", R1, R0, reply));
        }
      else
        log (string_format ("← %s", reply));
    }
  return reply;
}

// == JsTrigger ==
class JsTrigger::Impl {
  using Func = std::function<void (ValueS)>;
  const String id;
  Func func;
  using VoidFunc = std::function<void()>;
  std::vector<VoidFunc> destroyhooks;
  friend class JsTrigger;
  /*ctor*/ Impl   () = delete;
  /*copy*/ Impl   (const Impl&) = delete;
  Impl& operator= (const Impl&) = delete;
public:
  ~Impl ()
  {
    destroy();
  }
  Impl (const Func &f, const String &_id) :
    id (_id), func (f)
  {}
  void
  destroy ()
  {
    func = nullptr;
    while (!destroyhooks.empty())
      {
        VoidFunc destroyhook = destroyhooks.back();
        destroyhooks.pop_back();
        destroyhook();
      }
  }
};

void
JsTrigger::ondestroy (const VoidFunc &vf)
{
  assert_return (p_);
  if (vf)
    p_->destroyhooks.push_back (vf);
}

void
JsTrigger::call (ValueS &&args) const
{
  assert_return (p_);
  if (p_->func)
    p_->func (std::move (args));
}

JsTrigger
JsTrigger::create (const String &triggerid, const JsTrigger::Impl::Func &f)
{
  JsTrigger trigger;
  trigger.p_ = std::make_shared<JsTrigger::Impl> (f, triggerid);
  assert_return (f != nullptr, trigger);
  return trigger;
}

String
JsTrigger::id () const
{
  return p_ ? p_->id : "";
}

void
JsTrigger::destroy ()
{
  if (p_)
    p_->destroy();
}

JsTrigger::operator bool () const noexcept
{
  return p_ && p_->func;
}

JsTrigger
ConvertJsTrigger::lookup (const String &triggerid)
{
  if (current_message_conection)
    return current_message_conection->trigger_lookup (triggerid);
  assert_return (current_message_conection, {});
  return {};
}

CustomDataContainer*
jsonapi_connection_data ()
{
  if (current_message_conection)
    return current_message_conection.get();
  return nullptr;
}

} // Ase
