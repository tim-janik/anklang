// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "callback.hh"
#include "internal.hh"
#include <string.h>

#define CDEBUG(...)             Ase::debug ("callback", __VA_ARGS__)

namespace Ase {

// == RtCall::Callable ==
RtCall::~RtCall ()
{
  callable_ = 0;
  memset (mem_, 0, sizeof (mem_));
}

RtCall::RtCall (const RtCall &other)
{
  if (other.callable_) {
    assert_return (&other.mem_[0] == (void*) other.callable_);
    memcpy (mem_, other.mem_, sizeof (other.mem_));
    callable_ = reinterpret_cast<Callable*> (unalias_ptr (&mem_[0]));
  }
}

RtCall::RtCall (void (*f) ())
{
  struct Wrapper : Callable {
    Wrapper (void (*f) ()) : f_ (f) {}
    void call() override { return f_ (); }
    void (*f_) ();
  };
  static_assert (sizeof (mem_) >= sizeof (Wrapper));
  callable_ = new (mem_) Wrapper { f };
}

void
RtCall::invoke ()
{
  if (callable_)
    callable_->call();
}

void RtCall::Callable::call () {}

// == JobQueue ==
JobQueue::JobQueue (const Caller &caller) :
  caller_ (caller)
{}

} // Ase

#include <assert.h>

namespace {
using namespace Ase;

TEST_INTEGRITY (callback_list_test);
static void
callback_list_test()
{
  std::shared_ptr<CallbackList<const char*>> cbl = CallbackList<const char*>::make_shared();
  String r;
  const size_t aid = cbl->add ([&r] (const char *delim) { r += delim; r += "a"; });
  const size_t bid = cbl->add ([&r,&cbl,aid] (const char *delim) { r += delim; r += "b"; cbl->del (aid); });
  const size_t cid = cbl->add ([&r] (const char *delim) { r += delim; r += "c"; });
  (*cbl) ("+");
  assert (r == "+a+b+c");
  (*cbl) ("|");
  assert (r == "+a+b+c|b|c");
  cbl->del (bid);
  (*cbl) ("*");
  assert (r == "+a+b+c|b|c*c");
  cbl->del (cid);
  (*cbl) ("-");
  assert (r == "+a+b+c|b|c*c");
}

} // Anon
