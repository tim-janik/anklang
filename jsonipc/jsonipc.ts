// Dedicated to the Public Domain under the Unlicense: https://unlicense.org/UNLICENSE

/// WebSocket handling coded needed for the Jsonipc wire marshalling
export const Jsonipc = {
  pdefine: globalThis.Object.defineProperty,
  ofreeze: globalThis.Object.freeze,
  okeys: globalThis.Object.keys,
  finalization_registration: () => undefined,           // hook for downstream finalizer installation
  classes: {},
  receivers: {},
  onbinary: null,
  authresult: undefined,
  web_socket: null,
  counter: null,
  idmap: {},

  /// Registry to run cleanup callbacks once an instance was garbage collected
  cleanup_array_registry: new FinalizationRegistry (callback_array => {
    while (callback_array.length)
      callback_array.pop().call();
    // Note, verify this is called when altering $props.$weakthis & co
  }),

  /// Install auto-fetching for prop and get its value
  get_reactive_prop (prop, dflt) {
    const this_props = this.$props;
    // install prop if needed
    if (!this_props[prop]) {
      // install $props system if needed
      if (!this_props.$unwatchers) {
        this_props.$weakthis = new WeakRef (this);      // helper to not keep `this` alive
        this_props.$promise = null;                     // present if $promise !== undefined
        this_props.$unwatchers = [ clean_this_props ];
        Jsonipc.cleanup_array_registry.register (this, this_props.$unwatchers, this_props.$unwatchers);
        //this_props.$id = this.$id;                                     // DEBUG $id GC
        function clean_this_props() {
          //console.log ("GC: $id=" + this_props.$id, "delete $props");  // DEBUG $id GC
          for (let k of Jsonipc.okeys (this_props))
            delete this_props[k];                       // allow GC for all fields
        }
        // We use this_props + $weakthis instead of `this` as object/data handle, to allow GC
        // of `this`, which in turn calls all $props.$unwatchers[].
      }
      this_props[prop] = new globalThis.Signal.State (dflt); // cached state
      // refetch, maintain $promise while waiting
      function refetch_prop () {                        // async, returns Promise, avoid keeping `this` alive
        let fetch_promise;
        const last_promise = this_props.$promise;
        async function async_fetch_prop () {
          const self = this_props.$weakthis.deref();
          if (!self) return;
          let result = Jsonipc.send ('get/' + prop, [self]);  // `this`
          if (last_promise)
            await last_promise;                         // sync with last, before $promise reset
          result = await result;
          if (this_props.$promise === fetch_promise)
            this_props.$promise = null;                 // reset, this call is just being resolved
          this_props[prop].set (result);                // assign new value after reset, otherwise callbacks might see stale promise
        }
        // start fetching and remember call promise
        this_props.$promise = fetch_promise = async_fetch_prop();
        return fetch_promise;
      }
      const delnotifier = this.on ("notify:" + prop, refetch_prop);
      this_props.$unwatchers.push (delnotifier);
      refetch_prop();
    }
    // fetch its cached value
    return this_props[prop].get();
  },

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
    const promise = new globalThis.Promise ((resolve,reject) => {
      this.web_socket.onopen = (event) => {
	const psend = this.send ('Jsonipc/handshake', []);
	psend.then (result => {
	  this.authresult = result;
	  const protocol = 0x00000001;
	  if (this.authresult == protocol)
	    resolve (true);
	  else
	    reject ("invalid protocoal (" + this.authresult + "), expected: " + protocol);
	});
      };
    });
    return promise;
  },

  Jsonipc_objects: [],

  Jsonipc_prototype: class {
    constructor ($id) {
      Jsonipc.pdefine (this, '$id', { value: $id });
      Jsonipc.pdefine (this, '$props', { value: {} });
      Jsonipc.finalization_registration (this);
      // Note that Vue recursively invades *all* objects used in a Vue component,
      // which ultimately leads to Signal.get choking on being called on a Proxy.
      // Thus, for the time being, we have to freeze `this`.
    }
    // JSON.stringify replacer
    toJSON() {
      return { $id: this.$id };
    }
    // JSON.parse reviver
    static fromJSON (key, value) {
      if (value?.$id > 0) {
	const JsClass = Jsonipc.classes[value.$class];
	if (JsClass) {
	  let obj = Jsonipc.Jsonipc_objects[value.$id]?.deref();
	  if (!obj) {
	    obj = new JsClass (value.$id);
	    Jsonipc.Jsonipc_objects[value.$id] = new WeakRef (obj);
	  }
	  return obj;
	}
      }
      return value;
    }
  },

  /// Send a Jsonipc request
  send (method, params) {
    if (!this.web_socket)
      throw "Jsonipc: connection closed";
    const id = ++this.counter;
    let send_promise;                                   // promise to sync with this call
    const this_props = params?.[0]?.$props;             // avoid keeping method's `this` alive
    const last_promise = this_props?.$promise;
    const send_async = async (method, params) => {
      this.web_socket.send (globalThis.JSON.stringify ({ id, method, params }));
      const register_reply_handler = resolve => this.idmap[id] = resolve;
      let msg = new globalThis.Promise (register_reply_handler);
      if (last_promise)
	await last_promise;
      msg = await msg;
      if (last_promise !== undefined && this_props.$promise === send_promise)
	this_props.$promise = null;                     // reset, this call is just being resolved
      if (msg.error)
	throw globalThis.Error (
	  `${msg.error.code}: ${msg.error.message}\n` +
	  `Request: {"id":${id},"method":"${method}",…}\n` +
	  "Reply: " + globalThis.JSON.stringify (msg)
	);
      return msg.result;
    };
    send_promise = send_async (method, params);
    if (last_promise !== undefined)
      this_props.$promise = send_promise;               // chain this call with last promise
    return send_promise;
  },

  /// Observe Jsonipc notifications
  receive (methodname, handler) {
    if (handler)
      this.receivers[methodname] = handler;
    else
      delete this.receivers[methodname];
  },

  /// Handle binary messages
  handle_binary (handler) {
    this.onbinary = handler ? handler : null;
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
	  globalThis.console.error ("Unhandled message event:", event);
	return;
      }
    // Text message
    const maybe_prototype = event.data.indexOf ('"$class":"') >= 0;
    const msg = globalThis.JSON.parse (event.data, maybe_prototype ? Jsonipc.Jsonipc_prototype.fromJSON : null);
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
    globalThis.console.error ("Unhandled message:", event.data);
  },

  /// Simplify initialization of globals
  setup_promise_type (type, resolved = undefined) {
    let resolve;
    const p = new Promise (r => resolve = r);
    p.__resolve__ = instance => {
      if (instance instanceof type) {
	resolve (instance);
	if (resolved)
	  resolved (instance);
      }
    };
    return p;
  },
};

// ----- End of jsonipc/jsonipc.js -----
