// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/atquit.hh>
#include <ase/cxxaux.hh>

namespace Ase {

enum class WebuiFlags : uint32_t {
  NONE = 0,
  HEADLESS = 1 << 0,
  STDIO_REDIRECT = 1 << 1,
  CONSOLE_LOGS = 1 << 2,
};
ASE_DEFINE_FLAGS_ARITHMETIC (WebuiFlags);

ErrorReason     webui_start_browser (const std::string &mode, LoopP loop, const std::string &url, const std::function<void(int)> &onclose, WebuiFlags flags = WebuiFlags::NONE);
// check errno
String          webui_create_auth_redirect (const std::string &executable, unsigned port, const std::string &token, const std::string &snapmode = "");

} // Ase
