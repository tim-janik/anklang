// Generated file, inputs: ase/api.hh jsonipc/cxxjip.py ase/Makefile.mk
#include <ase/jsonapi.hh>
#include <ase/api.hh>
static void
jsonipc_4_api_hh()
{
  // namespace Ase
  ::Jsonipc::Enum< ::Ase::Error > enum_1001;
  enum_1001
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
  ::Jsonipc::Enum< ::Ase::MusicalTuning > enum_1002;
  enum_1002
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
  ::Jsonipc::Enum< ::Ase::ResourceType > enum_1003;
  enum_1003
    .set (::Ase::ResourceType::FOLDER, "FOLDER")
    .set (::Ase::ResourceType::FILE, "FILE")
    ;
  ::Jsonipc::Enum< ::Ase::LogFlags > enum_1004;
  enum_1004
    .set (::Ase::LogFlags::LOG_FILE, "LOG_FILE")
    .set (::Ase::LogFlags::LOG_STDERR, "LOG_STDERR")
    .set (::Ase::LogFlags::LOG_LOCATIONS, "LOG_LOCATIONS")
    ;
  ::Jsonipc::Class< ::Ase::SharedBase > class_1005;
  ::Jsonipc::Serializable< ::Ase::Choice > serializable_1006;
  serializable_1006
    .set ("ident", &::Ase::Choice::ident)
    .set ("icon", &::Ase::Choice::icon)
    .set ("label", &::Ase::Choice::label)
    .set ("blurb", &::Ase::Choice::blurb)
    .set ("notice", &::Ase::Choice::notice)
    .set ("warning", &::Ase::Choice::warning)
    ;
  ::Jsonipc::Serializable< ::Ase::TelemetryField > serializable_1007;
  serializable_1007
    .set ("name", &::Ase::TelemetryField::name)
    .set ("type", &::Ase::TelemetryField::type)
    .set ("offset", &::Ase::TelemetryField::offset)
    .set ("length", &::Ase::TelemetryField::length)
    ;
  ::Jsonipc::Class< ::Ase::Emittable > class_1008;
  class_1008
    .inherit< ::Ase::SharedBase >()
    .set ("emit_event", &::Ase::Emittable::emit_event)
    .set ("on_event", &::Ase::Emittable::on_event)
    .set ("emit_notify", &::Ase::Emittable::emit_notify)
    .set ("js_trigger", &::Ase::Emittable::js_trigger)
    ;
  ::Jsonipc::Class< ::Ase::Property > class_1009;
  class_1009
    .inherit< ::Ase::Emittable >()
    .set ("name", &::Ase::Property::name)
    .set ("value", &::Ase::Property::value)
    .set ("metadata", &::Ase::Property::metadata)
    .set ("ident", &::Ase::Property::ident)
    .set ("label", &::Ase::Property::label)
    .set ("nick", &::Ase::Property::nick)
    .set ("unit", &::Ase::Property::unit)
    .set ("get_min", &::Ase::Property::get_min)
    .set ("get_max", &::Ase::Property::get_max)
    .set ("get_step", &::Ase::Property::get_step)
    .set ("reset", &::Ase::Property::reset)
    .set ("get_value", &::Ase::Property::get_value)
    .set ("set_value", &::Ase::Property::set_value)
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
  ::Jsonipc::Class< ::Ase::Object > class_1010;
  class_1010
    .inherit< ::Ase::Emittable >()
    ;
  ::Jsonipc::Class< ::Ase::Gadget > class_1011;
  class_1011
    .inherit< ::Ase::Object >()
    .set ("name", &::Ase::Gadget::name)
    .set ("type_nick", &::Ase::Gadget::type_nick)
    .set ("list_properties", &::Ase::Gadget::list_properties)
    .set ("access_property", &::Ase::Gadget::access_property)
    .set ("access_properties", &::Ase::Gadget::access_properties)
    .set ("get_value", &::Ase::Gadget::get_value)
    .set ("set_value", &::Ase::Gadget::set_value)
    .set ("set_data", &::Ase::Gadget::set_data)
    .set ("get_data", &::Ase::Gadget::get_data)
    ;
  ::Jsonipc::Serializable< ::Ase::DeviceInfo > serializable_1012;
  serializable_1012
    .set ("uri", &::Ase::DeviceInfo::uri)
    .set ("name", &::Ase::DeviceInfo::name)
    .set ("category", &::Ase::DeviceInfo::category)
    .set ("description", &::Ase::DeviceInfo::description)
    .set ("website_url", &::Ase::DeviceInfo::website_url)
    .set ("creator_name", &::Ase::DeviceInfo::creator_name)
    .set ("creator_url", &::Ase::DeviceInfo::creator_url)
    ;
  ::Jsonipc::Class< ::Ase::Device > class_1013;
  class_1013
    .inherit< ::Ase::Gadget >()
    .set ("devs", &::Ase::Device::devs)
    .set ("is_active", &::Ase::Device::is_active)
    .set ("device_info", &::Ase::Device::device_info)
    .set ("list_devices", &::Ase::Device::list_devices)
    .set ("remove_self", &::Ase::Device::remove_self)
    .set ("gui_toggle", &::Ase::Device::gui_toggle)
    .set ("gui_supported", &::Ase::Device::gui_supported)
    .set ("gui_visible", &::Ase::Device::gui_visible)
    ;
  ::Jsonipc::Class< ::Ase::NativeDevice > class_1014;
  class_1014
    .inherit< ::Ase::Device >()
    .set ("is_combo_device", &::Ase::NativeDevice::is_combo_device)
    .set ("list_device_types", &::Ase::NativeDevice::list_device_types)
    .set ("remove_device", &::Ase::NativeDevice::remove_device)
    .set ("append_device", &::Ase::NativeDevice::append_device)
    .set ("insert_device", &::Ase::NativeDevice::insert_device)
    ;
  ::Jsonipc::Serializable< ::Ase::ClipNote > serializable_1015;
  serializable_1015
    .set ("id", &::Ase::ClipNote::id)
    .set ("channel", &::Ase::ClipNote::channel)
    .set ("key", &::Ase::ClipNote::key)
    .set ("selected", &::Ase::ClipNote::selected)
    .set ("tick", &::Ase::ClipNote::tick)
    .set ("duration", &::Ase::ClipNote::duration)
    .set ("velocity", &::Ase::ClipNote::velocity)
    .set ("fine_tune", &::Ase::ClipNote::fine_tune)
    ;
  ::Jsonipc::Class< ::Ase::Clip > class_1016;
  class_1016
    .inherit< ::Ase::Gadget >()
    .set ("all_notes", &::Ase::Clip::all_notes)
    .set ("end_tick", &::Ase::Clip::end_tick)
    .set ("get_all_notes", &::Ase::Clip::get_all_notes)
    .set ("set_all_notes", &::Ase::Clip::set_all_notes)
    .set ("get_end_tick", &::Ase::Clip::get_end_tick)
    .set ("set_end_tick", &::Ase::Clip::set_end_tick)
    .set ("start_tick", &::Ase::Clip::start_tick)
    .set ("stop_tick", &::Ase::Clip::stop_tick)
    .set ("assign_range", &::Ase::Clip::assign_range)
    .set ("change_batch", &::Ase::Clip::change_batch)
    .set ("list_all_notes", &::Ase::Clip::list_all_notes)
    ;
  ::Jsonipc::Class< ::Ase::Track > class_1017;
  class_1017
    .inherit< ::Ase::Device >()
    .set ("midi_channel", &::Ase::Track::midi_channel, &::Ase::Track::midi_channel)
    .set ("is_master", &::Ase::Track::is_master)
    .set ("launcher_clips", &::Ase::Track::launcher_clips)
    .set ("access_device", &::Ase::Track::access_device)
    .set ("create_monitor", &::Ase::Track::create_monitor)
    .set ("telemetry", &::Ase::Track::telemetry)
    ;
  ::Jsonipc::Serializable< ::Ase::ProbeFeatures > serializable_1018;
  serializable_1018
    .set ("probe_range", &::Ase::ProbeFeatures::probe_range)
    .set ("probe_energy", &::Ase::ProbeFeatures::probe_energy)
    .set ("probe_samples", &::Ase::ProbeFeatures::probe_samples)
    .set ("probe_fft", &::Ase::ProbeFeatures::probe_fft)
    ;
  ::Jsonipc::Class< ::Ase::Monitor > class_1019;
  class_1019
    .inherit< ::Ase::Gadget >()
    .set ("get_output", &::Ase::Monitor::get_output)
    .set ("get_ochannel", &::Ase::Monitor::get_ochannel)
    .set ("get_mix_freq", &::Ase::Monitor::get_mix_freq)
    .set ("get_frame_duration", &::Ase::Monitor::get_frame_duration)
    ;
  ::Jsonipc::Class< ::Ase::Project > class_1020;
  class_1020
    .inherit< ::Ase::Device >()
    .set ("bpm", &::Ase::Project::bpm)
    .set ("numerator", &::Ase::Project::numerator)
    .set ("denominator", &::Ase::Project::denominator)
    .set ("discard", &::Ase::Project::discard)
    .set ("start_playback", &::Ase::Project::start_playback)
    .set ("stop_playback", &::Ase::Project::stop_playback)
    .set ("is_playing", &::Ase::Project::is_playing)
    .set ("create_track", &::Ase::Project::create_track)
    .set ("remove_track", &::Ase::Project::remove_track)
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
    .set ("match_serialized", &::Ase::Project::match_serialized)
    ;
  ::Jsonipc::Serializable< ::Ase::Resource > serializable_1021;
  serializable_1021
    .set ("type", &::Ase::Resource::type)
    .set ("label", &::Ase::Resource::label)
    .set ("uri", &::Ase::Resource::uri)
    .set ("size", &::Ase::Resource::size)
    .set ("mtime", &::Ase::Resource::mtime)
    ;
  ::Jsonipc::Class< ::Ase::ResourceCrawler > class_1022;
  class_1022
    .inherit< ::Ase::Object >()
    .set ("folder", &::Ase::ResourceCrawler::folder)
    .set ("entries", &::Ase::ResourceCrawler::entries)
    .set ("get_folder", &::Ase::ResourceCrawler::get_folder)
    .set ("set_folder", &::Ase::ResourceCrawler::set_folder)
    .set ("get_entries", &::Ase::ResourceCrawler::get_entries)
    .set ("set_entries", &::Ase::ResourceCrawler::set_entries)
    .set ("assign", &::Ase::ResourceCrawler::assign)
    .set ("canonify", &::Ase::ResourceCrawler::canonify)
    ;
  ::Jsonipc::Serializable< ::Ase::UserNote > serializable_1023;
  serializable_1023
    .set ("noteid", &::Ase::UserNote::noteid)
    .set ("flags", &::Ase::UserNote::flags)
    .set ("channel", &::Ase::UserNote::channel)
    .set ("text", &::Ase::UserNote::text)
    .set ("rest", &::Ase::UserNote::rest)
    ;
  ::Jsonipc::Serializable< ::Ase::TelemetrySegment > serializable_1024;
  serializable_1024
    .set ("offset", &::Ase::TelemetrySegment::offset)
    .set ("length", &::Ase::TelemetrySegment::length)
    ;
  ::Jsonipc::Class< ::Ase::Server > class_1025;
  class_1025
    .inherit< ::Ase::Gadget >()
    .set ("shutdown", &::Ase::Server::shutdown)
    .set ("get_version", &::Ase::Server::get_version)
    .set ("get_build_id", &::Ase::Server::get_build_id)
    .set ("get_opus_version", &::Ase::Server::get_opus_version)
    .set ("get_flac_version", &::Ase::Server::get_flac_version)
    .set ("get_clap_version", &::Ase::Server::get_clap_version)
    .set ("error_blurb", &::Ase::Server::error_blurb)
    .set ("musical_tuning_label", &::Ase::Server::musical_tuning_label)
    .set ("musical_tuning_blurb", &::Ase::Server::musical_tuning_blurb)
    .set ("user_note", &::Ase::Server::user_note)
    .set ("user_reply", &::Ase::Server::user_reply)
    .set ("broadcast_telemetry", &::Ase::Server::broadcast_telemetry)
    .set ("list_preferences", &::Ase::Server::list_preferences)
    .set ("access_preference", &::Ase::Server::access_preference)
    .set ("engine_stats", &::Ase::Server::engine_stats)
    .set ("exit_program", &::Ase::Server::exit_program)
    .set ("last_project", &::Ase::Server::last_project)
    .set ("create_project", &::Ase::Server::create_project)
    .set ("dir_crawler", &::Ase::Server::dir_crawler)
    .set ("url_crawler", &::Ase::Server::url_crawler)
    ;
}
[[maybe_unused]] static bool init_jsonipc = (jsonipc_4_api_hh(), 0);
