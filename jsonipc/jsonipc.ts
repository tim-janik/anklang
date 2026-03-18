// Dedicated to the Public Domain under the Unlicense: https://unlicense.org/UNLICENSE

// For Callback handling, this assumes `Signal` is available in the global scope
interface SignalState<T> {
  get (): T;
  set (newValue: T): void;
};
declare const Signal: {
  State: new <T> (dflt: T) => SignalState<T>;
};
type CleanupCallback = () => void;
interface JsonipcMessage {
  id?: number;
  method?: string;
  params?: any[];
  result?: any;
  error?: {
    code: number;
    message: string;
  };
};

type JsonipcEnum = { readonly [key: string]: string };
type JsonipcRecord = new (...args: any[]) => any;
interface JsonipcPrototype {
  $id: number;
  $props: {
    [key: string]: any;
    $weakthis?: WeakRef<JsonipcPrototype>;
    $promise?: Promise<any> | null;
    $unwatchers?: CleanupCallback[];
  };
  toJSON(): { $id: number };
  $rpc (method: string, params: any[]): Promise<any>;
  $asyncs(): Promise<void>;	// Wait for all pending async operations to complete
  // Mixin method for classes with event handling
  on (event: string, callback: (...args: any[]) => void): () => void;
};
interface JsonipcClass {
  new ($id: number): JsonipcPrototype;
};
type JsonipcEntity = JsonipcClass | JsonipcRecord | JsonipcEnum;

type Receiver = (...args: any[]) => void;

// WebSocket handling code for Jsonipc wire marshalling
export const Jsonipc = {
  pdefine: globalThis.Object.defineProperty,
  ofreeze: globalThis.Object.freeze,
  okeys: globalThis.Object.keys,
  finalization_registration: (target: object): void => {}, // hook for downstream finalizer installation
  classes: {} as { [key: string]: JsonipcEntity },
  receivers: {} as { [key: string]: Receiver },
  onbinary: null as ((data: ArrayBuffer) => void),
  authresult: -1,
  web_socket: null as WebSocket,
  counter: 0,
  idmap: {} as { [key: number]: (msg: JsonipcMessage) => void },

  /// Registry to run cleanup callbacks once an instance was garbage collected
  cleanup_array_registry: new FinalizationRegistry<CleanupCallback[]> ((callback_array: CleanupCallback[]) =>
  {
    while (callback_array.length)
      callback_array.pop().call (undefined);
    // Note, verify this is called when altering $props.$weakthis & co
  }),

  /// Set a reactive property remotely and return promise for completion
  set_reactive_prop<T> (this: any, prop: string, val: T): Promise<any>
  {
    const promise = Jsonipc.send ('set/' + prop, [this, val]);
    Jsonipc.add_prop_promise.call (this, promise);
    return promise;
  },

  /// Chain a new promise to the object's async operation queue
  add_prop_promise<T> (this: any, nextpromise: Promise<any>)
  {
    if (!this.$props.$promise) {
      this.$props.$promise = nextpromise;
      return this.$props.$promise;
    }
    // Create a WeakRef to the object so it can be GC'd even if promises hold references
    const weakthis = Jsonipc.ensure_props.call (this).$weakthis;
    // Chain the existing promise with the new one
    const wrapperpromise = Promise.all ([this.$props.$promise, nextpromise]);
    wrapperpromise.then (([_, nextresult]) =>
      {
        // Dereference the object - may be null if GC'd
        const obj = weakthis.deref();
        if (obj?.$props?.$promise === wrapperpromise)
	  obj.$props.$promise = null;	// breaks references for GC
        return nextresult;
      });
    wrapperpromise.catch (exc => {
      const obj = weakthis.deref();
      if (obj?.$props?.$promise === wrapperpromise)
        obj.$props.$promise = null;
      throw exc;
    });
    // assign synchronization point
    this.$props.$promise = wrapperpromise;
    return this.$props.$promise;
  },

  /// Install auto-fetching for prop and get its value
  ensure_props (this: any): any
  {
    const this_props = this.$props;
    if (!this_props.$weakthis) {
      // install $props system
      this_props.$weakthis = new WeakRef (this);	// helper to not keep `this` alive
      this_props.$promise = null;			// present if $promise !== undefined
      //this_props.$id = this.$id;			// DEBUG $id GC
      const clean_this_props = (): void => {
        //console.log ("GC: $id=" + this_props.$id, "delete $props");	// DEBUG $id GC
        for (const k of Jsonipc.okeys (this_props))
          delete this_props[k];				// allow GC for all fields
      };
      this_props.$unwatchers = [ clean_this_props ];
      Jsonipc.cleanup_array_registry.register (this, this_props.$unwatchers, this_props.$unwatchers);
      // We use this_props + $weakthis instead of `this` as object/data handle,
      // to allow GC of `this`, which in turn calls all $props.$unwatchers[].
    }
    return this_props;
  },

  /// Install auto-fetching for prop and get its value
  get_reactive_prop<T> (this: any, prop: string, dflt: T): T
  {
    const this_props = Jsonipc.ensure_props.call (this);
    // install prop if needed
    if (!this_props[prop]) {
      this_props[prop] = new globalThis.Signal.State (dflt); // cached state
      const refetch_prop = (): Promise<T> =>
      {
        const async_fetch_prop = async (): Promise<void> =>
	{
          const self = this_props.$weakthis?.deref();
          if (!self) return;
          const result_promise = Jsonipc.send ('get/' + prop, [self]);
          const result = await result_promise;
          const signal_state = this_props[prop] as SignalState<T>;
	  signal_state.set (result);			// assign new value
        };
        return Jsonipc.add_prop_promise.call (this, async_fetch_prop());
      };
      const delnotifier = this.on ("notify:" + prop, refetch_prop);
      this_props.$unwatchers.push (delnotifier);
      refetch_prop();
    }
    // fetch its cached value
    return (this_props[prop] as SignalState<T>).get();
  },

  /// Open the Jsonipc websocket
  open (url: string, protocols?: string | string[], options: { onclose?: (event: CloseEvent) => void } = {}): Promise<boolean>
  {
    if (this.web_socket)
      throw globalThis.Error ("Jsonipc: connection open");
    this.counter = 1000000 * globalThis.Math.floor (100 + 899 * globalThis.Math.random());
    this.idmap = {};
    this.web_socket = new globalThis.WebSocket (url, protocols);
    this.web_socket.binaryType = 'arraybuffer';
    // this.web_socket.onerror = (event) => { throw event; };
    if (options.onclose)
      this.web_socket.onclose = options.onclose;
    this.web_socket.onmessage = this.socket_message.bind (this);
    const promise = new globalThis.Promise<boolean> ((resolve, reject) => {
      this.web_socket.onopen = (): void => {
        const psend = this.send ('Jsonipc/handshake', []);
        psend.then ((result: any) => {
          this.authresult = result;
          const protocol = 0x00000001;
          if (this.authresult === protocol)
            resolve (true);
          else
            reject ("invalid protocol (" + this.authresult + "), expected: " + protocol);
        });
      }
    });
    return promise;
  },

  Jsonipc_objects: [] as (WeakRef<JsonipcPrototype> | undefined)[],

  Jsonipc_prototype: class implements Omit<JsonipcPrototype, 'on'> {
    $id: number;
    $props: {
      [key: string]: any;
      $weakthis?: WeakRef<JsonipcPrototype>;
      $promise?: Promise<any> | null;
      $unwatchers?: CleanupCallback[];
    };
    constructor ($id: number)
    {
      Jsonipc.pdefine (this, '$id', { value: $id });
      Jsonipc.pdefine (this, '$props', { value: {} });
      Jsonipc.finalization_registration (this);
      // Note that Vue recursively invades *all* objects used in a Vue component,
      // which ultimately leads to Signal.get choking on being called on a Proxy.
      // Thus, for the time being, we have to freeze `this`.
    }
    // JSON.stringify replacer
    toJSON(): { $id: number }
    {
      return { $id: this.$id };
    }
    // Send a Jsonipc request and await notifications
    async $rpc (method: string, params: any[]): Promise<any>
    {
      const result = await Jsonipc.send (method, params);
      if (this.$props.$promise)	// pending notifications
	await this.$asyncs();	// await delivery
      return result;
    }
    /// Get a reactive property value (fetches if needed)
    $get<T> (prop: string, dflt: T): T
    {
      return Jsonipc.get_reactive_prop.call (this, prop, dflt) as T;
    }
    /// Set a reactive property remotely and await completion
    async $set<T> (prop: string, val: T): Promise<any>
    {
      return await Jsonipc.set_reactive_prop.call (this, prop, val);
    }
    // Wait for all pending async operations to complete
    async $asyncs()
    {
      const last_promise = this.$props.$promise;
      // NOP if no async operation is pending
      if (!last_promise)
	return;
      // await completion of any running async modifiocations
      await last_promise;
      // a new promise likely indicates the refetch() of a past modification
      if (this.$props.$promise && last_promise !== this.$props.$promise)
	await this.$props.$promise;
    }
    // JSON.parse reviver
    static fromJSON (key: string, value: any): any
    {
      if (value?.$id > 0) {
        const JsClass = Jsonipc.classes[value.$class];
        if (JsClass && typeof JsClass === 'function') {
          let obj = Jsonipc.Jsonipc_objects[value.$id]?.deref();
          if (!obj) {
            obj = new JsClass (value.$id);
            Jsonipc.Jsonipc_objects[value.$id] = new WeakRef (obj as JsonipcPrototype);
          }
          return obj;
        }
      }
      return value;
    }
  },

  /// Send a Jsonipc request
  send (method: string, params: any[]): Promise<any>
  {
    if (!this.web_socket)
      throw globalThis.Error ("Jsonipc: connection closed");
    const id = ++this.counter;
    let send_promise: Promise<any>;			// promise to sync with this call
    const this_props = params?.[0]?.$props as JsonipcPrototype['$props'] | undefined; // avoid keeping method's `this` alive
    const last_promise = this_props?.$promise;
    const send_async = async (method: string, params: any[]): Promise<any> => {
      this.web_socket.send (globalThis.JSON.stringify ({ id, method, params }));
      const msg_promise = new globalThis.Promise<JsonipcMessage> (resolve => this.idmap[id] = resolve);
      if (last_promise)
        await last_promise;
      const resolved_msg = await msg_promise;
      if (last_promise !== undefined && this_props?.$promise === send_promise)
        this_props.$promise = null;			// reset, this call has just been resolved
      if (resolved_msg.error)
        throw globalThis.Error (
          `${resolved_msg.error.code}: ${resolved_msg.error.message}\n` +
          `Request: {"id":${id},"method":"${method}",…}\n` +
          "Reply: " + globalThis.JSON.stringify (resolved_msg)
        );
      return resolved_msg.result;
    };
    send_promise = send_async (method, params);
    if (last_promise !== undefined)
      this_props.$promise = send_promise;		// chain this call with last promise
    return send_promise;
  },

  /// Observe Jsonipc notifications
  receive (methodname: string, handler: Receiver | undefined): void
  {
    if (handler)
      this.receivers[methodname] = handler;
    else
      delete this.receivers[methodname];
  },

  /// Handle binary messages
  handle_binary (handler: ((data: ArrayBuffer) => void) | undefined): void
  {
    this.onbinary = handler ? handler : null;
  },

  /// Handle a Jsonipc message
  socket_message (event: MessageEvent): void
  {
    // Binary message
    if (event.data instanceof globalThis.ArrayBuffer) {
      const handler = this.onbinary;
      if (handler)
        handler (event.data);
      else
        globalThis.console.error ("Unhandled message event:", event);
      return;
    }
    // Text message
    const maybe_prototype = event.data.indexOf ('"$class":"') >= 0;
    const msg: JsonipcMessage = globalThis.JSON.parse (event.data, maybe_prototype ? Jsonipc.Jsonipc_prototype.fromJSON : null);
    if (msg.id) {
      const handler = this.idmap[msg.id];
      delete this.idmap[msg.id];
      if (handler)
        return handler (msg);
    } else if (typeof msg.method === "string" && globalThis.Array.isArray (msg.params)) { // notification
      const receiver = this.receivers[msg.method];
      if (receiver)
        receiver.apply (null, msg.params);
      return;
    }
    globalThis.console.error ("Unhandled message:", event.data);
  },

  /// Simplify initialization of globals
  setup_promise_type<T> (type: new (...args: any[]) => T, resolved?: (instance: T) => void): Promise<T> & { __resolve__: (instance: any) => void }
  {
    let resolve: (instance: T | PromiseLike<T>) => void;
    const p = new Promise<T> (r => resolve = r);
    const extended_promise = p as Promise<T> & { __resolve__: (instance: any) => void };
    extended_promise.__resolve__ = (instance: any): void => {
      if (instance instanceof type) {
        resolve (instance);
        if (resolved)
          resolved (instance);
      }
    };
    return extended_promise;
  },
};

// ----- End of jsonipc/jsonipc.ts -----
