// Licensed CC0 Public Domain: http://creativecommons.org/publicdomain/zero/1.0

/// WebSocket handling coded needed for the Jsonipc wire marshalling
export const Jsonipc = {
  pdefine: globalThis.Object.defineProperty,
  ofreeze: globalThis.Object.freeze,
  okeys: globalThis.Object.keys,
  classes: {},
  receivers: {},
  authresult: undefined,
  web_socket: null,
  counter: null,
  idmap: {},

  /// Open the Jsonipc websocket
  open (url, protocols, options = {}) {
    if (this.web_socket)
      throw "Jsonipc: connection open";
    this.counter = 1000000 * globalThis.Math.floor (100 + 899 * globalThis.Math.random());
    this.idmap = {};
    this.web_socket = new globalThis.WebSocket (url, protocols);
    this.web_socket.binaryType = 'arraybuffer';
    // this.web_socket.onerror = (event) => { throw event; };
    if (options.onclose)
      this.web_socket.onclose = options.onclose;
    this.web_socket.onmessage = this.socket_message.bind (this);
    const promise = new globalThis.Promise (resolve => {
      this.web_socket.onopen = (event) => {
	const psend = this.send ('Jsonipc.initialize', []);
	psend.then (result => { this.authresult = result; resolve (this.authresult); });
      };
    });
    return promise;
  },

  /// Send a Jsonipc request
  async send (methodname, args) {
    if (!this.web_socket)
      throw "Jsonipc: connection closed";
    const unwrap_args = (e, i, a) => {
      if (globalThis.Array.isArray (e))
	e.forEach (unwrap_args);
      else if (null === e)
	;
      else if ('object' === typeof e)
	{
	  if (e.$id > 0)
	    a[i] = { '$id': e.$id }; // unwrap object
	  else
	    for (let key of Jsonipc.okeys (e))
	      unwrap_args (e[key], key, e);
	}
    };
    const wrap_args = (e, i, a) => {
      if (globalThis.Array.isArray (e))
	e.forEach (wrap_args);
      else if (e && 'object' === typeof e)
	{
	  if (e.$class)
	    a[i] = wrap_arg (e);
	  else
	    for (let key of Jsonipc.okeys (e))
	      wrap_args (e[key], key, e);
	}
    };
    const wrap_arg = (e) => {
      if (e && 'object' === typeof e && e.$class && e.$id)
	{
	  const JsClass = this.classes[e.$class];
	  if (JsClass)
	    return new JsClass (e.$id);
	}
      return e;
    };
    args.forEach (unwrap_args);
    const request_id = ++this.counter;
    this.web_socket.send (globalThis.JSON.stringify ({ id: request_id, method: methodname, params: args }));
    const reply_promise = new globalThis.Promise (resolve => { this.idmap[request_id] = resolve; });
    const msg = await reply_promise;
    if (msg.error)
      throw globalThis.Error (
	`${msg.error.code}: ${msg.error.message}\n` +
	`Request: {"id":${request_id},"method":"${methodname}",…}\n` +
	"Reply: " + globalThis.JSON.stringify (msg)
      );
    let r = msg.result;
    if (globalThis.Array.isArray (r))
      r.forEach (wrap_args);
    else
      r = wrap_arg (r);
    return r;
  },

  /// Observe Jsonipc notifications
  receive (methodname, handler) {
    if (handler)
      this.receivers[methodname] = handler;
    else
      delete this.receivers[methodname];
  },

  /// Handle a Jsonipc message
  socket_message (event) {
    // Binary message
    if (event.data instanceof globalThis.ArrayBuffer)
      {
	const handler = this.onbinary;
	if (handler)
	  handler (event.data);
	else
	  console.error ("Unhandled message event:", event);
	return;
      }
    // Text message
    const msg = globalThis.JSON.parse (event.data);
    if (msg.id)
      {
	const handler = this.idmap[msg.id];
	delete this.idmap[msg.id];
	if (handler)
	  return handler (msg);
      }
    else if ("string" === typeof msg.method && globalThis.Array.isArray (msg.params)) // notification
      {
	const receiver = this.receivers[msg.method];
	if (receiver)
	  receiver.apply (null, msg.params);
	return;
      }
    console.error ("Unhandled message:", event.data);
  },
};

// ----- End of jsonipc/jsonipc.js -----
