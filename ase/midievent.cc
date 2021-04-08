// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/midievent.hh"
#include "internal.hh"

#define EDEBUG(...)     Ase::debug ("event", __VA_ARGS__)

namespace Ase {

// == MidiEvent ==
MidiEvent::MidiEvent (const MidiEvent &other) :
  MidiEvent()
{
  *this = other;
}

MidiEvent::MidiEvent (MidiEventType etype)
{
  memset ((void*) this, 0, sizeof (*this));
  type = etype;
  // one main design consideration is minimized size
  static_assert (sizeof (MidiEvent) <= 2 * sizeof (void*));
}

MidiEvent&
MidiEvent::operator= (const MidiEvent &other)
{
  if (this != &other)
    memcpy ((void*) this, &other, sizeof (*this));
  return *this;
}

MidiEvent::~MidiEvent ()
{}

/// Determine extended message type an MidiEvent.
MidiMessage
MidiEvent::message () const
{
  if (type == MidiEvent::CONTROL_CHANGE)
    {
      if (param >= uint (MidiMessage::ALL_SOUND_OFF) &&
          param <= uint (MidiMessage::POLY_MODE_ON))
        return MidiMessage (param);
    }
  return MidiMessage (type);
}

std::string
MidiEvent::to_string () const
{
  const char *et = nullptr;
  switch (type)
    {
    case NOTE_OFF:        if (!et) et = "NOTE_OFF";
    case NOTE_ON:         if (!et) et = "NOTE_ON";
    case AFTERTOUCH:      if (!et) et = "AFTERTOUCH";
      return string_format ("%+4d ch=%-2u %-10s pitch=%d vel=%f tune=%f id=%x",
                            frame, channel, et, key, velocity, tuning, noteid);
    case CONTROL_CHANGE:        if (!et) et = "CONTROL_CHANGE";
      return string_format ("%+4d ch=%-2u %s control=%d value=%f (%02x) {%u}",
                            frame, channel, et, param, value, cval, fragment);
    case PROGRAM_CHANGE:        if (!et) et = "PROGRAM_CHANGE";
      return string_format ("%+4d ch=%-2u %s program=%d",
                            frame, channel, et, param);
    case CHANNEL_PRESSURE:      if (!et) et = "CHANNEL_PRESSURE";
    case PITCH_BEND:            if (!et) et = "PITCH_BEND";
      return string_format ("%+4d ch=%-2u %s value=%+f",
                            frame, channel, et, value);
    case SYSEX:                 if (!et) et = "SYSEX";
      return string_format ("%+4d %s (unhandled)", frame, et);
    default:
      return string_format ("%+4d MidiEvent-%u (unhandled)", frame, type);
    }
}

MidiEvent
make_note_on (uint16 chnl, uint8 mkey, float velo, float tune, uint nid)
{
  MidiEvent ev (velo > 0 ? MidiEvent::NOTE_ON : MidiEvent::NOTE_OFF);
  ev.channel = chnl;
  ev.key = mkey;
  ev.velocity = velo;
  ev.tuning = tune;
  ev.noteid = nid;
  return ev;
}

MidiEvent
make_note_off (uint16 chnl, uint8 mkey, float velo, float tune, uint nid)
{
  MidiEvent ev (MidiEvent::NOTE_OFF);
  ev.channel = chnl;
  ev.key = mkey;
  ev.velocity = velo;
  ev.tuning = tune;
  ev.noteid = nid;
  return ev;
}

MidiEvent
make_aftertouch (uint16 chnl, uint8 mkey, float velo, float tune, uint nid)
{
  MidiEvent ev (MidiEvent::AFTERTOUCH);
  ev.channel = chnl;
  ev.key = mkey;
  ev.velocity = velo;
  ev.tuning = tune;
  ev.noteid = nid;
  return ev;
}

MidiEvent
make_pressure (uint16 chnl, float velo)
{
  MidiEvent ev (MidiEvent::CHANNEL_PRESSURE);
  ev.channel = chnl;
  ev.velocity = velo;
  return ev;
}

MidiEvent
make_control (uint16 chnl, uint prm, float val)
{
  MidiEvent ev (MidiEvent::CONTROL_CHANGE);
  ev.channel = chnl;
  ev.param = prm;
  ev.value = val;
  ev.cval = ev.value * 127;
  return ev;
}

MidiEvent
make_control8 (uint16 chnl, uint prm, uint8 cval)
{
  MidiEvent ev (MidiEvent::CONTROL_CHANGE);
  ev.channel = chnl;
  ev.param = prm;
  ev.cval = cval;
  ev.value = ev.cval * (1.0 / 127.0);
  return ev;
}

MidiEvent
make_program (uint16 chnl, uint prgrm)
{
  MidiEvent ev (MidiEvent::PROGRAM_CHANGE);
  ev.channel = chnl;
  ev.param = prgrm;
  return ev;
}

MidiEvent
make_pitch_bend (uint16 chnl, float val)
{
  MidiEvent ev (MidiEvent::PITCH_BEND);
  ev.channel = chnl;
  ev.value = val;
  return ev;
}

// == MidiEventStream ==
MidiEventStream::MidiEventStream ()
{
  events_.reserve (16);
}

/// Append an MidiEvent with conscutive `frame` time stamp.
void
MidiEventStream::append (int8_t frame, const MidiEvent &event)
{
  const bool out_of_order_event = append_unsorted (frame, event);
  assert_return (!out_of_order_event);
}

/// Dangerous! Append a MidiEvent while ignoring sort order, violates constraints.
/// Returns if ensure_order() must be called due to adding an out-of-order event.
bool
MidiEventStream::append_unsorted (int8_t frame, const MidiEvent &event)
{
  const int64_t last_event_stamp = !events_.empty() ? events_.back().frame : -128;
  events_.push_back (event);
  events_.back().frame = frame;
  return frame < last_event_stamp;
}

/// Fix event order after append_unsorted() returned `true`.
void
MidiEventStream::ensure_order ()
{
  std::stable_sort (events_.begin(), events_.end(), [] (const MidiEvent &a, const MidiEvent &b) -> bool {
    return a.frame < b.frame;
  });
}

/// Fetch the latest event stamp, can be used to enforce order.
int64_t
MidiEventStream::last_frame () const
{
  return !events_.empty() ? events_.back().frame : -128;
}

// == MidiEventRange ==
MidiEventRange::MidiEventRange (const MidiEventStream &estream) :
  estream_ (estream)
{}

} // Ase
