// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

#pragma once
// Included by Juce, avoid Ase includes

namespace juce {
namespace LinuxEventLoop {

/// Register event loop callback for fd events, uses <poll.h> constants.
void    registerFdCallback      (int fd, std::function<void(int)> func, short evmask = 1 /*POLLIN*/);
/// Unregister fd and its callback
void    unregisterFdCallback    (int fd);

} // LinuxEventLoop
} // juce
