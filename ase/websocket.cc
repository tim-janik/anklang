// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "websocket.hh"
#include "platform.hh"
#include "path.hh"
#include "blob.hh"
#include "utils.hh"
#include "regex.hh"
#include "randomhash.hh"
#include "internal.hh"
#include <regex>
#include <fstream>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace Ase {

struct WebSocketServerImpl;
using WebSocketServerImplP = std::shared_ptr<WebSocketServerImpl>;

struct CustomServerConfig : public websocketpp::config::asio {
  static const size_t connection_read_buffer_size = 16384;
};
using WppServer = websocketpp::server<CustomServerConfig>;
using WppConnectionP = WppServer::connection_ptr;
using WppConnection = WppConnectionP::element_type;
using WppHdl = websocketpp::connection_hdl;

static inline bool
operator!= (const WppHdl &a, const WppHdl &b)
{
  return a.owner_before (b) || b.owner_before (a);
}
static inline bool operator== (const WppHdl &a, const WppHdl &b) { return !(a != b); }

// == WebSocketServerImpl ==
struct WebSocketServerImpl : public WebSocketServer {
  using ConVec = std::vector<WebSocketConnectionP>;
  using RegexVector = std::vector<std::regex>;
  std::thread   *initialized_thread_ = nullptr;
  WppServer      wppserver_;
  String         server_url_, dir_;
  std::vector<std::pair<String,String>> aliases_;
  ConVec         opencons_;
  RegexVector    ignores_;
  MakeConnection make_con_;
  int            logflags_ = 0;
  void   setup        (const String &host, int port);
  void   run          ();
  WebSocketConnectionP make_connection (WppHdl hdl);
  WebSocketConnection* force_con (WppHdl);
  friend class WebSocketServer;
public:
  String         url      () const override      { return server_url_; }
  WppConnectionP wppconp  (WppHdl hdl)           { return wppserver_.get_con_from_hdl (hdl); }
  void
  http_dir (const String &path) override
  {
    assert_return (!initialized_thread_);
    dir_ = Path::normalize (path);
    aliases_.clear();
    ignores_.clear();

    // compile .aseignore
    String cxxline;
    std::ifstream aseignore;
    aseignore.open (Path::join (dir_, ".aseignore"));
    while (!aseignore.fail() && std::getline (aseignore, cxxline))
      if (!cxxline.empty())
        ignores_.push_back (std::regex (cxxline));
  }
  void
  http_alias (const String &webdir, const String &path) override
  {
    assert_return (!dir_.empty());
    String aliaspath = Path::normalize (Path::abspath (path, dir_));
    aliases_.push_back (std::make_pair (webdir, aliaspath));
    // sort by URL length, longest URLs come first
    std::stable_sort (aliases_.begin(), aliases_.end(), [] (const auto &a, const auto &b) {
      return a.first.size() > b.first.size();
    });
  }
  String
  map_url (const String &urlpath) override
  {
    if (dir_.empty())
      return "";

    // decode URL, also uncovers '.' and '/'
    String absurl = string_url_decode (urlpath);

    // normalize '.' and '..' dirs
    absurl = Path::simplify_abspath (absurl);

    // ignore urls
    for (const auto &ignorepat : ignores_)
      if (std::regex_search (absurl, ignorepat))
        return "";

    // map URL to sorted aliases, prefers longest match
    for (const auto &pair : aliases_)
      if (absurl == pair.first)
        return pair.second;
      else if (absurl.size() > pair.first.size() &&
               absurl[pair.first.size()] == '/' &&
               strncmp (absurl.c_str(), pair.first.c_str(), pair.first.size()) == 0)
        return Path::join (pair.second, absurl.c_str() + pair.first.size());

    // fallback to root
    return Path::join (dir_, absurl);
  }
  void
  listen (const String &host, int port, const UnlistenCB &ulcb) override
  {
    assert_return (!initialized_thread_);
    setup (host, port);
    initialized_thread_ = new std::thread ([this, ulcb] () {
      this_thread_set_name ("AsioWebSocket");
      this->run();
      if (ulcb)
        ulcb();
    });
  }
  void
  shutdown () override
  {
    if (initialized_thread_)
      {
        wppserver_.stop();
        initialized_thread_->join();
        initialized_thread_ = nullptr;
      }
    reset();
  }
  void ws_opened (WebSocketConnectionP);
  void ws_closed (WebSocketConnectionP);
  void reset         () override;
  WebSocketServerImpl (const MakeConnection &make_con, int logflags) :
    make_con_ (make_con), logflags_ (logflags)
  {}
};

void
WebSocketServerImpl::setup (const String &host, int port)
{
  // setup websocket and run asio loop
  wppserver_.set_user_agent (user_agent());
  wppserver_.set_validate_handler ([this] (WppHdl hdl) {
    WebSocketConnectionP conp = make_connection (hdl);
    WppConnectionP cp = this->wppconp (hdl);
    return_unless (conp && cp, false);
    const int index = conp->validate();
    if (index >= 0)
      {
        const std::vector<std::string> &subprotocols = cp->get_requested_subprotocols();
        if (size_t (index) < subprotocols.size())
          {
            cp->select_subprotocol (subprotocols[index]);
            return true;
          }
        else if (subprotocols.size() == 0 && index == 0)
          return true;
      }
    cp->set_status (websocketpp::http::status_code::forbidden);
    return false;
  });
  wppserver_.set_http_handler ([this] (WppHdl hdl) {
    WebSocketConnectionP conp = make_connection (hdl);
    if (conp)
      conp->http_request();
  });
  wppserver_.init_asio();
  wppserver_.clear_access_channels (websocketpp::log::alevel::all);
  wppserver_.clear_error_channels (websocketpp::log::elevel::all);
  wppserver_.set_reuse_addr (true);

  // listen on localhost
  namespace IP = boost::asio::ip;
  IP::tcp::endpoint endpoint_local = IP::tcp::endpoint (IP::address::from_string (host), port);
  websocketpp::lib::error_code ec;
  wppserver_.listen (endpoint_local, ec);
  if (ec)
    fatal_error ("failed to listen on socket: %s:%d: %s", host, port, ec.message());
  if (port == 0)
    {
      websocketpp::lib::asio::error_code ac;
      port = wppserver_.get_local_endpoint (ac).port();
    }
  String fullurl = string_format ("http://%s:%d/", host, port);
  server_url_ = fullurl;
}

void
WebSocketServerImpl::run ()
{
  wppserver_.start_accept();
  wppserver_.run();
}

// == WebSocketConnection ==
struct WebSocketConnection::Internals {
  WebSocketServerImplP server;
  WppServer           &wppserver;
  WppHdl               hdl;
  String               nickname_;
  bool                 opened = false;
  WppConnectionP       wppconp()        { return server->wppconp (hdl); }
  friend struct WebSocketServerImpl;
};

WebSocketConnection::WebSocketConnection (Internals &internals, int logflags) :
  internals_ (*new (internals_mem_) Internals (internals)), logflags_ (logflags)
{
  static_assert (sizeof (Internals) <= sizeof (internals_mem_));
  assert_return (internals_.server != nullptr);
}

WebSocketConnection::~WebSocketConnection()
{
  internals_.~Internals();
}

bool
WebSocketConnection::is_open () const
{
  return internals_.opened;
}

bool
WebSocketConnection::send_text (const String &message)
{
  assert_return (!message.empty(), false);
  WppConnectionP cp = internals_.wppconp();
  return_unless (cp, false);
  websocketpp::lib::error_code ec;
  internals_.wppserver.send (internals_.hdl, message, websocketpp::frame::opcode::text, ec);
  if (ec)
    {
      if (logflags_ > 0)
        log (string_format ("Error: %s: %s", __func__, ec.message()));
      websocketpp::lib::error_code ec2;
      cp->close (websocketpp::close::status::going_away, "", ec2);
      return false;
    }
  return true;
}

bool
WebSocketConnection::send_binary (const String &blob)
{
  WppConnectionP cp = internals_.wppconp();
  return_unless (cp, false);
  websocketpp::lib::error_code ec;
  // See "Sending Messages" about `endpoint::send` in utility_client.md
  internals_.wppserver.send (internals_.hdl, blob, websocketpp::frame::opcode::binary, ec); // MT-Safe, locks mutex
  if (ec)
    {
      if (logflags_ > 0)
        log (string_format ("Error: %s: %s", __func__, ec.message()));
      websocketpp::lib::error_code ec2;
      cp->close (websocketpp::close::status::going_away, "", ec2);
      return false;   // invalid connection or send failed
    }
  if (ASE_UNLIKELY (logflags_ & 256))
    {
      String hex;
      for (size_t i = 0; i < blob.size(); i++)
        {
          if (i && 0 == i % 16)
            hex += "\n ";
          else if (0 == i % 8)
            hex += " ";
          hex += string_format (" %02x", blob[i]);
        }
      log (string_format ("⇜ Blob: len=%d hash=%016x\n%s", blob.size(), fnv1a_consthash64 (blob.data(), blob.size()), hex));
    }
  return true; // connection alive and message queued
}

WebSocketConnection::Info
WebSocketConnection::get_info ()
{
  WppConnectionP cp = internals_.wppconp();
  const websocketpp::http::parser::request &rq = cp->get_request();
  auto headers = std::make_shared<websocketpp::http::parser::header_list> (rq.get_headers());
  Info info;
  info.header = [headers] (const String &header) {
    const auto it = headers->find (header);
    return it == headers->end() ? "" : it->second;
  };
  if (0)
    for (auto it : *headers)
      printerr ("%s: %s\n", it.first, it.second);
  // https://github.com/zaphoyd/websocketpp/issues/694#issuecomment-454623641
  const auto &socket = cp->get_raw_socket();
  const auto &laddress = socket.local_endpoint().address();
  info.local = laddress.to_string();
  info.lport = socket.local_endpoint().port();
  const auto &raddress = socket.remote_endpoint().address();
  info.remote = raddress.to_string();
  info.rport = socket.remote_endpoint().port();
  info.subs = cp->get_requested_subprotocols();
  return info;
}

String
WebSocketConnection::nickname ()
{
  if (internals_.nickname_.empty())
    {
      Info info = get_info();
      const String ua = info.header ("User-Agent");
      String s = info.local + ":" + string_from_int (info.lport) + "\n" +
                 info.remote + ":" + string_from_int (info.rport * 0) + "\n" +
                 ua + "\n" +
                 info.header ("Accept-Encoding") + "\n" +
                 info.header ("Accept-Language") + "\n" +
                 info.header ("sec-ch-ua") + "\n" +
                 info.header ("sec-ch-ua-mobile") + "\n" +
                 info.header ("sec-gpc") + "\n";
      String hh;
      uint64_t hash = string_hash64 (s);
      if      (Re::search (R"(\bFirefox/)", ua) >= 0)
        hh = "FF";
      else if (Re::search (R"(\bElectron/)", ua) >= 0)
        hh = "El";
      else if (Re::search (R"(\bChrome-Lighthouse\b)", ua) >= 0)
        hh = "Lh";
      else if (Re::search (R"(\bChrome/)", ua) >= 0)
        hh = "Ch";
      else if (Re::search (R"(\bSafari/)", ua) >= 0)
        hh = "Sa";
      else
        hh = "Uk";
      internals_.nickname_ = string_format ("%s-%08x:%x", hh, uint32_t (hash ^ (hash >> 32)), info.rport);
    }
  return internals_.nickname_;
}

int  WebSocketConnection::validate ()                           { return -1; }
void WebSocketConnection::failed ()                             { if (logflags_ & 2) log (__func__); }
void WebSocketConnection::opened ()                             { if (logflags_ & 4) log (__func__); }
void WebSocketConnection::message (const String &message)       { if (logflags_ & 8) log (__func__); }
void WebSocketConnection::closed ()                             { if (logflags_ & 4) log (__func__); }
void WebSocketConnection::log (const String &message)           { printerr ("%s\n", message);}

void
WebSocketConnection::http_request ()
{
  if (internals_.server->dir_.empty())
    return;
  WppConnectionP cp = internals_.wppconp();
  const auto &parts = string_split (cp->get_resource(), "?");

  // find file for URL
  String filepath = internals_.server->map_url (parts[0]);

  // map directories to index.html
  if (!filepath.empty() && Path::check (filepath, "dx"))
    filepath = Path::join (filepath, "index.html");

  // serve existing files
  websocketpp::http::status_code::value status;
  if (!filepath.empty() && Path::check (filepath, "fr"))
    {
      const char *ext = strrchr (filepath.c_str(), '.');
      const String mimetype = WebSocketServer::mime_type (ext ? ext + 1 : "", true);
      cp->append_header ("Content-Type", mimetype.c_str());
      cp->append_header ("Cache-Control", "no-store, max-age=0");
      // cp->append_header ("Cache-Control", "public, max-age=604800, immutable");
      Blob blob = Blob::from_file (filepath);
      cp->set_body (blob.string());
      status = websocketpp::http::status_code::ok;
    }
  else // 404
    {
      cp->append_header ("Content-Type", "text/html; charset=utf-8");
      cp->set_body ("<!DOCTYPE html>\n"
                    "<html><head><title>404 Not Found</title></head><body>\n"
                    "<h1>Not Found</h1>\n"
                    "<p>The requested URL was not found: <tt>" + cp->get_uri()->str() + "</tt></p>\n"
                    "<hr><address>" + WebSocketServer::user_agent() + "</address>\n"
                    "<hr></body></html>\n");
      status = websocketpp::http::status_code::not_found;
    }
  cp->set_status (status);
  if (logflags_ & 16)
    {
      using namespace AnsiColors;
      String C1 = status >= 400 && status <= 499 ? color (FG_RED) : "";
      String C0 = status >= 400 && status <= 499 ? color (RESET) : "";
      log (string_format ("%s%d %s %s%s%s",
                          C1, int (status),
                          cp->get_request().get_method(), cp->get_resource(), C0,
                          filepath.empty() ? " [IGNORE]" : ""));
    }
}

// == WebSocketServerImpl ==
WebSocketConnectionP
WebSocketServerImpl::make_connection (WppHdl hdl)
{
  WppConnectionP cp = wppconp (hdl);
  assert_return (cp, nullptr);
  WebSocketConnection::Internals internals {
    std::dynamic_pointer_cast<WebSocketServerImpl> (shared_from_this()),
    wppserver_, hdl,
  };
  WebSocketConnectionP conp = make_con_ (internals, logflags_);
  // capturing conp in handler contexts keeps it alive long enough for calls
  cp->set_http_handler ([conp] (WppHdl hdl) {
    WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
    assert_return (hdl == internals_.hdl);
    conp->http_request();
  });
  // cp->set_validate_handler (...); - too late, validate_handler calls make_connection()
  cp->set_fail_handler ([conp] (WppHdl hdl) {
    WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
    assert_return (hdl == internals_.hdl);
    conp->failed();
  });
  cp->set_open_handler  ([conp] (WppHdl hdl) {
    WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
    assert_return (hdl == internals_.hdl);
    internals_.server->ws_opened (conp);
  });
  cp->set_message_handler ([conp] (WppHdl hdl, WppServer::message_ptr msg) {
    WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
    assert_return (hdl == internals_.hdl);
    conp->message (msg->get_payload());
  });
  cp->set_close_handler ([conp] (WppHdl hdl) {
    WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
    assert_return (hdl == internals_.hdl);
    internals_.server->ws_closed (conp);
  });
  return conp;
}

void
WebSocketServerImpl::ws_opened (WebSocketConnectionP conp)
{
  WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
  internals_.opened = true;
  opencons_.push_back (conp);
  conp->opened ();
}

void
WebSocketServerImpl::ws_closed (WebSocketConnectionP conp)
{
  WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
  if (internals_.opened)
    {
      internals_.opened = false;
      auto it = std::find (opencons_.begin(), opencons_.end(), conp);
      if (it != opencons_.end())
        opencons_.erase (it);
      conp->closed ();
    }
}

void
WebSocketServerImpl::reset()
{
  // stop open asio connections
  for (ssize_t i = ssize_t (opencons_.size()) - 1; i >= 0; i--)
    {
      WebSocketConnectionP conp = opencons_[i];
      WebSocketConnection::Internals &internals_ = WebSocketServer::internals (*conp);
      WppConnectionP cp = internals_.wppconp();
      websocketpp::lib::error_code ec;
      cp->close (websocketpp::close::status::going_away, "", ec); // omit_handshake
      (void) ec;
    }
}

// == WebSocketServer ==
WebSocketServer::~WebSocketServer()
{}

WebSocketServerP
WebSocketServer::create (const MakeConnection &make, int logflags)
{
  return std::make_shared<WebSocketServerImpl> (make, logflags);
}

String
WebSocketServer::user_agent ()
{
  return String ("AnklangSynthEngine/") + ase_version();
}

#include "mime-types.hh"        // static const char mime_types[];

String
WebSocketServer::mime_type (const String &ext, bool utf8)
{
  using MimeMap = std::unordered_map<String, String>;
  static MimeMap mime_map = [] () {
    MimeMap mime_map;
    // mime_types, list of "mime/type ext ext2\n" lines
    for (const String &line : string_split (mime_types, "\n"))
      {
        const StringS w = string_split (line, " ");
        for (size_t i = 1; i < w.size(); i++)
          if (!w[i].empty())
            {
              if (mime_map.end() != mime_map.find (w[i]))
                warning ("mime-types.hh: duplicate extension: %s", w[i]);
              mime_map[w[i]] = w[0];
            }
      }
    return mime_map;
  } ();
  auto it = mime_map.find (ext);
  String mimetype = it != mime_map.end() ? it->second : "application/octet-stream";
  if (utf8)
    {
      if (mimetype == "text/html" || mimetype == "text/markdown" || mimetype == "text/plain")
        mimetype += "; charset=utf-8";
    }
  return mimetype;
}

} // Ase
