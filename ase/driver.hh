// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DRIVER_HH__
#define __ASE_DRIVER_HH__

#include <ase/api.hh>
#include <ase/midievent.hh>
#include <functional>

namespace Ase {

ASE_CLASS_DECLS (Driver);
ASE_CLASS_DECLS (MidiDriver);
ASE_CLASS_DECLS (PcmDriver);

/// Driver information for PCM and MIDI handling.
struct DriverEntry {
  String devid;
  String device_name;
  String capabilities;
  String device_info;
  String notice;
  String hints;
  int32  priority = 0;
  bool   readonly = false;
  bool   writeonly = false;
};

class Driver : public std::enable_shared_from_this<Driver> {
protected:
  struct Flags { enum { OPENED = 1, READABLE = 2, WRITABLE = 4, }; };
  const String       driver_, devid_;
  size_t             flags_ = 0;
  explicit           Driver     (const String &driver, const String &devid);
  virtual           ~Driver     ();
  template<class Derived> std::shared_ptr<Derived>
  /**/               shared_from_base () { return std::static_pointer_cast<Derived> (shared_from_this()); }
public:
  enum {
    // bonus bits
    SURROUND  = 0x08 << 24,
    HEADSET   = 0x04 << 24,
    RECORDER  = 0x02 << 24,
    MIDI_THRU = 0x01 << 24,
    // penalty bits
    JACK      = 0x1f << 24,
    ALSA_USB  = 0x2f << 24,
    ALSA_KERN = 0x3f << 24,
    OSS       = 0x4f << 24,
    PULSE     = 0x5f << 24,
    ALSA_USER = 0x6f << 24,
    PSEUDO    = 0x76 << 24,
    PAUTO     = 0x79 << 24,
    PNULL     = 0x7c << 24,
    WCARD     = 0x01 << 16,
    WDEV      = 0x01 <<  8,
    WSUB      = 0x01 <<  0,
  };
  enum IODir { READONLY = 1, WRITEONLY = 2, READWRITE = 3 };
  typedef std::shared_ptr<Driver> DriverP;
  bool           opened        () const        { return flags_ & Flags::OPENED; }
  bool           readable      () const        { return flags_ & Flags::READABLE; }
  bool           writable      () const        { return flags_ & Flags::WRITABLE; }
  String         devid         () const;
  virtual void   close         () = 0;
  static String  priority_string (uint priority);
  // registry
  using Entry = DriverEntry;
  using EntryVec = DriverEntryS;
};
using DriverP = Driver::DriverP;

class MidiDriver : public Driver {
protected:
  explicit           MidiDriver      (const String &driver, const String &devid);
  virtual Ase::Error open            (IODir iodir) = 0;
public:
  typedef std::shared_ptr<MidiDriver> MidiDriverP;
  static MidiDriverP open            (const String &devid, IODir iodir, Ase::Error *ep);
  virtual bool       has_events      () = 0;
  virtual uint       fetch_events    (MidiEventOutput &estream, double samplerate) = 0;
  static EntryVec    list_drivers    ();
  static String      register_driver (const String &driverid,
                                      const std::function<MidiDriverP (const String&)> &create,
                                      const std::function<void (EntryVec&)> &list);
};
using MidiDriverP = MidiDriver::MidiDriverP;

struct PcmDriverConfig {
  uint n_channels = 0;
  uint mix_freq = 0;
  uint block_length = 0;
  uint latency_ms = 0;
};

class PcmDriver : public Driver {
protected:
  explicit           PcmDriver        (const String &driver, const String &devid);
  virtual Ase::Error open             (IODir iodir, const PcmDriverConfig &config) = 0;
public:
  typedef std::shared_ptr<PcmDriver> PcmDriverP;
  static PcmDriverP  open             (const String &devid, IODir desired, IODir required, const PcmDriverConfig &config, Ase::Error *ep);
  virtual uint       pcm_n_channels   () const = 0;
  virtual uint       pcm_mix_freq     () const = 0;
  virtual uint       pcm_block_length () const = 0;
  virtual void       pcm_latency      (uint *rlatency, uint *wlatency) const = 0;
  virtual bool       pcm_check_io     (int64 *timeoutp) = 0;
  virtual size_t     pcm_read         (size_t n, float *values) = 0;
  virtual void       pcm_write        (size_t n, const float *values) = 0;
  static EntryVec    list_drivers     ();
  static String      register_driver  (const String &driverid,
                                       const std::function<PcmDriverP (const String&)> &create,
                                       const std::function<void (EntryVec&)> &list);
};
using PcmDriverP = PcmDriver::PcmDriverP;

bool* register_driver_loader  (const char *staticwhat, Error (*loader) ());
void  load_registered_drivers ();

} // Ase

#endif  // __ASE_DRIVER_HH__
