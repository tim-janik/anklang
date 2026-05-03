// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "gadget.hh"
#include "jsonipc/jsonipc.hh"
#include "strings.hh"
#include "utils.hh"
#include "project.hh"
#include "internal.hh"
#include "randomhash.hh"

namespace Ase {

// == Gadget ==
Gadget::Gadget()
{}

// == GadgetImpl ==
GadgetImpl::~GadgetImpl()
{}

uint64_t
GadgetImpl::gadget_flags (uint64_t setbits, uint64_t mask)
{
  gadget_flags_ &= mask;
  gadget_flags_ |= setbits;
  return gadget_flags_;
}

void
GadgetImpl::_set_parent (GadgetImpl *parent)
{
  if (parent)
    assert_return (parent_ == nullptr);
  else // !parent
    assert_return (parent_ != nullptr);
  parent_ = parent;
}

String
GadgetImpl::fallback_name () const
{
  return type_nick();
}

String
GadgetImpl::canonify_key (const String &input)
{
  String key = string_canonify (input, string_set_a2z() + string_set_A2Z() + "_0123456789.", "_");
  if (key.size() && key[0] == '.')
    key = "_" + key;
  return key;
}

Value
GadgetImpl::get_data (const String &key) const
{
  const String ckey = canonify_key (key);
  return session_data_[ckey];
}

bool
GadgetImpl::set_data (const String &key, const Value &v)
{
  const String ckey = canonify_key (key);
  return_unless (ckey.size(), false);
  session_data_[ckey] = v;
  emit_event ("data", ckey);
  return true;
}

String
GadgetImpl::type_nick () const
{
  String tname = Jsonipc::rtti_typename (*this);
  ssize_t colon = tname.rfind (':');
  if (colon != ssize_t (tname.npos))
    tname = tname.substr (colon + 1);
  if (string_endswith (tname, "Impl"))
    tname = tname.substr (0, tname.size() - 4);
  return tname;
}

static CustomDataKey<String> gadget_name_key;

String
GadgetImpl::name () const
{
  if (!has_custom_data (&gadget_name_key))
    return fallback_name();
  return get_custom_data (&gadget_name_key);
}

void
GadgetImpl::name (const std::string &n)
{
  String newname = string_strip (n);
  if (newname.empty())
    del_custom_data (&gadget_name_key);
  else
    set_custom_data (&gadget_name_key, newname);
  emit_notify ("name");
}

PropertyS
GadgetImpl::access_properties ()
{
  if (props_.empty())
    create_properties();
  return { begin (props_), end (props_) };
}

// == GadgetImpl ==
void
GadgetImpl::remove_self ()
{
  parent_ = nullptr;
  emit_event ("object", "removed");
}

// == Gadget ==
ProjectImpl*
Gadget::_project() const
{
  Gadget *last = const_cast<Gadget*> (this);
  for (Gadget *parent = last->_parent(); parent; parent = last->_parent())
    last = parent;
  return dynamic_cast<ProjectImpl*> (last);
}

StringS
Gadget::list_properties ()
{
  PropertyS props = access_properties();
  StringS names;
  names.reserve (props.size());
  for (const PropertyP &prop : props)
    names.push_back (prop->ident());
  return names;
}

PropertyP
Gadget::access_property (String ident)
{
  for (const auto &p : access_properties())
    if (p->ident() == ident)
      return p;
  return {};
}

Value
Gadget::get_value (String ident)
{
  PropertyP prop = access_property (ident);
  return prop ? prop->value() : Value {};
}

bool
Gadget::set_value (String ident, const Value &v)
{
  PropertyP prop = access_property (ident);
  return prop && prop->value (v);
}

struct MemberAccessor {
  const char      *member_typeid_name = nullptr;
  ptrdiff_t        memb_offset = -1;
  GadgetImpl::MemberAccessF func;
  GadgetImpl::MemberInfosP infosp = nullptr;
  Param::ExtraVals ev;
  uint64_t flags = 0;
};

struct GadgetClassMemberList {
  /*key*/
  const char              *class_typeid_name = nullptr;
  /*payload*/
  GadgetImpl::MemberClassT classtest = nullptr;
  std::vector<MemberAccessor*> members;
};

static auto&
cml_set()
{
  static auto gcml_hash  = [] (const GadgetClassMemberList &m) {
    return fnv1a_consthash64 (m.class_typeid_name);
  };
  static auto gcml_equal = [] (const GadgetClassMemberList &a, const GadgetClassMemberList &b) {
    return !strcmp (a.class_typeid_name, b.class_typeid_name);
  };
  using MemberAccessorSet = std::unordered_set<GadgetClassMemberList, decltype (gcml_hash), decltype (gcml_equal)>;
  static MemberAccessorSet mas (0, gcml_hash, gcml_equal);
  return mas;
}

bool
GadgetImpl::requires_accessor (const char *ot, const char *mt, ptrdiff_t offset)
{
  auto &cml = cml_set();
  const GadgetClassMemberList key { .class_typeid_name = ot };
  auto it = cml.find (key);
  if (it != cml.end())
    for (const MemberAccessor *maf : it->members)
      if (!strcmp (mt, maf->member_typeid_name)) {
        assert_return (maf->memb_offset == offset, false);
        return false;
      }
  return true;
}

void
GadgetImpl::register_accessor (const char *ot, const char *mt, ptrdiff_t offset, MemberClassT classtest,
                               const Param::ExtraVals &ev, MemberAccessF &&accessfunc,
                               MemberInfosP infosp, uint64_t flags)
{
  auto &cml = cml_set();
  auto [celement, inserted] = cml.emplace (ot, classtest);
  assert_return (celement != cml.end());
  GadgetClassMemberList *element = const_cast<GadgetClassMemberList*> (&*celement);
  element->members.push_back (new MemberAccessor {mt, offset, std::move (accessfunc), infosp, ev, flags});
  //printerr ("%s: %s+%s=%+zd\n", __func__, ot, mt, offset);
}

void
GadgetImpl::create_properties ()
{
  /* When creating the properties for an instance, walk through the known
   * GadgetClassMemberList entries, test each via dynamic_cast and thus
   * identify all class member lists that match the instance.
   */
  auto &cml = cml_set();
  for (const GadgetClassMemberList &ml : cml)
    if (ml.classtest (*this))
      for (const MemberAccessor *m : ml.members) {
        PropertyGetter getter = [this,m] (Value &value) { m->func (this, nullptr, &value); };
        PropertySetter setter = [this,m] (const Value &value) { return m->func (this, &value, nullptr); };
        PropertyLister lister = nullptr;
        StringS infos = m->infosp();
        String hints = kvpairs_fetch (infos, "hints");
        if (m->flags & MemberDetails::READABLE)
          hints += ":r";
        if (m->flags & MemberDetails::WRITABLE)
          hints += ":w";
        if (m->flags & MemberDetails::STORAGE)
          hints += ":S";
        if (m->flags & MemberDetails::GUI)
          hints += ":G";
        kvpairs_assign (infos, "hints=" + hints);
        Param param { .extras = m->ev, .metadata = infos };
        this->props_.push_back (PropertyImpl::make_shared (param, getter, setter, lister));
      }
}

} // Ase
