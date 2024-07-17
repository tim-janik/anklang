# Source files needed for juce and tracktion_engine
JUCE_SOURCES = $(strip \
  juce_audio_basics/juce_audio_basics.cpp \
  juce_audio_devices/juce_audio_devices.cpp \
  juce_audio_formats/juce_audio_formats.cpp \
  juce_audio_processors/juce_audio_processors.cpp \
  juce_audio_processors/juce_audio_processors_ara.cpp \
  juce_audio_processors/juce_audio_processors_lv2_libs.cpp \
  juce_audio_utils/juce_audio_utils.cpp \
  juce_core/juce_core.cpp \
  juce_data_structures/juce_data_structures.cpp \
  juce_dsp/juce_dsp.cpp \
  juce_events/juce_events.cpp \
  juce_graphics/juce_graphics.cpp \
  juce_gui_basics/juce_gui_basics.cpp \
  juce_gui_extra/juce_gui_extra.cpp \
  juce_osc/juce_osc.cpp \
)
TRACKTION_SOURCES = $(strip \
  tracktion_core/tracktion_core.cpp \
  tracktion_engine/tracktion_engine_airwindows_1.cpp \
  tracktion_engine/tracktion_engine_airwindows_2.cpp \
  tracktion_engine/tracktion_engine_airwindows_3.cpp \
  tracktion_engine/tracktion_engine_audio_files.cpp \
  tracktion_engine/tracktion_engine_model_1.cpp \
  tracktion_engine/tracktion_engine_model_2.cpp \
  tracktion_engine/tracktion_engine_playback.cpp \
  tracktion_engine/tracktion_engine_plugins.cpp \
  tracktion_engine/tracktion_engine_timestretch.cpp \
  tracktion_engine/tracktion_engine_utils.cpp \
  tracktion_graph/tracktion_graph.cpp \
)

TRACKTION_INTERNAL_INCLUDES = \
  -Ijuce_audio_processors/format_types/LV2_SDK \
  -Ijuce_audio_processors/format_types/LV2_SDK/lilv \
  -Ijuce_audio_processors/format_types/LV2_SDK/lilv/src \
  -Ijuce_audio_processors/format_types/LV2_SDK/lv2 \
  -Ijuce_audio_processors/format_types/LV2_SDK/serd \
  -Ijuce_audio_processors/format_types/LV2_SDK/sord \
  -Ijuce_audio_processors/format_types/LV2_SDK/sord/src \
  -Ijuce_audio_processors/format_types/LV2_SDK/sratom \
  -Ijuce_audio_processors/format_types/VST3_SDK

TRACKTION_EXTERNAL_INCLUDES = \
  -I/usr/include/webkitgtk-4.0 \
  -I/usr/include/glib-2.0 \
  -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
  -I/usr/include/gtk-3.0 \
  -I/usr/include/pango-1.0 \
  -I/usr/include/harfbuzz \
  -I/usr/include/freetype2 \
  -I/usr/include/libpng16 \
  -I/usr/include/libmount \
  -I/usr/include/blkid \
  -I/usr/include/fribidi \
  -I/usr/include/cairo \
  -I/usr/include/pixman-1 \
  -I/usr/include/gdk-pixbuf-2.0 \
  -I/usr/include/webp \
  -I/usr/include/gio-unix-2.0 \
  -I/usr/include/atk-1.0 \
  -I/usr/include/at-spi2-atk/2.0 \
  -I/usr/include/at-spi-2.0 \
  -I/usr/include/dbus-1.0 \
  -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
  -I/usr/include/libsoup-2.4 \
  -I/usr/include/libxml2

# Macros used by header files
TRACKTION_HEADER_DEFINES = \
  -D JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
  -D JUCE_MODAL_LOOPS_PERMITTED=1 \
  -D JUCE_MODULE_AVAILABLE_juce_data_structures=1 \
  -D JUCE_MODULE_AVAILABLE_juce_graphics=1 \
  -D JUCE_MODULE_AVAILABLE_juce_gui_extra=1 \
  -D JUCE_PLUGINHOST_AU=1 \
  -D JUCE_PLUGINHOST_LADSPA=1 \
  -D JUCE_PLUGINHOST_VST3=1 \
  -D JUCE_STRICT_REFCOUNTEDPOINTER=1 \
  -D JUCE_USE_CURL=0 \
  -D JUCE_VST3_CAN_REPLACE_VST2=0 \
  -D JUCE_WEB_BROWSER=0 \
  -D LINUX=1 \
  -D NDEBUG=1 \
  -D TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1 \
  -D _NDEBUG=1

# Macros used by C and C++ source files
TRACKTION_CCBODY_DEFINES = \
  -D JUCE_MODULE_AVAILABLE_juce_audio_devices=1 \
  -D JUCE_MODULE_AVAILABLE_juce_audio_utils=1 \
  -D JUCE_MODULE_AVAILABLE_juce_events=1 \
  -D JUCE_MODULE_AVAILABLE_juce_gui_basics=1 \
  -D JUCE_STANDALONE_APPLICATION=1 \
  -D JUCE_TARGET_HAS_BINARY_DATA=1

TRACKTION_UNUSED_DEFINES = \
  -D JUCE_MODULE_AVAILABLE_juce_audio_basics=1 \
  -D JUCE_MODULE_AVAILABLE_juce_audio_formats=1 \
  -D JUCE_MODULE_AVAILABLE_juce_audio_processors=1 \
  -D JUCE_MODULE_AVAILABLE_juce_core=1 \
  -D JUCE_MODULE_AVAILABLE_juce_dsp=1 \
  -D JUCE_MODULE_AVAILABLE_juce_osc=1 \
  -D JUCE_MODULE_AVAILABLE_tracktion_core=1 \
  -D JUCE_MODULE_AVAILABLE_tracktion_engine=1 \
  -D JUCE_MODULE_AVAILABLE_tracktion_graph=1

