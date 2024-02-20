// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_WEBSOCKET_HH__
#define __ASE_WEBSOCKET_HH__

#include <ase/cxxaux.hh>

namespace Ase {

class WebSocketServer;
using WebSocketServerP = std::shared_ptr<WebSocketServer>;

class WebSocketConnection : public virtual std::enable_shared_from_this<WebSocketConnection> {
  friend class WebSocketServer;
protected:
  virtual ~WebSocketConnection () = 0;
  struct Info { std::function<String (String)> header; StringS subs; String local, remote; int lport = 0, rport = 0; };
  Info           get_info      ();
public:
  bool           is_open       () const;
  String         nickname      ();
  virtual int    validate      ();                              ///< Return true to allow opened().
  virtual void   failed        ();                              ///< Never folloed by opened().
  virtual void   opened        ();                              ///< Pairs with closed().
  virtual void   http_request  ();                              ///< Only if opened.
  virtual void   message       (const String &message);         ///< Only if opened.
  virtual void   closed        ();                              ///< Pairs with opened().
  virtual void   log           (const String &message);
  bool           send_text     (const String &message);         ///< Returns true if text message was sent.
  bool           send_binary   (const String &blob);            ///< Returns true if binary blob was sent.
  struct Internals;
private:
  Internals &internals_;
  uint64_t internals_mem_[17];
protected:
  explicit WebSocketConnection (Internals &internals, int logflags);
  const int logflags_ = 0;
};
using WebSocketConnectionP = std::shared_ptr<WebSocketConnection>;

class WebSocketServer : public virtual std::enable_shared_from_this<WebSocketServer> {
protected:
  static WebSocketConnection::Internals& internals (WebSocketConnection &c) { return c.internals_; }
  virtual ~WebSocketServer();
public:
  using MakeConnection = std::function<WebSocketConnectionP (WebSocketConnection::Internals&, int)>;
  using UnlistenCB = std::function<void ()>;
  virtual void            http_dir      (const String &path) = 0;
  virtual void            http_alias    (const String &webdir, const String &path) = 0;
  virtual String          map_url       (const String &urlpath) = 0;
  virtual std::string     url           () const = 0;
  virtual void            listen        (const String &host = "", int port = 0, const UnlistenCB& = {}) = 0;
  virtual void            reset         () = 0;
  virtual void            shutdown      () = 0;
  static WebSocketServerP create        (const MakeConnection &make, int logflags = 0);
  static String           user_agent    ();
  static String           mime_type     (const String &ext, bool utf8);
  static bool             utf8_validate (const std::string &utf8string);
};

} // Ase

#endif // __ASE_WEBSOCKET_HH__
