#pragma once

// tracktion_decls.hh is included by tracktion_engine/tracktion_engine.h

namespace Ase {
// Ase common base type to allow casting between polymorphic classes.
struct VirtualBase; // for tracktion_engine/selection/tracktion_Selectable.h
} // Ase

// Forward declarations of tracktion namespace
namespace tracktion {

/// Declarations from this namespaces are inlined into @ref tracktion.
inline
namespace engine {

// Forward Decls
class EngineBehaviour;
class Engine;
class DeviceManager;
class GrooveTemplateManager;
class Edit;
class Track;
class Clip;
class ClipOwner;
class Plugin;
struct PluginRenderContext;
class AudioFile;
class Project;
class InputDevice;
class OutputDevice;
class WaveInputDevice;
class MidiInputDevice;
class FolderTrack;
class ClipTrack;
class AutomationTrack;
class ArrangerTrack;
class ChordTrack;
class MarkerTrack;
class MasterTrack;
class TempoTrack;
struct TrackInsertPoint;
struct TrackList;
class TrackCompManager;
class CompFactory;
class WarpTimeFactory;
class TempoSequence;
class WarpTimeManager;
class ControlSurface;
struct AudioFileInfo;
class LoopInfo;
class RenderOptions;
class AutomatableParameter;
class MacroParameterList;
class MelodyneFileReader;
struct ARADocumentHolder;
class ClipEffects;
class WaveAudioClip;
class CollectionClip;
class MidiClip;
class EditClip;
class MidiList;
class MarkerManager;
class TransportControl;
class AbletonLink;
class ParameterControlMappings;
class ParameterChangeHandler;
class AutomationRecordManager;
class RenderManager;
class EditPlaybackContext;
class EditInputDevices;
class InputDeviceInstance;
class GrooveTemplate;
class MidiOutputDevice;
class LevelMeterPlugin;
class VolumeAndPanPlugin;
class VCAPlugin;
class NovationAutomap;
class ExternalController;
class EditInsertPoint;
class AudioFileManager;
class AudioClipBase;
class AudioTrack;
class PluginList;
class RackType;
class RackInstance;
class MidiControllerParser;
class MidiInputDeviceInstanceBase;
struct RetrospectiveMidiBuffer;
class MidiLearnState;
struct EditDeleter;
struct ActiveEdits;
class AudioFileFormatManager;
class AutomatableEditItem;
class RecordingThumbnailManager;
class WaveInputRecordingThread;
class ProjectManager;
class ExternalAutomatableParameter;
class PitchShiftPlugin;
struct PluginUnloadInhibitor;
class ChordClip;
struct TimecodeSnapType;
class MidiNote;
class AutomationCurveSource;
struct Modifier;
class MidiTimecodeGenerator;
class MidiClockGenerator;
class MidiOutputDeviceInstance;
class WaveInputDeviceInstance;
class WaveOutputDeviceInstance;
struct RetrospectiveRecordBuffer;
class Clipboard;
class PropertyStorage;
class ClipSlotList;
class ClipSlot;
class LaunchHandle;
class LaunchQuantisation;
class BufferedAudioFileManager;

// Early Decls - needed for Proxy impls
class Selectable;

// trkn/tracktion_engine/selection/tracktion_Selectable.h
class SelectableListener {
public:
  virtual ~SelectableListener() {}
  virtual void selectableObjectChanged (Selectable*) = 0;
  virtual void selectableObjectAboutToBeDeleted (Selectable*) = 0;
};

} } // tracktion::engine
