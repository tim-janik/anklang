// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/trkn-utils.hh>
#include <ase/project.hh>

namespace Ase {

class ClipImpl : public GadgetImpl, public virtual Clip {
  friend class TrackImpl;
private:
  SelectableWeakref<tracktion::Clip> clip_;
  class ClipStateListener;
  std::unique_ptr<ClipStateListener> state_listener_;
protected:
  explicit ClipImpl         (tracktion::Clip &clip);
  virtual ~ClipImpl         ();
  ssize_t  clip_index       () const;
  ClipNoteS all_notes   () const override;
  void      all_notes   (const ClipNoteS &notes) override;
public:
  String         name           () const override;
  void           name           (const std::string &n) override;
  ClipPosition   position       () const override;
  void           position       (const ClipPosition &position) override;
  double         start_beat     () const override;
  void           start_beat     (double) override;
  double         end_beat       () const override;
  void           end_beat       (double) override;
  ClipNoteS      list_all_notes () override;
  ClipNoteS      change_batch   (const ClipNoteS &notes, const String &undogroup) override;
  bool           muted          () const override;
  void           muted          (bool muted) override;
  float          volume         () const override;
  void           volume         (float db) override;
  float          pan            () const override;
  void           pan            (float pan) override;
  void           update_telemetry ();
  void           remove_self     () override;
  TelemetryFieldS telemetry      () const override;
  static ClipImplP from_trkn    (tracktion::Clip&);
  ASE_DEFINE_MAKE_SHARED (ClipImpl);
};

} // Ase
