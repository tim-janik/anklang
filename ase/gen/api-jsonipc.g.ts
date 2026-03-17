// Generated from: api.hh
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
  send (this: any, method: string, params: any[]): Promise<any>
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
export class SharedBase // Ase::SharedBase
  extends Jsonipc.Jsonipc_prototype {
  constructor (d) { super (d);
 if (new.target === SharedBase) Jsonipc.ofreeze (this); }
};
Jsonipc.classes["Ase::SharedBase"] = SharedBase;

export const Error = { // Ase::Error
  NONE: "Ase.Error.NONE", // 0
  PERMS: "Ase.Error.PERMS", // 1
  IO: "Ase.Error.IO", // 5
  NO_MEMORY: "Ase.Error.NO_MEMORY", // 12
  NO_SPACE: "Ase.Error.NO_SPACE", // 28
  NO_FILES: "Ase.Error.NO_FILES", // 23
  MANY_FILES: "Ase.Error.MANY_FILES", // 24
  RETRY: "Ase.Error.RETRY", // 4
  NOT_DIRECTORY: "Ase.Error.NOT_DIRECTORY", // 20
  FILE_NOT_FOUND: "Ase.Error.FILE_NOT_FOUND", // 2
  FILE_IS_DIR: "Ase.Error.FILE_IS_DIR", // 21
  FILE_EXISTS: "Ase.Error.FILE_EXISTS", // 17
  FILE_BUSY: "Ase.Error.FILE_BUSY", // 16
  INTERNAL: "Ase.Error.INTERNAL", // 805306368
  UNIMPLEMENTED: "Ase.Error.UNIMPLEMENTED", // 805306369
  FILE_EOF: "Ase.Error.FILE_EOF", // 805310464
  FILE_OPEN_FAILED: "Ase.Error.FILE_OPEN_FAILED", // 805310465
  FILE_SEEK_FAILED: "Ase.Error.FILE_SEEK_FAILED", // 805310466
  FILE_READ_FAILED: "Ase.Error.FILE_READ_FAILED", // 805310467
  FILE_WRITE_FAILED: "Ase.Error.FILE_WRITE_FAILED", // 805310468
  PARSE_ERROR: "Ase.Error.PARSE_ERROR", // 805314560
  NO_HEADER: "Ase.Error.NO_HEADER", // 805314561
  NO_SEEK_INFO: "Ase.Error.NO_SEEK_INFO", // 805314562
  NO_DATA_AVAILABLE: "Ase.Error.NO_DATA_AVAILABLE", // 805314563
  DATA_CORRUPT: "Ase.Error.DATA_CORRUPT", // 805314564
  WRONG_N_CHANNELS: "Ase.Error.WRONG_N_CHANNELS", // 805314565
  FORMAT_INVALID: "Ase.Error.FORMAT_INVALID", // 805314566
  FORMAT_UNKNOWN: "Ase.Error.FORMAT_UNKNOWN", // 805314567
  DATA_UNMATCHED: "Ase.Error.DATA_UNMATCHED", // 805314568
  CODEC_FAILURE: "Ase.Error.CODEC_FAILURE", // 805314569
  BROKEN_ARCHIVE: "Ase.Error.BROKEN_ARCHIVE", // 805314570
  BAD_PROJECT: "Ase.Error.BAD_PROJECT", // 805314571
  NO_PROJECT_DIR: "Ase.Error.NO_PROJECT_DIR", // 805314572
  DEVICE_NOT_AVAILABLE: "Ase.Error.DEVICE_NOT_AVAILABLE", // 805318656
  DEVICE_ASYNC: "Ase.Error.DEVICE_ASYNC", // 805318657
  DEVICE_BUSY: "Ase.Error.DEVICE_BUSY", // 805318658
  DEVICE_FORMAT: "Ase.Error.DEVICE_FORMAT", // 805318659
  DEVICE_BUFFER: "Ase.Error.DEVICE_BUFFER", // 805318660
  DEVICE_LATENCY: "Ase.Error.DEVICE_LATENCY", // 805318661
  DEVICE_CHANNELS: "Ase.Error.DEVICE_CHANNELS", // 805318662
  DEVICE_FREQUENCY: "Ase.Error.DEVICE_FREQUENCY", // 805318663
  DEVICES_MISMATCH: "Ase.Error.DEVICES_MISMATCH", // 805318664
  WAVE_NOT_FOUND: "Ase.Error.WAVE_NOT_FOUND", // 805322752
  INVALID_PROPERTY: "Ase.Error.INVALID_PROPERTY", // 805322753
  INVALID_MIDI_CONTROL: "Ase.Error.INVALID_MIDI_CONTROL", // 805322754
  OPERATION_BUSY: "Ase.Error.OPERATION_BUSY", // 805322755
} as const;
export type Error = typeof Error[keyof typeof Error];
Jsonipc.classes["Ase::Error"] = Error;

export const MusicalTuning = { // Ase::MusicalTuning
  OD_12_TET: "Ase.MusicalTuning.OD_12_TET", // 0
  OD_7_TET: "Ase.MusicalTuning.OD_7_TET", // 1
  OD_5_TET: "Ase.MusicalTuning.OD_5_TET", // 2
  DIATONIC_SCALE: "Ase.MusicalTuning.DIATONIC_SCALE", // 3
  INDIAN_SCALE: "Ase.MusicalTuning.INDIAN_SCALE", // 4
  PYTHAGOREAN_TUNING: "Ase.MusicalTuning.PYTHAGOREAN_TUNING", // 5
  PENTATONIC_5_LIMIT: "Ase.MusicalTuning.PENTATONIC_5_LIMIT", // 6
  PENTATONIC_BLUES: "Ase.MusicalTuning.PENTATONIC_BLUES", // 7
  PENTATONIC_GOGO: "Ase.MusicalTuning.PENTATONIC_GOGO", // 8
  QUARTER_COMMA_MEANTONE: "Ase.MusicalTuning.QUARTER_COMMA_MEANTONE", // 9
  SILBERMANN_SORGE: "Ase.MusicalTuning.SILBERMANN_SORGE", // 10
  WERCKMEISTER_3: "Ase.MusicalTuning.WERCKMEISTER_3", // 11
  WERCKMEISTER_4: "Ase.MusicalTuning.WERCKMEISTER_4", // 12
  WERCKMEISTER_5: "Ase.MusicalTuning.WERCKMEISTER_5", // 13
  WERCKMEISTER_6: "Ase.MusicalTuning.WERCKMEISTER_6", // 14
  KIRNBERGER_3: "Ase.MusicalTuning.KIRNBERGER_3", // 15
  YOUNG: "Ase.MusicalTuning.YOUNG", // 16
} as const;
export type MusicalTuning = typeof MusicalTuning[keyof typeof MusicalTuning];
Jsonipc.classes["Ase::MusicalTuning"] = MusicalTuning;

export const ResourceType = { // Ase::ResourceType
  FOLDER: "Ase.ResourceType.FOLDER", // 1
  FILE: "Ase.ResourceType.FILE", // 2
} as const;
export type ResourceType = typeof ResourceType[keyof typeof ResourceType];
Jsonipc.classes["Ase::ResourceType"] = ResourceType;

export const Flags = { // Ase::UserNote::Flags
  APPEND: "Ase.UserNote.Flags.APPEND", // 0
  CLEAR: "Ase.UserNote.Flags.CLEAR", // 1
  TRANSIENT: "Ase.UserNote.Flags.TRANSIENT", // 2
} as const;
export type Flags = typeof Flags[keyof typeof Flags];
Jsonipc.classes["Ase::UserNote::Flags"] = Flags;

export class Choice { // Ase::Choice
  ident: string;
  icon: string;
  label: string;
  blurb: string;
  notice: string;
  warning: string;
  constructor (ident: string = '', icon: string = '', label: string = '', blurb: string = '', notice: string = '', warning: string = '')
  {
    this.ident = ident;
    this.icon = icon;
    this.label = label;
    this.blurb = blurb;
    this.notice = notice;
    this.warning = warning;
  }
};
Jsonipc.classes["Ase::Choice"] = Choice;

export class TelemetryField { // Ase::TelemetryField
  name: string;
  type: string;
  offset: number;
  length: number;
  constructor (name: string = '', type: string = '', offset: number = 0, length: number = 0)
  {
    this.name = name;
    this.type = type;
    this.offset = offset;
    this.length = length;
  }
};
Jsonipc.classes["Ase::TelemetryField"] = TelemetryField;

export class DeviceInfo { // Ase::DeviceInfo
  uri: string;
  name: string;
  category: string;
  description: string;
  website_url: string;
  creator_name: string;
  creator_url: string;
  constructor (uri: string = '', name: string = '', category: string = '', description: string = '', website_url: string = '', creator_name: string = '', creator_url: string = '')
  {
    this.uri = uri;
    this.name = name;
    this.category = category;
    this.description = description;
    this.website_url = website_url;
    this.creator_name = creator_name;
    this.creator_url = creator_url;
  }
};
Jsonipc.classes["Ase::DeviceInfo"] = DeviceInfo;

export class ClipNote { // Ase::ClipNote
  id: number;
  channel: number;
  key: number;
  selected: boolean;
  tick: number;
  duration: number;
  velocity: number;
  fine_tune: number;
  constructor (id: number = 0, channel: number = 0, key: number = 0, selected: boolean = false, tick: number = 0, duration: number = 0, velocity: number = 0.0, fine_tune: number = 0.0)
  {
    this.id = id;
    this.channel = channel;
    this.key = key;
    this.selected = selected;
    this.tick = tick;
    this.duration = duration;
    this.velocity = velocity;
    this.fine_tune = fine_tune;
  }
};
Jsonipc.classes["Ase::ClipNote"] = ClipNote;

export class ProbeFeatures { // Ase::ProbeFeatures
  probe_range: boolean;
  probe_energy: boolean;
  probe_samples: boolean;
  probe_fft: boolean;
  constructor (probe_range: boolean = false, probe_energy: boolean = false, probe_samples: boolean = false, probe_fft: boolean = false)
  {
    this.probe_range = probe_range;
    this.probe_energy = probe_energy;
    this.probe_samples = probe_samples;
    this.probe_fft = probe_fft;
  }
};
Jsonipc.classes["Ase::ProbeFeatures"] = ProbeFeatures;

export class Resource { // Ase::Resource
  type: ResourceType;
  label: string;
  uri: string;
  size: number;
  mtime: number;
  constructor (type: ResourceType = '' as ResourceType, label: string = '', uri: string = '', size: number = 0, mtime: number = 0)
  {
    this.type = type;
    this.label = label;
    this.uri = uri;
    this.size = size;
    this.mtime = mtime;
  }
};
Jsonipc.classes["Ase::Resource"] = Resource;

export class UserNote { // Ase::UserNote
  noteid: number;
  flags: Flags;
  channel: string;
  text: string;
  rest: string;
  constructor (noteid: number = 0, flags: Flags = '' as Flags, channel: string = '', text: string = '', rest: string = '')
  {
    this.noteid = noteid;
    this.flags = flags;
    this.channel = channel;
    this.text = text;
    this.rest = rest;
  }
};
Jsonipc.classes["Ase::UserNote"] = UserNote;

export class TelemetrySegment { // Ase::TelemetrySegment
  offset: number;
  length: number;
  constructor (offset: number = 0, length: number = 0)
  {
    this.offset = offset;
    this.length = length;
  }
};
Jsonipc.classes["Ase::TelemetrySegment"] = TelemetrySegment;

export class UiConfig { // Ase::UiConfig
  has_ui_tests: boolean;
  auto_exit: boolean;
  constructor (has_ui_tests: boolean = false, auto_exit: boolean = false)
  {
    this.has_ui_tests = has_ui_tests;
    this.auto_exit = auto_exit;
  }
};
Jsonipc.classes["Ase::UiConfig"] = UiConfig;

export class Emittable // Ase::Emittable
  extends Jsonipc.classes["Ase::SharedBase"]
{
  constructor ($id)
  { super ($id); if (new.target === Emittable) Jsonipc.ofreeze (this); }
  emit_event (arg1: string, arg2: string, arg3: { [key: string]: any }): Promise<void>
  { return Jsonipc.send ("emit_event", [this, arg1, arg2, arg3]); }
  emit_notify (arg1: string): Promise<void>
  { return Jsonipc.send ("emit_notify", [this, arg1]); }
  js_trigger (arg1: string, arg2: any): Promise<void>
  { return Jsonipc.send ("js_trigger", [this, arg1, arg2]); }
};
Jsonipc.classes["Ase::Emittable"] = Emittable;

export class Property // Ase::Property
  extends Jsonipc.classes["Ase::Emittable"]
{
  constructor ($id)
  { super ($id); if (new.target === Property) Jsonipc.ofreeze (this); }
  get normalized (): number
  { return Jsonipc.get_reactive_prop.call (this, "normalized", 0.0) as number; }
  set normalized (v: number)
  { Jsonipc.set_reactive_prop.call (this, "normalized", v); }
  get text (): string
  { return Jsonipc.get_reactive_prop.call (this, "text", '') as string; }
  set text (v: string)
  { Jsonipc.set_reactive_prop.call (this, "text", v); }
  get name (): string
  { return Jsonipc.get_reactive_prop.call (this, "name", '') as string; }
  set name (v: string)
  { Jsonipc.set_reactive_prop.call (this, "name", v); }
  get metadata (): string[]
  { return Jsonipc.get_reactive_prop.call (this, "metadata", []) as string[]; }
  set metadata (v: string[])
  { Jsonipc.set_reactive_prop.call (this, "metadata", v); }
  ident (): Promise<string>
  { return Jsonipc.send ("ident", [this]); }
  label (): Promise<string>
  { return Jsonipc.send ("label", [this]); }
  nick (): Promise<string>
  { return Jsonipc.send ("nick", [this]); }
  unit (): Promise<string>
  { return Jsonipc.send ("unit", [this]); }
  get_min (): Promise<number>
  { return Jsonipc.send ("get_min", [this]); }
  get_max (): Promise<number>
  { return Jsonipc.send ("get_max", [this]); }
  get_step (): Promise<number>
  { return Jsonipc.send ("get_step", [this]); }
  reset (): Promise<void>
  { return Jsonipc.send ("reset", [this]); }
  get value (): any
  { return Jsonipc.get_reactive_prop.call (this, "value", '') as any; }
  set value (v: any)
  { Jsonipc.set_reactive_prop.call (this, "value", v); }
  get_normalized (): Promise<number>
  { return Jsonipc.send ("get_normalized", [this]); }
  set_normalized (arg1: number): Promise<boolean>
  { return Jsonipc.send ("set_normalized", [this, arg1]); }
  get_text (): Promise<string>
  { return Jsonipc.send ("get_text", [this]); }
  set_text (arg1: string): Promise<boolean>
  { return Jsonipc.send ("set_text", [this, arg1]); }
  is_numeric (): Promise<boolean>
  { return Jsonipc.send ("is_numeric", [this]); }
  choices (): Promise<Choice[]>
  { return Jsonipc.send ("choices", [this]); }
  hints (): Promise<string>
  { return Jsonipc.send ("hints", [this]); }
  blurb (): Promise<string>
  { return Jsonipc.send ("blurb", [this]); }
  descr (): Promise<string>
  { return Jsonipc.send ("descr", [this]); }
  group (): Promise<string>
  { return Jsonipc.send ("group", [this]); }
};
Jsonipc.classes["Ase::Property"] = Property;

export class Object // Ase::Object
  extends Jsonipc.classes["Ase::Emittable"]
{
  constructor ($id)
  { super ($id); if (new.target === Object) Jsonipc.ofreeze (this); }
};
Jsonipc.classes["Ase::Object"] = Object;

export class Gadget // Ase::Gadget
  extends Jsonipc.classes["Ase::Object"]
{
  constructor ($id)
  { super ($id); if (new.target === Gadget) Jsonipc.ofreeze (this); }
  get name (): string
  { return Jsonipc.get_reactive_prop.call (this, "name", '') as string; }
  set name (v: string)
  { Jsonipc.set_reactive_prop.call (this, "name", v); }
  type_nick (): Promise<string>
  { return Jsonipc.send ("type_nick", [this]); }
  list_properties (): Promise<string[]>
  { return Jsonipc.send ("list_properties", [this]); }
  access_property (arg1: string): Promise<Property>
  { return Jsonipc.send ("access_property", [this, arg1]); }
  access_properties (): Promise<Property[]>
  { return Jsonipc.send ("access_properties", [this]); }
  get_value (arg1: string): Promise<any>
  { return Jsonipc.send ("get_value", [this, arg1]); }
  set_value (arg1: string, arg2: any): Promise<boolean>
  { return Jsonipc.send ("set_value", [this, arg1, arg2]); }
  set_data (arg1: string, arg2: any): Promise<boolean>
  { return Jsonipc.send ("set_data", [this, arg1, arg2]); }
  get_data (arg1: string): Promise<any>
  { return Jsonipc.send ("get_data", [this, arg1]); }
};
Jsonipc.classes["Ase::Gadget"] = Gadget;

export class Device // Ase::Device
  extends Jsonipc.classes["Ase::Gadget"]
{
  constructor ($id)
  { super ($id); if (new.target === Device) Jsonipc.ofreeze (this); }
  get devices (): Device[]
  { return Jsonipc.get_reactive_prop.call (this, "devices", []) as Device[]; }
  set devices (v: Device[])
  { Jsonipc.set_reactive_prop.call (this, "devices", v); }
  is_active (): Promise<boolean>
  { return Jsonipc.send ("is_active", [this]); }
  device_info (): Promise<DeviceInfo>
  { return Jsonipc.send ("device_info", [this]); }
  get_devices (): Promise<Device[]>
  { return Jsonipc.send ("get_devices", [this]); }
  set_devices (arg1: Device[]): Promise<void>
  { return Jsonipc.send ("set_devices", [this, arg1]); }
  remove_self (): Promise<void>
  { return Jsonipc.send ("remove_self", [this]); }
  gui_toggle (): Promise<void>
  { return Jsonipc.send ("gui_toggle", [this]); }
  gui_supported (): Promise<boolean>
  { return Jsonipc.send ("gui_supported", [this]); }
  gui_visible (): Promise<boolean>
  { return Jsonipc.send ("gui_visible", [this]); }
};
Jsonipc.classes["Ase::Device"] = Device;

export class Clip // Ase::Clip
  extends Jsonipc.classes["Ase::Gadget"]
{
  constructor ($id)
  { super ($id); if (new.target === Clip) Jsonipc.ofreeze (this); }
  is_muted (): Promise<boolean>
  { return Jsonipc.send ("is_muted", [this]); }
  set_muted (arg1: boolean): Promise<void>
  { return Jsonipc.send ("set_muted", [this, arg1]); }
  get volume (): number
  { return Jsonipc.get_reactive_prop.call (this, "volume", 0.0) as number; }
  set volume (v: number)
  { Jsonipc.set_reactive_prop.call (this, "volume", v); }
  get pan (): number
  { return Jsonipc.get_reactive_prop.call (this, "pan", 0.0) as number; }
  set pan (v: number)
  { Jsonipc.set_reactive_prop.call (this, "pan", v); }
  get all_notes (): ClipNote[]
  { return Jsonipc.get_reactive_prop.call (this, "all_notes", []) as ClipNote[]; }
  set all_notes (v: ClipNote[])
  { Jsonipc.set_reactive_prop.call (this, "all_notes", v); }
  get end_tick (): number
  { return Jsonipc.get_reactive_prop.call (this, "end_tick", 0) as number; }
  set end_tick (v: number)
  { Jsonipc.set_reactive_prop.call (this, "end_tick", v); }
  start_tick (): Promise<number>
  { return Jsonipc.send ("start_tick", [this]); }
  stop_tick (): Promise<number>
  { return Jsonipc.send ("stop_tick", [this]); }
  assign_range (arg1: number, arg2: number): Promise<void>
  { return Jsonipc.send ("assign_range", [this, arg1, arg2]); }
  change_batch (arg1: ClipNote[], arg2: string): Promise<number>
  { return Jsonipc.send ("change_batch", [this, arg1, arg2]); }
  list_all_notes (): Promise<ClipNote[]>
  { return Jsonipc.send ("list_all_notes", [this]); }
  telemetry (): Promise<TelemetryField[]>
  { return Jsonipc.send ("telemetry", [this]); }
};
Jsonipc.classes["Ase::Clip"] = Clip;

export class Track // Ase::Track
  extends Jsonipc.classes["Ase::Device"]
{
  constructor ($id)
  { super ($id); if (new.target === Track) Jsonipc.ofreeze (this); }
  get midi_channel (): number
  { return Jsonipc.get_reactive_prop.call (this, "midi_channel", 0) as number; }
  set midi_channel (v: number)
  { Jsonipc.set_reactive_prop.call (this, "midi_channel", v); }
  is_master (): Promise<boolean>
  { return Jsonipc.send ("is_master", [this]); }
  is_muted (): Promise<boolean>
  { return Jsonipc.send ("is_muted", [this]); }
  set_muted (arg1: boolean): Promise<void>
  { return Jsonipc.send ("set_muted", [this, arg1]); }
  is_solo (): Promise<boolean>
  { return Jsonipc.send ("is_solo", [this]); }
  set_solo (arg1: boolean): Promise<void>
  { return Jsonipc.send ("set_solo", [this, arg1]); }
  get volume (): number
  { return Jsonipc.get_reactive_prop.call (this, "volume", 0.0) as number; }
  set volume (v: number)
  { Jsonipc.set_reactive_prop.call (this, "volume", v); }
  get pan (): number
  { return Jsonipc.get_reactive_prop.call (this, "pan", 0.0) as number; }
  set pan (v: number)
  { Jsonipc.set_reactive_prop.call (this, "pan", v); }
  launcher_clips (): Promise<Clip[]>
  { return Jsonipc.send ("launcher_clips", [this]); }
  access_device (): Promise<Device>
  { return Jsonipc.send ("access_device", [this]); }
  create_monitor (arg1: number): Promise<Monitor>
  { return Jsonipc.send ("create_monitor", [this, arg1]); }
  telemetry (): Promise<TelemetryField[]>
  { return Jsonipc.send ("telemetry", [this]); }
};
Jsonipc.classes["Ase::Track"] = Track;

export class Monitor // Ase::Monitor
  extends Jsonipc.classes["Ase::Gadget"]
{
  constructor ($id)
  { super ($id); if (new.target === Monitor) Jsonipc.ofreeze (this); }
  get_output (): Promise<Device>
  { return Jsonipc.send ("get_output", [this]); }
  get_ochannel (): Promise<number>
  { return Jsonipc.send ("get_ochannel", [this]); }
  get_mix_freq (): Promise<number>
  { return Jsonipc.send ("get_mix_freq", [this]); }
  get_frame_duration (): Promise<number>
  { return Jsonipc.send ("get_frame_duration", [this]); }
};
Jsonipc.classes["Ase::Monitor"] = Monitor;

export class Project // Ase::Project
  extends Jsonipc.classes["Ase::Device"]
{
  constructor ($id)
  { super ($id); if (new.target === Project) Jsonipc.ofreeze (this); }
  get bpm (): number
  { return Jsonipc.get_reactive_prop.call (this, "bpm", 0.0) as number; }
  set bpm (v: number)
  { Jsonipc.set_reactive_prop.call (this, "bpm", v); }
  get numerator (): number
  { return Jsonipc.get_reactive_prop.call (this, "numerator", 0.0) as number; }
  set numerator (v: number)
  { Jsonipc.set_reactive_prop.call (this, "numerator", v); }
  get denominator (): number
  { return Jsonipc.get_reactive_prop.call (this, "denominator", 0.0) as number; }
  set denominator (v: number)
  { Jsonipc.set_reactive_prop.call (this, "denominator", v); }
  discard (): Promise<void>
  { return Jsonipc.send ("discard", [this]); }
  start_playback (): Promise<void>
  { return Jsonipc.send ("start_playback", [this]); }
  pause_playback (): Promise<void>
  { return Jsonipc.send ("pause_playback", [this]); }
  stop_playback (): Promise<void>
  { return Jsonipc.send ("stop_playback", [this]); }
  get is_playing (): boolean
  { return Jsonipc.get_reactive_prop.call (this, "is_playing", false) as boolean; }
  set is_playing (v: boolean)
  { Jsonipc.set_reactive_prop.call (this, "is_playing", v); }
  create_track (): Promise<Track>
  { return Jsonipc.send ("create_track", [this]); }
  remove_track (arg1: Track): Promise<boolean>
  { return Jsonipc.send ("remove_track", [this, arg1]); }
  all_tracks (): Promise<Track[]>
  { return Jsonipc.send ("all_tracks", [this]); }
  master_track (): Promise<Track>
  { return Jsonipc.send ("master_track", [this]); }
  save_project (arg1: string, arg2: boolean): Promise<Error>
  { return Jsonipc.send ("save_project", [this, arg1, arg2]); }
  saved_filename (): Promise<string>
  { return Jsonipc.send ("saved_filename", [this]); }
  load_project (arg1: string): Promise<Error>
  { return Jsonipc.send ("load_project", [this, arg1]); }
  telemetry (): Promise<TelemetryField[]>
  { return Jsonipc.send ("telemetry", [this]); }
  group_undo (arg1: string): Promise<void>
  { return Jsonipc.send ("group_undo", [this, arg1]); }
  ungroup_undo (): Promise<void>
  { return Jsonipc.send ("ungroup_undo", [this]); }
  undo (): Promise<void>
  { return Jsonipc.send ("undo", [this]); }
  can_undo (): Promise<boolean>
  { return Jsonipc.send ("can_undo", [this]); }
  redo (): Promise<void>
  { return Jsonipc.send ("redo", [this]); }
  can_redo (): Promise<boolean>
  { return Jsonipc.send ("can_redo", [this]); }
  length (): Promise<number>
  { return Jsonipc.send ("length", [this]); }
  get master_volume (): number
  { return Jsonipc.get_reactive_prop.call (this, "master_volume", 0.0) as number; }
  set master_volume (v: number)
  { Jsonipc.set_reactive_prop.call (this, "master_volume", v); }
  match_serialized (arg1: string, arg2: number): Promise<string>
  { return Jsonipc.send ("match_serialized", [this, arg1, arg2]); }
};
Jsonipc.classes["Ase::Project"] = Project;

export class ResourceCrawler // Ase::ResourceCrawler
  extends Jsonipc.classes["Ase::Object"]
{
  constructor ($id)
  { super ($id); if (new.target === ResourceCrawler) Jsonipc.ofreeze (this); }
  get folder (): Resource
  { return Jsonipc.get_reactive_prop.call (this, "folder", {}) as Resource; }
  set folder (v: Resource)
  { Jsonipc.set_reactive_prop.call (this, "folder", v); }
  get entries (): Resource[]
  { return Jsonipc.get_reactive_prop.call (this, "entries", []) as Resource[]; }
  set entries (v: Resource[])
  { Jsonipc.set_reactive_prop.call (this, "entries", v); }
  assign (arg1: string, arg2: boolean): Promise<[string, string]>
  { return Jsonipc.send ("assign", [this, arg1, arg2]); }
  canonify (arg1: string, arg2: string, arg3: boolean, arg4: boolean): Promise<Resource>
  { return Jsonipc.send ("canonify", [this, arg1, arg2, arg3, arg4]); }
};
Jsonipc.classes["Ase::ResourceCrawler"] = ResourceCrawler;

export class Server // Ase::Server
  extends Jsonipc.classes["Ase::Gadget"]
{
  constructor ($id)
  { super ($id); if (new.target === Server) Jsonipc.ofreeze (this); }
  shutdown (): Promise<void>
  { return Jsonipc.send ("shutdown", [this]); }
  get_version (): Promise<string>
  { return Jsonipc.send ("get_version", [this]); }
  get_build_id (): Promise<string>
  { return Jsonipc.send ("get_build_id", [this]); }
  get_opus_version (): Promise<string>
  { return Jsonipc.send ("get_opus_version", [this]); }
  get_flac_version (): Promise<string>
  { return Jsonipc.send ("get_flac_version", [this]); }
  get_sndfile_version (): Promise<string>
  { return Jsonipc.send ("get_sndfile_version", [this]); }
  error_blurb (arg1: Error): Promise<string>
  { return Jsonipc.send ("error_blurb", [this, arg1]); }
  musical_tuning_label (arg1: MusicalTuning): Promise<string>
  { return Jsonipc.send ("musical_tuning_label", [this, arg1]); }
  musical_tuning_blurb (arg1: MusicalTuning): Promise<string>
  { return Jsonipc.send ("musical_tuning_blurb", [this, arg1]); }
  user_note (arg1: string, arg2: string, arg3: Flags, arg4: string): Promise<number>
  { return Jsonipc.send ("user_note", [this, arg1, arg2, arg3, arg4]); }
  user_reply (arg1: number, arg2: number): Promise<boolean>
  { return Jsonipc.send ("user_reply", [this, arg1, arg2]); }
  broadcast_telemetry (arg1: TelemetrySegment[], arg2: number): Promise<boolean>
  { return Jsonipc.send ("broadcast_telemetry", [this, arg1, arg2]); }
  list_preferences (): Promise<string[]>
  { return Jsonipc.send ("list_preferences", [this]); }
  access_preference (arg1: string): Promise<Property>
  { return Jsonipc.send ("access_preference", [this, arg1]); }
  ui_config (): Promise<UiConfig>
  { return Jsonipc.send ("ui_config", [this]); }
  ui_test_fetch (): Promise<string>
  { return Jsonipc.send ("ui_test_fetch", [this]); }
  ui_test_report (arg1: string, arg2: boolean): Promise<void>
  { return Jsonipc.send ("ui_test_report", [this, arg1, arg2]); }
  engine_stats (): Promise<string>
  { return Jsonipc.send ("engine_stats", [this]); }
  exit_program (arg1: number): Promise<void>
  { return Jsonipc.send ("exit_program", [this, arg1]); }
  last_project (): Promise<Project>
  { return Jsonipc.send ("last_project", [this]); }
  create_project (arg1: string): Promise<Project>
  { return Jsonipc.send ("create_project", [this, arg1]); }
  dir_crawler (arg1: string): Promise<ResourceCrawler>
  { return Jsonipc.send ("dir_crawler", [this, arg1]); }
  url_crawler (arg1: string): Promise<ResourceCrawler>
  { return Jsonipc.send ("url_crawler", [this, arg1]); }
};
Jsonipc.classes["Ase::Server"] = Server;


/**@type{ServerImpl}*/
export let server: Server =Jsonipc.setup_promise_type (Server, s => server = s) as unknown as Server;
