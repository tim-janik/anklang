// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

// This header file contains the main Juce configs
#define JUCE_APP_CONFIG_HEADER "trkn/config.hh"

/* This header file is used to configure compilation of juce and
 * tracktion internal sources, and compilation of external sources
 * that use juce or tracktion. So all juce and tracktion_engine
 * configuration macros go here.
 */

/* Keep in sync with
 * trkn/trkn.g.mk: TRACKTION_HEADER_DEFINES
 */
// #define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED		1
#define JUCE_MODAL_LOOPS_PERMITTED	        	1
#define JUCE_MODULE_AVAILABLE_juce_data_structures	1
#define JUCE_MODULE_AVAILABLE_juce_graphics		1
#define JUCE_MODULE_AVAILABLE_juce_gui_extra		1
#define JUCE_PLUGINHOST_AU	                	1
#define JUCE_PLUGINHOST_LADSPA	                	1
#define JUCE_PLUGINHOST_VST3            		1
#define JUCE_STRICT_REFCOUNTEDPOINTER	        	1
#define JUCE_USE_CURL	                        	0
#define JUCE_VST3_CAN_REPLACE_VST2	        	0
#define JUCE_WEB_BROWSER	                	0
// #define LINUX	                               	1       // use __linux__ instead
// #define NDEBUG	                               	1       // leave to build config
// #define _NDEBUG	                               	1       // leave to build config
#define TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH		1

/* Avoid any juce_* or tracktion_* header includes, these
 * are including this file and use pragma once.
 */
