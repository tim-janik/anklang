// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "member.hh"
#include "api.hh"
#include "regex.hh"
#include "strings.hh"
#include "internal.hh"
#include "testing.hh"

namespace Ase::Lib {
bool
kvpairs_assign (StringS &kvs, const String &key_value_pair)
{
  const bool casesensitive = true;
  return Ase::kvpairs_assign (kvs, key_value_pair, casesensitive);
}

} // Ase::Lib

// == tests ==
namespace {
using namespace Ase;

/* Related:
 * "C++ Properties - a Library Solution", by Lois Goldthwaite
 * https://accu.org/journals/overload/13/65/goldthwaite_255/#d0e143
 * https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2004/n1615.pdf
 */

static String tlog;

static uint base_registry_count = 0;
struct BaseRegistry {
  template<class O, class M> void
  _register_parameter (O *o, M *m, const ParamExtraVals&)
  {
    // fake registry, test instead
    using value_type = decltype (m->get());
    const ptrdiff_t offset = ptrdiff_t (m) - ptrdiff_t (o);
    auto accessor = [offset] (O *o, value_type *v) {
      const ptrdiff_t maddr = ptrdiff_t (o) + offset;
      M *m = reinterpret_cast<M*> (maddr);
      if (v) m->set (*v);
      return m->get();
    };
    std::function<value_type(O*,value_type*)> accessor_func = accessor;
    tlog += string_format ("BaseRegistry=%d;\n", ++base_registry_count);
  }
};

struct Widget : BaseRegistry {
  char f_ = {};
  bool f_a (const char *n, char *v)  { if (n) f_ = *n; if (v) *v = f_; tlog += string_format ("Widget.f=%d\n", f_); if (n) f.notify(); return true; }
  char g_ = 0; void g_s (const char &n) { g_ = n; g.notify(); } char g_g () const { return g_; }
  int bit_;
  int bit_g ()                      { tlog += string_format ("realgetter=%d\n", bit_); return bit_; }
  int bit_s (const int &bit)        { tlog += string_format ("realsetter=%d\n", bit);  return bit_ = bit; }
  Ase::Member<&Widget::bit_g, &Widget::bit_s> bit [[no_unique_address]];
  Ase::Member<&Widget::f_a> f [[no_unique_address]];
  Ase::Member<&Widget::g_g, &Widget::g_s> g [[no_unique_address]];
  Widget (const std::string &foo = "") :
    bit (this),
    f (this, "f", { "blurb=_f_property", "" }),
    g (this, "g", { "blurb=_g_property", "", })
  {
    f = 'f';
    tlog += string_format ("sizeof(Widget)=%zd (%p)\n", sizeof (Widget), this);
    tlog += string_format ("sizeof(bit_)=%zd (%p)\n", sizeof (bit_), &bit_);
    tlog += string_format ("sizeof(bit)=%zd (%p) [[no_unique_address]]\n", sizeof (bit), &bit);
    tlog += string_format ("sizeof(f_)=%zd (%p)\n", sizeof (f_), &f_);
    tlog += string_format ("sizeof(f)=%zd (%p) [[no_unique_address]]\n", sizeof (f), &f);
    tlog += string_format ("sizeof(g_)=%zd (%p)\n", sizeof (g_), &g_);
    tlog += string_format ("sizeof(g)=%zd (%p) [[no_unique_address]]\n", sizeof (g), &g);
  }
  void
  emit_notify (const std::string &p)
  {
    assert_return (!p.empty());
    tlog += string_format ("NOTIFY: %s (%p)\n", p.c_str(), this);
  }
};

TEST_INTEGRITY (member_tmpl_tests);
static void
member_tmpl_tests()
{
  tlog = "";
  Widget w;
  TASSERT (sizeof (Widget) <= 2 * sizeof (int));
  TASSERT (w.f == 'f');
  w.bit = -17171;
  w.f = +71;
  w.g = -128;
  const int b = w.bit;
  tlog += string_format ("Widget.bit: %d\n", b);
  tlog += string_format ("Widget: { Widget = { .bit=%d, .f=%d, .g=%d };\n", w.bit_, w.f(), int (w.g));
  // printerr ("%s\n", tlog.c_str());
  TASSERT (w.bit.get() == -17171 && w.f == 71 && w.g == -128);
  TASSERT (w.f.info ("blurb") == "_f_property");
  TASSERT (w.g.info ("blurb") == "_g_property");
  TASSERT (Re::search ("realgetter=-17171", tlog) >= 0);
  TASSERT (Re::search ("NOTIFY: f ", tlog) >= 0);
  TASSERT (Re::search ("NOTIFY: g ", tlog) >= 0);
  TASSERT (Re::search ("BaseRegistry=3;", tlog) >= 0);
}

} // Anon
