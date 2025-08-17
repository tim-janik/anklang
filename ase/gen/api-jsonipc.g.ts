// Generated from: api.hh jsonipc.ts AnklangSynthEngine Makefile.mk
// @ts-nocheck
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
  cleanup_array_registry: new FinalizationRegistry<CleanupCallback[]> ((callback_array: CleanupCallback[]) => {
    while (callback_array.length)
      callback_array.pop().call (undefined);
    // Note, verify this is called when altering $props.$weakthis & co
  }),

  /// Install auto-fetching for prop and get its value
  get_reactive_prop<T> (this: JsonipcPrototype, prop: string, dflt: T): T
  {
    const this_props = this.$props;
    // install prop if needed
    if (!this_props[prop]) {
      // install $props system if needed
      if (!this_props.$unwatchers) {
        this_props.$weakthis = new WeakRef (this);	// helper to not keep `this` alive
        this_props.$promise = null;			// present if $promise !== undefined
        const clean_this_props = (): void => {
          //console.log ("GC: $id=" + this_props.$id, "delete $props");  // DEBUG $id GC
          for (const k of Jsonipc.okeys (this_props))
            delete this_props[k];			// allow GC for all fields
        };
        this_props.$unwatchers = [ clean_this_props ];
        Jsonipc.cleanup_array_registry.register (this, this_props.$unwatchers, this_props.$unwatchers);
        //this_props.$id = this.$id;                                     // DEBUG $id GC

        // We use this_props + $weakthis instead of `this` as object/data handle, to allow GC
        // of `this`, which in turn calls all $props.$unwatchers[].
      }
      this_props[prop] = new globalThis.Signal.State (dflt); // cached state
      // refetch, maintain $promise while waiting
      const refetch_prop = (): Promise<void> => {	// async, returns Promise, avoid keeping `this` alive
        let fetch_promise: Promise<void>;
        const last_promise = this_props.$promise;
        const async_fetch_prop = async (): Promise<void> => {
          const self = this_props.$weakthis?.deref();
          if (!self) return;
          const result_promise = Jsonipc.send ('get/' + prop, [self]); // `this`
          if (last_promise)
            await last_promise;				// sync with last, before $promise reset
          const result = await result_promise;
          if (this_props.$promise === fetch_promise) {
            this_props.$promise = null;			// reset, this call is just being resolved
          }
          const signal_state = this_props[prop] as SignalState<T>;
	  signal_state.set (result);			// assign new value after reset, otherwise callbacks might see stale promise
        };
        // start fetching and remember call promise
        this_props.$promise = fetch_promise = async_fetch_prop();
        return fetch_promise;
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

export const Error =  // Ase::Error
Jsonipc.ofreeze ({
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
});
Jsonipc.classes['Ase::Error'] = Error;

export const MusicalTuning =  // Ase::MusicalTuning
Jsonipc.ofreeze ({
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
});
Jsonipc.classes['Ase::MusicalTuning'] = MusicalTuning;

export const ResourceType =  // Ase::ResourceType
Jsonipc.ofreeze ({
  FOLDER: "Ase.ResourceType.FOLDER", // 1
  FILE: "Ase.ResourceType.FILE", // 2
});
Jsonipc.classes['Ase::ResourceType'] = ResourceType;

export const Flags =  // Ase::UserNote::Flags
Jsonipc.ofreeze ({
  APPEND: "Ase.UserNote.Flags.APPEND", // 0
  CLEAR: "Ase.UserNote.Flags.CLEAR", // 1
  TRANSIENT: "Ase.UserNote.Flags.TRANSIENT", // 2
});
Jsonipc.classes['Ase::UserNote::Flags'] = Flags;

export class Choice // Ase::Choice
{
  constructor (ident, icon, label, blurb, notice, warning) {
    this.ident = ident;
    this.icon = icon;
    this.label = label;
    this.blurb = blurb;
    this.notice = notice;
    this.warning = warning;
  }
}
Jsonipc.classes['Ase::Choice'] = Choice;

export class TelemetryField // Ase::TelemetryField
{
  constructor (name, type, offset, length) {
    this.name = name;
    this.type = type;
    this.offset = offset;
    this.length = length;
  }
}
Jsonipc.classes['Ase::TelemetryField'] = TelemetryField;

export class DeviceInfo // Ase::DeviceInfo
{
  constructor (uri, name, category, description, website_url, creator_name, creator_url) {
    this.uri = uri;
    this.name = name;
    this.category = category;
    this.description = description;
    this.website_url = website_url;
    this.creator_name = creator_name;
    this.creator_url = creator_url;
  }
}
Jsonipc.classes['Ase::DeviceInfo'] = DeviceInfo;

export class ClipNote // Ase::ClipNote
{
  constructor (id, channel, key, selected, tick, duration, velocity, fine_tune) {
    this.id = id;
    this.channel = channel;
    this.key = key;
    this.selected = selected;
    this.tick = tick;
    this.duration = duration;
    this.velocity = velocity;
    this.fine_tune = fine_tune;
  }
}
Jsonipc.classes['Ase::ClipNote'] = ClipNote;

export class ProbeFeatures // Ase::ProbeFeatures
{
  constructor (probe_range, probe_energy, probe_samples, probe_fft) {
    this.probe_range = probe_range;
    this.probe_energy = probe_energy;
    this.probe_samples = probe_samples;
    this.probe_fft = probe_fft;
  }
}
Jsonipc.classes['Ase::ProbeFeatures'] = ProbeFeatures;

export class Resource // Ase::Resource
{
  constructor (type, label, uri, size, mtime) {
    this.type = type;
    this.label = label;
    this.uri = uri;
    this.size = size;
    this.mtime = mtime;
  }
}
Jsonipc.classes['Ase::Resource'] = Resource;

export class UserNote // Ase::UserNote
{
  constructor (noteid, flags, channel, text, rest) {
    this.noteid = noteid;
    this.flags = flags;
    this.channel = channel;
    this.text = text;
    this.rest = rest;
  }
}
Jsonipc.classes['Ase::UserNote'] = UserNote;

export class TelemetrySegment // Ase::TelemetrySegment
{
  constructor (offset, length) {
    this.offset = offset;
    this.length = length;
  }
}
Jsonipc.classes['Ase::TelemetrySegment'] = TelemetrySegment;

export class SharedBase // Ase::SharedBase
  extends Jsonipc.Jsonipc_prototype
{
  constructor ($id) { super ($id); if (new.target === SharedBase) Jsonipc.ofreeze (this); }
}
Jsonipc.classes['Ase::SharedBase'] = SharedBase;

export class Emittable // Ase::Emittable
  extends Jsonipc.classes['Ase::SharedBase']
{
  constructor ($id) { super ($id); if (new.target === Emittable) Jsonipc.ofreeze (this); }
  emit_event (a1, a2, a3) { return Jsonipc.send ('emit_event', [this, a1, a2, a3]); }
  emit_notify (a1) { return Jsonipc.send ('emit_notify', [this, a1]); }
  js_trigger (a1, a2) { return Jsonipc.send ('js_trigger', [this, a1, a2]); }
}
Jsonipc.classes['Ase::Emittable'] = Emittable;

export class Property // Ase::Property
  extends Jsonipc.classes['Ase::Emittable']
{
  constructor ($id) { super ($id); if (new.target === Property) Jsonipc.ofreeze (this); }
  get name ()  { return Jsonipc.get_reactive_prop.call (this, 'name', ''); }
  set name (v) { return Jsonipc.send ('set/' + 'name', [this, v]); }
  get metadata ()  { return Jsonipc.get_reactive_prop.call (this, 'metadata', []); }
  set metadata (v) { return Jsonipc.send ('set/' + 'metadata', [this, v]); }
  get value ()  { return Jsonipc.get_reactive_prop.call (this, 'value', ''); }
  set value (v) { return Jsonipc.send ('set/' + 'value', [this, v]); }
  get normalized ()  { return Jsonipc.get_reactive_prop.call (this, 'normalized', 0.0); }
  set normalized (v) { return Jsonipc.send ('set/' + 'normalized', [this, v]); }
  get text ()  { return Jsonipc.get_reactive_prop.call (this, 'text', ''); }
  set text (v) { return Jsonipc.send ('set/' + 'text', [this, v]); }
  get_name () { return Jsonipc.send ('get_name', [this]); }
  set_name (a1) { return Jsonipc.send ('set_name', [this, a1]); }
  get_metadata () { return Jsonipc.send ('get_metadata', [this]); }
  set_metadata (a1) { return Jsonipc.send ('set_metadata', [this, a1]); }
  ident () { return Jsonipc.send ('ident', [this]); }
  label () { return Jsonipc.send ('label', [this]); }
  nick () { return Jsonipc.send ('nick', [this]); }
  unit () { return Jsonipc.send ('unit', [this]); }
  get_min () { return Jsonipc.send ('get_min', [this]); }
  get_max () { return Jsonipc.send ('get_max', [this]); }
  get_step () { return Jsonipc.send ('get_step', [this]); }
  reset () { return Jsonipc.send ('reset', [this]); }
  get_value () { return Jsonipc.send ('get_value', [this]); }
  set_value (a1) { return Jsonipc.send ('set_value', [this, a1]); }
  get_normalized () { return Jsonipc.send ('get_normalized', [this]); }
  set_normalized (a1) { return Jsonipc.send ('set_normalized', [this, a1]); }
  get_text () { return Jsonipc.send ('get_text', [this]); }
  set_text (a1) { return Jsonipc.send ('set_text', [this, a1]); }
  is_numeric () { return Jsonipc.send ('is_numeric', [this]); }
  choices () { return Jsonipc.send ('choices', [this]); }
  hints () { return Jsonipc.send ('hints', [this]); }
  blurb () { return Jsonipc.send ('blurb', [this]); }
  descr () { return Jsonipc.send ('descr', [this]); }
  group () { return Jsonipc.send ('group', [this]); }
}
Jsonipc.classes['Ase::Property'] = Property;

export class Object // Ase::Object
  extends Jsonipc.classes['Ase::Emittable']
{
  constructor ($id) { super ($id); if (new.target === Object) Jsonipc.ofreeze (this); }
}
Jsonipc.classes['Ase::Object'] = Object;

export class Gadget // Ase::Gadget
  extends Jsonipc.classes['Ase::Object']
{
  constructor ($id) { super ($id); if (new.target === Gadget) Jsonipc.ofreeze (this); }
  get name ()  { return Jsonipc.get_reactive_prop.call (this, 'name', ''); }
  set name (v) { return Jsonipc.send ('set/' + 'name', [this, v]); }
  get_name () { return Jsonipc.send ('get_name', [this]); }
  set_name (a1) { return Jsonipc.send ('set_name', [this, a1]); }
  type_nick () { return Jsonipc.send ('type_nick', [this]); }
  list_properties () { return Jsonipc.send ('list_properties', [this]); }
  access_property (a1) { return Jsonipc.send ('access_property', [this, a1]); }
  access_properties () { return Jsonipc.send ('access_properties', [this]); }
  get_value (a1) { return Jsonipc.send ('get_value', [this, a1]); }
  set_value (a1, a2) { return Jsonipc.send ('set_value', [this, a1, a2]); }
  set_data (a1, a2) { return Jsonipc.send ('set_data', [this, a1, a2]); }
  get_data (a1) { return Jsonipc.send ('get_data', [this, a1]); }
}
Jsonipc.classes['Ase::Gadget'] = Gadget;

export class ResourceCrawler // Ase::ResourceCrawler
  extends Jsonipc.classes['Ase::Object']
{
  constructor ($id) { super ($id); if (new.target === ResourceCrawler) Jsonipc.ofreeze (this); }
  get folder ()  { return Jsonipc.get_reactive_prop.call (this, 'folder', {}); }
  set folder (v) { return Jsonipc.send ('set/' + 'folder', [this, v]); }
  get entries ()  { return Jsonipc.get_reactive_prop.call (this, 'entries', []); }
  set entries (v) { return Jsonipc.send ('set/' + 'entries', [this, v]); }
  get_folder () { return Jsonipc.send ('get_folder', [this]); }
  set_folder (a1) { return Jsonipc.send ('set_folder', [this, a1]); }
  get_entries () { return Jsonipc.send ('get_entries', [this]); }
  set_entries (a1) { return Jsonipc.send ('set_entries', [this, a1]); }
  assign (a1, a2) { return Jsonipc.send ('assign', [this, a1, a2]); }
  canonify (a1, a2, a3, a4) { return Jsonipc.send ('canonify', [this, a1, a2, a3, a4]); }
}
Jsonipc.classes['Ase::ResourceCrawler'] = ResourceCrawler;

export class Device // Ase::Device
  extends Jsonipc.classes['Ase::Gadget']
{
  constructor ($id) { super ($id); if (new.target === Device) Jsonipc.ofreeze (this); }
  get devices ()  { return Jsonipc.get_reactive_prop.call (this, 'devices', []); }
  set devices (v) { return Jsonipc.send ('set/' + 'devices', [this, v]); }
  is_active () { return Jsonipc.send ('is_active', [this]); }
  device_info () { return Jsonipc.send ('device_info', [this]); }
  get_devices () { return Jsonipc.send ('get_devices', [this]); }
  set_devices (a1) { return Jsonipc.send ('set_devices', [this, a1]); }
  remove_self () { return Jsonipc.send ('remove_self', [this]); }
  gui_toggle () { return Jsonipc.send ('gui_toggle', [this]); }
  gui_supported () { return Jsonipc.send ('gui_supported', [this]); }
  gui_visible () { return Jsonipc.send ('gui_visible', [this]); }
}
Jsonipc.classes['Ase::Device'] = Device;

export class Clip // Ase::Clip
  extends Jsonipc.classes['Ase::Gadget']
{
  constructor ($id) { super ($id); if (new.target === Clip) Jsonipc.ofreeze (this); }
  get all_notes ()  { return Jsonipc.get_reactive_prop.call (this, 'all_notes', []); }
  set all_notes (v) { return Jsonipc.send ('set/' + 'all_notes', [this, v]); }
  get end_tick ()  { return Jsonipc.get_reactive_prop.call (this, 'end_tick', 0); }
  set end_tick (v) { return Jsonipc.send ('set/' + 'end_tick', [this, v]); }
  get_all_notes () { return Jsonipc.send ('get_all_notes', [this]); }
  set_all_notes (a1) { return Jsonipc.send ('set_all_notes', [this, a1]); }
  get_end_tick () { return Jsonipc.send ('get_end_tick', [this]); }
  set_end_tick (a1) { return Jsonipc.send ('set_end_tick', [this, a1]); }
  start_tick () { return Jsonipc.send ('start_tick', [this]); }
  stop_tick () { return Jsonipc.send ('stop_tick', [this]); }
  assign_range (a1, a2) { return Jsonipc.send ('assign_range', [this, a1, a2]); }
  change_batch (a1, a2) { return Jsonipc.send ('change_batch', [this, a1, a2]); }
  list_all_notes () { return Jsonipc.send ('list_all_notes', [this]); }
}
Jsonipc.classes['Ase::Clip'] = Clip;

export class Monitor // Ase::Monitor
  extends Jsonipc.classes['Ase::Gadget']
{
  constructor ($id) { super ($id); if (new.target === Monitor) Jsonipc.ofreeze (this); }
  get_output () { return Jsonipc.send ('get_output', [this]); }
  get_ochannel () { return Jsonipc.send ('get_ochannel', [this]); }
  get_mix_freq () { return Jsonipc.send ('get_mix_freq', [this]); }
  get_frame_duration () { return Jsonipc.send ('get_frame_duration', [this]); }
}
Jsonipc.classes['Ase::Monitor'] = Monitor;

export class Server // Ase::Server
  extends Jsonipc.classes['Ase::Gadget']
{
  constructor ($id) { super ($id); if (new.target === Server) Jsonipc.ofreeze (this); }
  shutdown () { return Jsonipc.send ('shutdown', [this]); }
  get_version () { return Jsonipc.send ('get_version', [this]); }
  get_build_id () { return Jsonipc.send ('get_build_id', [this]); }
  get_opus_version () { return Jsonipc.send ('get_opus_version', [this]); }
  get_flac_version () { return Jsonipc.send ('get_flac_version', [this]); }
  get_clap_version () { return Jsonipc.send ('get_clap_version', [this]); }
  error_blurb (a1) { return Jsonipc.send ('error_blurb', [this, a1]); }
  musical_tuning_label (a1) { return Jsonipc.send ('musical_tuning_label', [this, a1]); }
  musical_tuning_blurb (a1) { return Jsonipc.send ('musical_tuning_blurb', [this, a1]); }
  user_note (a1, a2, a3, a4) { return Jsonipc.send ('user_note', [this, a1, a2, a3, a4]); }
  user_reply (a1, a2) { return Jsonipc.send ('user_reply', [this, a1, a2]); }
  broadcast_telemetry (a1, a2) { return Jsonipc.send ('broadcast_telemetry', [this, a1, a2]); }
  list_preferences () { return Jsonipc.send ('list_preferences', [this]); }
  access_preference (a1) { return Jsonipc.send ('access_preference', [this, a1]); }
  engine_stats () { return Jsonipc.send ('engine_stats', [this]); }
  exit_program (a1) { return Jsonipc.send ('exit_program', [this, a1]); }
  last_project () { return Jsonipc.send ('last_project', [this]); }
  create_project (a1) { return Jsonipc.send ('create_project', [this, a1]); }
  dir_crawler (a1) { return Jsonipc.send ('dir_crawler', [this, a1]); }
  url_crawler (a1) { return Jsonipc.send ('url_crawler', [this, a1]); }
}
Jsonipc.classes['Ase::Server'] = Server;

export class NativeDevice // Ase::NativeDevice
  extends Jsonipc.classes['Ase::Device']
{
  constructor ($id) { super ($id); if (new.target === NativeDevice) Jsonipc.ofreeze (this); }
  is_combo_device () { return Jsonipc.send ('is_combo_device', [this]); }
  list_device_types () { return Jsonipc.send ('list_device_types', [this]); }
  remove_device (a1) { return Jsonipc.send ('remove_device', [this, a1]); }
  append_device (a1) { return Jsonipc.send ('append_device', [this, a1]); }
  insert_device (a1, a2) { return Jsonipc.send ('insert_device', [this, a1, a2]); }
}
Jsonipc.classes['Ase::NativeDevice'] = NativeDevice;

export class Track // Ase::Track
  extends Jsonipc.classes['Ase::Device']
{
  constructor ($id) { super ($id); if (new.target === Track) Jsonipc.ofreeze (this); }
  get midi_channel ()  { return Jsonipc.get_reactive_prop.call (this, 'midi_channel', 0); }
  set midi_channel (v) { return Jsonipc.send ('set/' + 'midi_channel', [this, v]); }
  is_master () { return Jsonipc.send ('is_master', [this]); }
  launcher_clips () { return Jsonipc.send ('launcher_clips', [this]); }
  access_device () { return Jsonipc.send ('access_device', [this]); }
  create_monitor (a1) { return Jsonipc.send ('create_monitor', [this, a1]); }
  telemetry () { return Jsonipc.send ('telemetry', [this]); }
}
Jsonipc.classes['Ase::Track'] = Track;

export class Project // Ase::Project
  extends Jsonipc.classes['Ase::Device']
{
  constructor ($id) { super ($id); if (new.target === Project) Jsonipc.ofreeze (this); }
  get bpm ()  { return Jsonipc.get_reactive_prop.call (this, 'bpm', 0.0); }
  set bpm (v) { return Jsonipc.send ('set/' + 'bpm', [this, v]); }
  get numerator ()  { return Jsonipc.get_reactive_prop.call (this, 'numerator', 0.0); }
  set numerator (v) { return Jsonipc.send ('set/' + 'numerator', [this, v]); }
  get denominator ()  { return Jsonipc.get_reactive_prop.call (this, 'denominator', 0.0); }
  set denominator (v) { return Jsonipc.send ('set/' + 'denominator', [this, v]); }
  set_bpm (a1) { return Jsonipc.send ('set_bpm', [this, a1]); }
  get_bpm () { return Jsonipc.send ('get_bpm', [this]); }
  set_numerator (a1) { return Jsonipc.send ('set_numerator', [this, a1]); }
  get_numerator () { return Jsonipc.send ('get_numerator', [this]); }
  set_denominator (a1) { return Jsonipc.send ('set_denominator', [this, a1]); }
  get_denominator () { return Jsonipc.send ('get_denominator', [this]); }
  discard () { return Jsonipc.send ('discard', [this]); }
  start_playback () { return Jsonipc.send ('start_playback', [this]); }
  stop_playback () { return Jsonipc.send ('stop_playback', [this]); }
  is_playing () { return Jsonipc.send ('is_playing', [this]); }
  create_track () { return Jsonipc.send ('create_track', [this]); }
  remove_track (a1) { return Jsonipc.send ('remove_track', [this, a1]); }
  all_tracks () { return Jsonipc.send ('all_tracks', [this]); }
  master_track () { return Jsonipc.send ('master_track', [this]); }
  save_project (a1, a2) { return Jsonipc.send ('save_project', [this, a1, a2]); }
  saved_filename () { return Jsonipc.send ('saved_filename', [this]); }
  load_project (a1) { return Jsonipc.send ('load_project', [this, a1]); }
  telemetry () { return Jsonipc.send ('telemetry', [this]); }
  group_undo (a1) { return Jsonipc.send ('group_undo', [this, a1]); }
  ungroup_undo () { return Jsonipc.send ('ungroup_undo', [this]); }
  undo () { return Jsonipc.send ('undo', [this]); }
  can_undo () { return Jsonipc.send ('can_undo', [this]); }
  redo () { return Jsonipc.send ('redo', [this]); }
  can_redo () { return Jsonipc.send ('can_redo', [this]); }
  match_serialized (a1, a2) { return Jsonipc.send ('match_serialized', [this, a1, a2]); }
}
Jsonipc.classes['Ase::Project'] = Project;

/**@type{ServerImpl}*/
export let server: Promise<Server> | Server =Jsonipc.setup_promise_type (Server, s => server = s);
