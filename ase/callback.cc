// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "callback.hh"
#include "internal.hh"

#define CDEBUG(...)             Ase::debug ("callback", __VA_ARGS__)

namespace Ase {

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
