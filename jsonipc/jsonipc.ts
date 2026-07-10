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
  $asyncs(): Promise<void>;	         // Wait for all pending async operations to complete
  $refetch<T> (cb: () => T): Promise<T>; // Run cb, await asyncs, maybe re-run cb for fresh cached value
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

  /// Chain a new promise to the object's async operation queue
  /// The returned promise resolves to the result of `nextpromise`, so
  /// `await add_prop_promise (nextpromise)` yields the same value as `await nextpromise`.
  add_prop_promise<T> (this: any, nextpromise: Promise<any>)
  {
    if (!this.$props.$promise) {
      this.$props.$promise = nextpromise;
      return this.$props.$promise;	// resolves to value of nextpromise
    }
    // Create a WeakRef to the object so it can be GC'd even if promises hold references
    const weakthis = Jsonipc.ensure_props.call (this).$weakthis;
    // Chain the existing promise with the new one
    const prevpromise = this.$props.$promise;
    const wrapperpromise = new Promise (async resolve =>
      {
	await prevpromise;
	const nextresult = await nextpromise;
        // Dereference the object - may be null if GC'd
        const obj = weakthis.deref();
        if (obj?.$props?.$promise === wrapperpromise)
	  obj.$props.$promise = null;	// breaks references for GC
        resolve (nextresult);
      }
    );
    wrapperpromise.catch (exc => {
      const obj = weakthis.deref();
      if (obj?.$props?.$promise === wrapperpromise)
        obj.$props.$promise = null;
      throw exc;
    });
    // assign synchronization point
    this.$props.$promise = wrapperpromise;
    return this.$props.$promise;	// resolves to value of nextpromise
  },

  /// Install auto-fetching for prop and get its value
  ensure_props (this: any): any
  {
    const this_props = this.$props;
    if (!this_props.$weakthis) {
      // install $props system
      this_props.$weakthis = new WeakRef (this);	// helper to not keep `this` alive
      if (!this_props.$promise)
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
        const promise_send_and_update = async (): Promise<T> =>
	{
          const self = this_props.$weakthis?.deref();
          if (!self) return;
          const result = await Jsonipc.send ('get/' + prop, [self]);
          const signal_state = this_props[prop] as SignalState<T>;
	  signal_state.set (result);			// assign new value
	  return result;
        };
        return Jsonipc.add_prop_promise.call (this, promise_send_and_update());
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
    this.web_socket.onmessage = this.socket_onmessage.bind (this);
    const promise = new globalThis.Promise<boolean> ((resolve, reject) => {
      this.web_socket.onopen = (): void => {
        const promise_handshake = this.send ('Jsonipc/handshake', []);
        promise_handshake.then ((result: any) => {
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
      // Freeze `this`, because otherwise frameworks like Vue recursively invade all
      // objects and ultimately lead to Signal.get choking when `this` is a Proxy.
    }
    // JSON.stringify replacer
    toJSON(): { $id: number }
    {
      return { $id: this.$id };
    }
    // Send a Jsonipc remote call, await result, await cascading notifications
    async $rpc (method: string, params: any[]): Promise<any>
    {
      const promise = Jsonipc.send (method, params);
      // remote should first send change notification, then RPC result
      const result = Jsonipc.add_prop_promise.call (this, promise);
      // here, change notification async callbacks have started, not yet finished
      if (this.$props.$promise) // pending async callbacks
	await this.$asyncs();   // await callbacks / property refetch updates
      // here, RPC is done and properties were refetched
      return result;
    }
    /// Get a property value via createSignal(), returns dflt on first access and queues refetch
    $get<T> (prop: string, dflt: T): T
    {
      return Jsonipc.get_reactive_prop.call (this, prop, dflt) as T;
    }
    /// Set a reactive property remotely and await completion
    async $set<T> (prop: string, val: T): Promise<any>
    {
      const promise = Jsonipc.send ('set/' + prop, [this, val]);
      return await Jsonipc.add_prop_promise.call (this, promise);
    }
    /// Wait for all pending async operations to complete
    async $asyncs()
    {
      const last_promise = this.$props.$promise;
      // NOP if no async operation is pending
      if (!last_promise)
	return;
      // await completion of any running async modifications
      await last_promise;
      // a new promise likely indicates the refetch() of a past modification
      if (this.$props.$promise && last_promise !== this.$props.$promise)
	await this.$props.$promise;
    }
    /// Run cb, await $asyncs(), maybe re-run cb for fresh cached value
    async $refetch<T> (cb: () => T): Promise<T>
    {
      let result = cb();
      if (this.$props.$promise) {
	await this.$asyncs();
	result = cb();
      }
      return result;
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

  /// Send a Jsonipc request, await result
  async send (method: string, params: any[])
  {
    if (!this.web_socket)
      throw globalThis.Error ("Jsonipc: connection closed");
    const id = ++this.counter;
    let send_promise: Promise<any>;			// promise to sync with this call
    this.web_socket.send (globalThis.JSON.stringify ({ id, method, params }));
    const promise_reply = new globalThis.Promise<JsonipcMessage> (handle_reply => this.idmap[id] = handle_reply);
    const reply = await promise_reply; // resolved in socket_onmessage()
    if (reply.error)
      throw globalThis.Error (
        `${reply.error.code}: ${reply.error.message}\n` +
        `Request: {"id":${id},"method":"${method}",…}\n` +
        "Reply: " + globalThis.JSON.stringify (reply)
      );
    return reply.result;
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
  socket_onmessage (event: MessageEvent): void
  {
    // Binary message
    if (event.data instanceof globalThis.ArrayBuffer) {
      const binary_handler = this.onbinary;
      if (binary_handler)
        binary_handler (event.data);
      else
        globalThis.console.error ("Unhandled message event:", event);
      return;
    }
    // Text message
    const maybe_prototype = event.data.indexOf ('"$class":"') >= 0;
    const msg: JsonipcMessage = globalThis.JSON.parse (event.data, maybe_prototype ? Jsonipc.Jsonipc_prototype.fromJSON : null);
    if (msg.id) {
      const handle_reply = this.idmap[msg.id];
      delete this.idmap[msg.id];
      if (handle_reply)
        return handle_reply (msg);
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
