// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "gadget.hh"
#include "jsonipc/jsonipc.hh"
#include "utils.hh"
#include "serialize.hh"
#include "internal.hh"

namespace Ase {

// == GadgetImpl ==
JSONIPC_INHERIT (GadgetImpl, Gadget);

GadgetImpl::~GadgetImpl()
{}

String
GadgetImpl::fallback_name () const
{
  return type_nick();
}

void
GadgetImpl::serialize (WritNode &xs)
{
  // name
  String current_name = name();
  if (xs.in_save() && current_name != fallback_name())
    xs["name"] & current_name;
  if (xs.in_load() && xs.has ("name"))
    {
      String new_name;
      xs["name"] & new_name;
      if (current_name != new_name)     // avoid fixating a fallback
        name (new_name);
    }
  // Serializable
  Serializable::serialize (xs);
  // properties
  for (PropertyP p : access_properties())
    {
      const String hints = p->hints();
      if (!string_option_check (hints, "S"))
        continue;
      if (xs.in_save() && string_option_check (hints, "r"))
        {
          Value v = p->get_value();
          xs[p->identifier()] & v;
        }
      if (xs.in_load() && string_option_check (hints, "w") && xs.has (p->identifier()))
        {
          Value v;
          xs[p->identifier()] & v;
          p->set_value (v);
        }
    }
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
GadgetImpl::name (String newname)
{
  newname = string_strip (newname);
  if (newname.empty())
    del_custom_data (&gadget_name_key);
  else
    set_custom_data (&gadget_name_key, newname);
  emit_event ("notify", "name");
}

StringS
GadgetImpl::list_properties ()
{
  return {}; // TODO: implement list_properties
}

PropertyP
GadgetImpl::access_property (String ident)
{
  return {}; // TODO: implement access_property
}

PropertyS
GadgetImpl::access_properties ()
{
  return {}; // TODO: implement access_properties
}

} // Ase
