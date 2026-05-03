// Generated from: api-jsonipc.json
#include <ase/jsonapi.hh>
#include <ase/api.hh>

static void
jsonipc_for_api_jsonipc_json()
{
  ::Jsonipc::Enum< ::Ase::Error > enum__Ase_Error;
  enum__Ase_Error
    .set (::Ase::Error::NONE, "NONE")
    .set (::Ase::Error::PERMS, "PERMS")
    .set (::Ase::Error::IO, "IO")
    .set (::Ase::Error::NO_MEMORY, "NO_MEMORY")
    .set (::Ase::Error::NO_SPACE, "NO_SPACE")
    .set (::Ase::Error::NO_FILES, "NO_FILES")
    .set (::Ase::Error::MANY_FILES, "MANY_FILES")
    .set (::Ase::Error::RETRY, "RETRY")
    .set (::Ase::Error::NOT_DIRECTORY, "NOT_DIRECTORY")
    .set (::Ase::Error::FILE_NOT_FOUND, "FILE_NOT_FOUND")
    .set (::Ase::Error::FILE_IS_DIR, "FILE_IS_DIR")
    .set (::Ase::Error::FILE_EXISTS, "FILE_EXISTS")
    .set (::Ase::Error::FILE_BUSY, "FILE_BUSY")
    .set (::Ase::Error::INTERNAL, "INTERNAL")
    .set (::Ase::Error::UNIMPLEMENTED, "UNIMPLEMENTED")
    .set (::Ase::Error::FILE_EOF, "FILE_EOF")
    .set (::Ase::Error::FILE_OPEN_FAILED, "FILE_OPEN_FAILED")
    .set (::Ase::Error::FILE_SEEK_FAILED, "FILE_SEEK_FAILED")
    .set (::Ase::Error::FILE_READ_FAILED, "FILE_READ_FAILED")
    .set (::Ase::Error::FILE_WRITE_FAILED, "FILE_WRITE_FAILED")
    .set (::Ase::Error::PARSE_ERROR, "PARSE_ERROR")
    .set (::Ase::Error::NO_HEADER, "NO_HEADER")
    .set (::Ase::Error::NO_SEEK_INFO, "NO_SEEK_INFO")
    .set (::Ase::Error::NO_DATA_AVAILABLE, "NO_DATA_AVAILABLE")
    .set (::Ase::Error::DATA_CORRUPT, "DATA_CORRUPT")
    .set (::Ase::Error::WRONG_N_CHANNELS, "WRONG_N_CHANNELS")
    .set (::Ase::Error::FORMAT_INVALID, "FORMAT_INVALID")
    .set (::Ase::Error::FORMAT_UNKNOWN, "FORMAT_UNKNOWN")
    .set (::Ase::Error::DATA_UNMATCHED, "DATA_UNMATCHED")
    .set (::Ase::Error::CODEC_FAILURE, "CODEC_FAILURE")
    .set (::Ase::Error::BROKEN_ARCHIVE, "BROKEN_ARCHIVE")
    .set (::Ase::Error::BAD_PROJECT, "BAD_PROJECT")
    .set (::Ase::Error::NO_PROJECT_DIR, "NO_PROJECT_DIR")
    .set (::Ase::Error::DEVICE_NOT_AVAILABLE, "DEVICE_NOT_AVAILABLE")
    .set (::Ase::Error::DEVICE_ASYNC, "DEVICE_ASYNC")
    .set (::Ase::Error::DEVICE_BUSY, "DEVICE_BUSY")
    .set (::Ase::Error::DEVICE_FORMAT, "DEVICE_FORMAT")
    .set (::Ase::Error::DEVICE_BUFFER, "DEVICE_BUFFER")
    .set (::Ase::Error::DEVICE_LATENCY, "DEVICE_LATENCY")
    .set (::Ase::Error::DEVICE_CHANNELS, "DEVICE_CHANNELS")
    .set (::Ase::Error::DEVICE_FREQUENCY, "DEVICE_FREQUENCY")
    .set (::Ase::Error::DEVICES_MISMATCH, "DEVICES_MISMATCH")
    .set (::Ase::Error::WAVE_NOT_FOUND, "WAVE_NOT_FOUND")
    .set (::Ase::Error::INVALID_PROPERTY, "INVALID_PROPERTY")
    .set (::Ase::Error::INVALID_MIDI_CONTROL, "INVALID_MIDI_CONTROL")
    .set (::Ase::Error::OPERATION_BUSY, "OPERATION_BUSY")
    ;
  ::Jsonipc::Enum< ::Ase::MusicalTuning > enum__Ase_MusicalTuning;
  enum__Ase_MusicalTuning
    .set (::Ase::MusicalTuning::OD_12_TET, "OD_12_TET")
    .set (::Ase::MusicalTuning::OD_7_TET, "OD_7_TET")
    .set (::Ase::MusicalTuning::OD_5_TET, "OD_5_TET")
    .set (::Ase::MusicalTuning::DIATONIC_SCALE, "DIATONIC_SCALE")
    .set (::Ase::MusicalTuning::INDIAN_SCALE, "INDIAN_SCALE")
    .set (::Ase::MusicalTuning::PYTHAGOREAN_TUNING, "PYTHAGOREAN_TUNING")
    .set (::Ase::MusicalTuning::PENTATONIC_5_LIMIT, "PENTATONIC_5_LIMIT")
    .set (::Ase::MusicalTuning::PENTATONIC_BLUES, "PENTATONIC_BLUES")
    .set (::Ase::MusicalTuning::PENTATONIC_GOGO, "PENTATONIC_GOGO")
    .set (::Ase::MusicalTuning::QUARTER_COMMA_MEANTONE, "QUARTER_COMMA_MEANTONE")
    .set (::Ase::MusicalTuning::SILBERMANN_SORGE, "SILBERMANN_SORGE")
    .set (::Ase::MusicalTuning::WERCKMEISTER_3, "WERCKMEISTER_3")
    .set (::Ase::MusicalTuning::WERCKMEISTER_4, "WERCKMEISTER_4")
    .set (::Ase::MusicalTuning::WERCKMEISTER_5, "WERCKMEISTER_5")
    .set (::Ase::MusicalTuning::WERCKMEISTER_6, "WERCKMEISTER_6")
    .set (::Ase::MusicalTuning::KIRNBERGER_3, "KIRNBERGER_3")
    .set (::Ase::MusicalTuning::YOUNG, "YOUNG")
    ;
  ::Jsonipc::Enum< ::Ase::ResourceType > enum__Ase_ResourceType;
  enum__Ase_ResourceType
    .set (::Ase::ResourceType::FOLDER, "FOLDER")
    .set (::Ase::ResourceType::FILE, "FILE")
    ;
  ::Jsonipc::Enum< ::Ase::UserNote::Flags > enum__Ase_UserNote_Flags;
  enum__Ase_UserNote_Flags
    .set (::Ase::UserNote::Flags::APPEND, "APPEND")
    .set (::Ase::UserNote::Flags::CLEAR, "CLEAR")
    .set (::Ase::UserNote::Flags::TRANSIENT, "TRANSIENT")
    ;

  ::Jsonipc::Serializable< ::Ase::Choice > serializable__Ase_Choice;
  serializable__Ase_Choice
    .set ("ident", &::Ase::Choice::ident)
    .set ("icon", &::Ase::Choice::icon)
    .set ("label", &::Ase::Choice::label)
    .set ("blurb", &::Ase::Choice::blurb)
    .set ("notice", &::Ase::Choice::notice)
    .set ("warning", &::Ase::Choice::warning)
    ;
  ::Jsonipc::Serializable< ::Ase::TelemetryField > serializable__Ase_TelemetryField;
  serializable__Ase_TelemetryField
    .set ("name", &::Ase::TelemetryField::name)
    .set ("type", &::Ase::TelemetryField::type)
    .set ("offset", &::Ase::TelemetryField::offset)
    .set ("length", &::Ase::TelemetryField::length)
    ;
  ::Jsonipc::Serializable< ::Ase::DeviceInfo > serializable__Ase_DeviceInfo;
  serializable__Ase_DeviceInfo
    .set ("uri", &::Ase::DeviceInfo::uri)
    .set ("name", &::Ase::DeviceInfo::name)
    .set ("category", &::Ase::DeviceInfo::category)
    .set ("description", &::Ase::DeviceInfo::description)
    .set ("website_url", &::Ase::DeviceInfo::website_url)
    .set ("creator_name", &::Ase::DeviceInfo::creator_name)
    .set ("creator_url", &::Ase::DeviceInfo::creator_url)
    ;
  ::Jsonipc::Serializable< ::Ase::ClipNote > serializable__Ase_ClipNote;
  serializable__Ase_ClipNote
    .set ("id", &::Ase::ClipNote::id)
    .set ("channel", &::Ase::ClipNote::channel)
    .set ("key", &::Ase::ClipNote::key)
    .set ("selected", &::Ase::ClipNote::selected)
    .set ("tick", &::Ase::ClipNote::tick)
    .set ("duration", &::Ase::ClipNote::duration)
    .set ("velocity", &::Ase::ClipNote::velocity)
    .set ("fine_tune", &::Ase::ClipNote::fine_tune)
    ;
  ::Jsonipc::Serializable< ::Ase::ProbeFeatures > serializable__Ase_ProbeFeatures;
  serializable__Ase_ProbeFeatures
    .set ("probe_range", &::Ase::ProbeFeatures::probe_range)
    .set ("probe_energy", &::Ase::ProbeFeatures::probe_energy)
    .set ("probe_samples", &::Ase::ProbeFeatures::probe_samples)
    .set ("probe_fft", &::Ase::ProbeFeatures::probe_fft)
    ;
  ::Jsonipc::Serializable< ::Ase::Resource > serializable__Ase_Resource;
  serializable__Ase_Resource
    .set ("type", &::Ase::Resource::type)
    .set ("label", &::Ase::Resource::label)
    .set ("uri", &::Ase::Resource::uri)
    .set ("size", &::Ase::Resource::size)
    .set ("mtime", &::Ase::Resource::mtime)
    ;
  ::Jsonipc::Serializable< ::Ase::UserNote > serializable__Ase_UserNote;
  serializable__Ase_UserNote
    .set ("noteid", &::Ase::UserNote::noteid)
    .set ("flags", &::Ase::UserNote::flags)
    .set ("channel", &::Ase::UserNote::channel)
    .set ("text", &::Ase::UserNote::text)
    .set ("rest", &::Ase::UserNote::rest)
    ;
  ::Jsonipc::Serializable< ::Ase::TelemetrySegment > serializable__Ase_TelemetrySegment;
  serializable__Ase_TelemetrySegment
    .set ("offset", &::Ase::TelemetrySegment::offset)
    .set ("length", &::Ase::TelemetrySegment::length)
    ;
  ::Jsonipc::Serializable< ::Ase::UiConfig > serializable__Ase_UiConfig;
  serializable__Ase_UiConfig
    .set ("has_ui_tests", &::Ase::UiConfig::has_ui_tests)
    .set ("auto_exit", &::Ase::UiConfig::auto_exit)
    ;

  ::Jsonipc::Class< ::Ase::Emittable > class__Ase_Emittable;
  class__Ase_Emittable
    .inherit< ::Ase::SharedBase >()
    .set ("emit_event", &::Ase::Emittable::emit_event)
    .set ("emit_notify", &::Ase::Emittable::emit_notify)
    .set ("js_trigger", &::Ase::Emittable::js_trigger)
    ;

  ::Jsonipc::Class< ::Ase::Property > class__Ase_Property;
  class__Ase_Property
    .inherit< ::Ase::Emittable >()
    .set ("normalized", &::Ase::Property::get_normalized, &::Ase::Property::set_normalized)
    .set ("text", &::Ase::Property::get_text, &::Ase::Property::set_text)
    .set ("name", &::Ase::Property::name, &::Ase::Property::name)
    .set ("metadata", &::Ase::Property::metadata, &::Ase::Property::metadata)
    .set ("ident", &::Ase::Property::ident)
    .set ("label", &::Ase::Property::label)
    .set ("nick", &::Ase::Property::nick)
    .set ("unit", &::Ase::Property::unit)
    .set ("get_min", &::Ase::Property::get_min)
    .set ("get_max", &::Ase::Property::get_max)
    .set ("get_step", &::Ase::Property::get_step)
    .set ("reset", &::Ase::Property::reset)
    .set ("value", &::Ase::Property::value, &::Ase::Property::value)
    .set ("get_normalized", &::Ase::Property::get_normalized)
    .set ("set_normalized", &::Ase::Property::set_normalized)
    .set ("get_text", &::Ase::Property::get_text)
    .set ("set_text", &::Ase::Property::set_text)
    .set ("is_numeric", &::Ase::Property::is_numeric)
    .set ("choices", &::Ase::Property::choices)
    .set ("hints", &::Ase::Property::hints)
    .set ("blurb", &::Ase::Property::blurb)
    .set ("descr", &::Ase::Property::descr)
    .set ("group", &::Ase::Property::group)
    ;

  ::Jsonipc::Class< ::Ase::Object > class__Ase_Object;
  class__Ase_Object
    .inherit< ::Ase::Emittable >()
    ;

  ::Jsonipc::Class< ::Ase::Gadget > class__Ase_Gadget;
  class__Ase_Gadget
    .inherit< ::Ase::Object >()
    .set ("name", &::Ase::Gadget::name, &::Ase::Gadget::name)
    .set ("type_nick", &::Ase::Gadget::type_nick)
    .set ("list_properties", &::Ase::Gadget::list_properties)
    .set ("access_property", &::Ase::Gadget::access_property)
    .set ("access_properties", &::Ase::Gadget::access_properties)
    .set ("get_value", &::Ase::Gadget::get_value)
    .set ("set_value", &::Ase::Gadget::set_value)
    .set ("set_data", &::Ase::Gadget::set_data)
    .set ("get_data", &::Ase::Gadget::get_data)
    .set ("remove_self", &::Ase::Gadget::remove_self)
    ;

  ::Jsonipc::Class< ::Ase::Device > class__Ase_Device;
  class__Ase_Device
    .inherit< ::Ase::Gadget >()
    .set ("device_info", &::Ase::Device::device_info)
    ;

  ::Jsonipc::Class< ::Ase::Plugin > class__Ase_Plugin;
  class__Ase_Plugin
    .inherit< ::Ase::Device >()
    .set ("plugin_type", &::Ase::Plugin::plugin_type)
    .set ("is_enabled", &::Ase::Plugin::is_enabled)
    .set ("set_enabled", &::Ase::Plugin::set_enabled)
    .set ("is_frozen", &::Ase::Plugin::is_frozen)
    .set ("set_frozen", &::Ase::Plugin::set_frozen)
    ;

  ::Jsonipc::Class< ::Ase::Clip > class__Ase_Clip;
  class__Ase_Clip
    .inherit< ::Ase::Gadget >()
    .set ("is_muted", &::Ase::Clip::is_muted)
    .set ("set_muted", &::Ase::Clip::set_muted)
    .set ("volume", &::Ase::Clip::volume, &::Ase::Clip::volume)
    .set ("pan", &::Ase::Clip::pan, &::Ase::Clip::pan)
    .set ("all_notes", &::Ase::Clip::all_notes, &::Ase::Clip::all_notes)
    .set ("end_tick", &::Ase::Clip::end_tick, &::Ase::Clip::end_tick)
    .set ("start_tick", &::Ase::Clip::start_tick)
    .set ("stop_tick", &::Ase::Clip::stop_tick)
    .set ("assign_range", &::Ase::Clip::assign_range)
    .set ("change_batch", &::Ase::Clip::change_batch)
    .set ("list_all_notes", &::Ase::Clip::list_all_notes)
    .set ("telemetry", &::Ase::Clip::telemetry)
    ;

  ::Jsonipc::Class< ::Ase::Track > class__Ase_Track;
  class__Ase_Track
    .inherit< ::Ase::Device >()
    .set ("midi_channel", &::Ase::Track::midi_channel, &::Ase::Track::midi_channel)
    .set ("is_master", &::Ase::Track::is_master)
    .set ("is_muted", &::Ase::Track::is_muted)
    .set ("set_muted", &::Ase::Track::set_muted)
    .set ("is_hidden", &::Ase::Track::is_hidden)
    .set ("set_hidden", &::Ase::Track::set_hidden)
    .set ("is_solo", &::Ase::Track::is_solo)
    .set ("set_solo", &::Ase::Track::set_solo)
    .set ("volume", &::Ase::Track::volume, &::Ase::Track::volume)
    .set ("pan", &::Ase::Track::pan, &::Ase::Track::pan)
    .set ("launcher_clips", &::Ase::Track::launcher_clips)
    .set ("create_midi_clip", &::Ase::Track::create_midi_clip)
    .set ("create_audio_clip", &::Ase::Track::create_audio_clip)
    .set ("create_plugin", &::Ase::Track::create_plugin)
    .set ("list_plugins", &::Ase::Track::list_plugins)
    .set ("access_device", &::Ase::Track::access_device)
    .set ("create_monitor", &::Ase::Track::create_monitor)
    .set ("telemetry", &::Ase::Track::telemetry)
    ;

  ::Jsonipc::Class< ::Ase::Monitor > class__Ase_Monitor;
  class__Ase_Monitor
    .inherit< ::Ase::Gadget >()
    .set ("get_output", &::Ase::Monitor::get_output)
    .set ("get_ochannel", &::Ase::Monitor::get_ochannel)
    .set ("get_mix_freq", &::Ase::Monitor::get_mix_freq)
    .set ("get_frame_duration", &::Ase::Monitor::get_frame_duration)
    ;

  ::Jsonipc::Class< ::Ase::Project > class__Ase_Project;
  class__Ase_Project
    .inherit< ::Ase::Device >()
    .set ("bpm", &::Ase::Project::bpm, &::Ase::Project::bpm)
    .set ("numerator", &::Ase::Project::numerator, &::Ase::Project::numerator)
    .set ("denominator", &::Ase::Project::denominator, &::Ase::Project::denominator)
    .set ("discard", &::Ase::Project::discard)
    .set ("start_playback", &::Ase::Project::start_playback)
    .set ("pause_playback", &::Ase::Project::pause_playback)
    .set ("stop_playback", &::Ase::Project::stop_playback)
    .set ("is_playing", &::Ase::Project::is_playing, &::Ase::Project::is_playing)
    .set ("create_track", &::Ase::Project::create_track)
    .set ("all_tracks", &::Ase::Project::all_tracks)
    .set ("master_track", &::Ase::Project::master_track)
    .set ("save_project", &::Ase::Project::save_project)
    .set ("saved_filename", &::Ase::Project::saved_filename)
    .set ("load_project", &::Ase::Project::load_project)
    .set ("telemetry", &::Ase::Project::telemetry)
    .set ("group_undo", &::Ase::Project::group_undo)
    .set ("ungroup_undo", &::Ase::Project::ungroup_undo)
    .set ("undo", &::Ase::Project::undo)
    .set ("can_undo", &::Ase::Project::can_undo)
    .set ("redo", &::Ase::Project::redo)
    .set ("can_redo", &::Ase::Project::can_redo)
    .set ("length", &::Ase::Project::length)
    .set ("master_volume", &::Ase::Project::master_volume, &::Ase::Project::master_volume)
    .set ("match_serialized", &::Ase::Project::match_serialized)
    ;

  ::Jsonipc::Class< ::Ase::ResourceCrawler > class__Ase_ResourceCrawler;
  class__Ase_ResourceCrawler
    .inherit< ::Ase::Object >()
    .set ("folder", &::Ase::ResourceCrawler::folder, &::Ase::ResourceCrawler::folder)
    .set ("entries", &::Ase::ResourceCrawler::entries, &::Ase::ResourceCrawler::entries)
    .set ("assign", &::Ase::ResourceCrawler::assign)
    .set ("canonify", &::Ase::ResourceCrawler::canonify)
    ;

  ::Jsonipc::Class< ::Ase::Server > class__Ase_Server;
  class__Ase_Server
    .inherit< ::Ase::Gadget >()
    .set ("shutdown", &::Ase::Server::shutdown)
    .set ("get_version", &::Ase::Server::get_version)
    .set ("get_build_id", &::Ase::Server::get_build_id)
    .set ("get_opus_version", &::Ase::Server::get_opus_version)
    .set ("get_flac_version", &::Ase::Server::get_flac_version)
    .set ("get_sndfile_version", &::Ase::Server::get_sndfile_version)
    .set ("error_blurb", &::Ase::Server::error_blurb)
    .set ("musical_tuning_label", &::Ase::Server::musical_tuning_label)
    .set ("musical_tuning_blurb", &::Ase::Server::musical_tuning_blurb)
    .set ("user_note", &::Ase::Server::user_note)
    .set ("user_reply", &::Ase::Server::user_reply)
    .set ("broadcast_telemetry", &::Ase::Server::broadcast_telemetry)
    .set ("list_preferences", &::Ase::Server::list_preferences)
    .set ("access_preference", &::Ase::Server::access_preference)
    .set ("ui_config", &::Ase::Server::ui_config)
    .set ("ui_test_fetch", &::Ase::Server::ui_test_fetch)
    .set ("ui_test_report", &::Ase::Server::ui_test_report)
    .set ("engine_stats", &::Ase::Server::engine_stats)
    .set ("exit_program", &::Ase::Server::exit_program)
    .set ("last_project", &::Ase::Server::last_project)
    .set ("create_project", &::Ase::Server::create_project)
    .set ("dir_crawler", &::Ase::Server::dir_crawler)
    .set ("url_crawler", &::Ase::Server::url_crawler)
    ;
}
[[maybe_unused]] static bool init_jsonipc = [] {
  if (getenv ("ASE_JSONTS"))
    Jsonipc::g_binding_printer = new Jsonipc::BindingPrinter();
  jsonipc_for_api_jsonipc_json();
  return 0;
} ();
