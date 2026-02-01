// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/juce.hh"   // PCH include must come first

#include "utils.hh"
#include "main.hh"
#include "internal.hh"

/* Linux support for Juce. Notes on the original juce-linux setup.
 * JUCEApplicationBase::main() does:
 * - It calls JUCEApplicationBase::createInstance() to create the JUCEApplication singleton
 * - The JUCEApplicationBase ctor assigns appInstance for JUCEApplicationBase::getInstance()
 * - It calls the JUCEApplicationBase initialiseApp() and shutdownApp() methods
 * - It also runs the "message" loop via MessageManager::getInstance()->runDispatchLoop()
 * - It tries to catch and handle any exceptions bubbeling up
 * Then MessageManager::runDispatchLoop() does:
 * - It asserts to be run on isThisTheMessageThread()
 * - It ends upon quitMessageReceived
 * - It runs detail::dispatchNextMessageOnSystemQueue() and sleeps for 1ms
 * - There is also a variant runDispatchLoopUntil(), only present if JUCE_MODAL_LOOPS_PERMITTED
 * Then dispatchNextMessageOnSystemQueue() does:
 * - Call JUCEApplicationBase::quit() on SIGINT
 * - Dispatches all pending events
 * - Will poll the fds for 2000ms if no events were pending
 */

// TODO: disable        JUCE_MODAL_LOOPS_PERMITTED

namespace juce {

namespace detail {
bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages);
bool
dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages)
{ // trkn/juce_events/messages/juce_MessageManager.cpp
  Ase::warning ("%s: -ENOIMPL\n", __func__);
  return false;
}
} // detail

// == juce::MessageManager ==
static CriticalSection message_lock;
static ReferenceCountedArray<MessageManager::MessageBase> message_queue;

void
MessageManager::broadcastMessage (const String &messageText)
{
  // send a message to all other running juce applications; unimplemented in JUCE-7.0.12
  Ase::warning ("%s: -ENOIMPL\n", __func__);
}

bool
MessageManager::postMessageToSystemQueue (MessageManager::MessageBase *const message)
{
  // Must be thread safe
  ScopedLock sl (message_lock);
  message_queue.add (message);
  Ase::main_loop_wakeup();
  return true;
}

static bool
juce_loop_dispatcher (const Ase::LoopState &state)
{
  switch (state.phase)
    {
    case Ase::LoopState::PREPARE:
    case Ase::LoopState::CHECK: {
      ScopedLock sl (message_lock); // TODO: use atomic counter
      return message_queue.size() > 0;
    }
    case Ase::LoopState::DISPATCH: {
      MessageManager::MessageBase::Ptr msg;
      {
        ScopedLock sl (message_lock);
        msg = message_queue.removeAndReturn (0);
      }
      // TODO: catch exceptions
      msg->messageCallback();
      return true; // keep alive
    }
    default: ;
    }
  return false;
}

void
MessageManager::doPlatformSpecificInitialisation()
{
  Ase::main_loop->exec_dispatcher (juce_loop_dispatcher, Ase::EventLoop::PRIORITY_RTAUDIO);
}

void
MessageManager::doPlatformSpecificShutdown()
{
  // undo doPlatformSpecificInitialisation
}

namespace LinuxEventLoop {

void
registerFdCallback (int fd, std::function<void(int)> func, short evmask)
{
  // Must be thread safe
  Ase::warning ("%s: -ENOIMPL\n", __func__);
}

void
unregisterFdCallback (int fd)
{
  // Must be thread safe
  Ase::warning ("%s: -ENOIMPL\n", __func__);
}

} // LinuxEventLoop

} // juce
