// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/atquit.hh>

namespace Ase {

ErrorReason     webui_start_browser (const std::string &mode, MainLoopP loop, const std::string &url, const std::function<void()> &onclose);
// check errno
String          webui_create_auth_redirect (const std::string &executable, unsigned port, const std::string &token, const std::string &snapmode = "");

} // Ase
