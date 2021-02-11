// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "gadget.hh"
#include "jsonipc/jsonipc.hh"
#include "utils.hh"
#include "internal.hh"

namespace Ase {

// == GadgetImpl ==
JSONIPC_INHERIT (GadgetImpl, Gadget);

GadgetImpl::~GadgetImpl()
{}

std::string
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

std::string
GadgetImpl::name () const
{
  return type_nick();
}

void
GadgetImpl::name (std::string newname)
{}

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
