// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "serialize.hh"
#include "jsonapi.hh"
#include "utils.hh"
#include "driver.hh"
#include "internal.hh"
#include <rapidjson/prettywriter.h>

namespace Ase {

// == Writ::InstanceMap ==
Jsonipc::JsonValue
Writ::InstanceMap::wrapper_to_json (Wrapper *wrapper, const size_t thisid, const std::string &wraptype, Jsonipc::JsonAllocator &allocator)
{
  warning ("Ase::Writ: object pointer is not persistent: (%s*) {\"$id\":%d}", wraptype, thisid);
  //return Jsonipc::JsonValue(); // null
  return this->Jsonipc::InstanceMap::wrapper_to_json (wrapper, thisid, wraptype, allocator);
}

Jsonipc::InstanceMap::Wrapper*
Writ::InstanceMap::wrapper_from_json (const Jsonipc::JsonValue &value)
{
  if (!value.IsNull())
    warning ("Ase::Writ: non persistent object cannot resolve: %s*", Jsonipc::jsonvalue_to_string<Jsonipc::RELAXED> (value));
  //return nullptr;
  return this->Jsonipc::InstanceMap::wrapper_from_json (value);
}

// == Writ ==
Writ::Writ (Flags flags) :
  root_(*this, std::make_shared<Value> (Value::empty_value)),
  skip_zero_ (flags & SKIP_ZERO),
  skip_emptystring_ (flags & SKIP_EMPTYSTRING),
  relaxed_ (flags & RELAXED),
  dummy_ (std::make_shared<Value>())
{}

ValueP
Writ::dummy ()
{
  if (dummy_->index() != Value::NONE)
    {
      warning ("invalid Writ::dummy assignment: %s", dummy_->repr());
      *dummy_ = Value();
    }
  return dummy_;
}

void
Writ::reset (int mode)
{
  assert_return (mode == 1 || mode == 2);
  in_load_ = mode == 1;
  in_save_ = mode == 2;
  root_.value_ = Value::empty_value;
  linkptrs_.clear();
  links_.clear();
  link_counter_ = 8000;
}

String
Writ::to_json()
{
  Jsonipc::Scope scope (instance_map_);
  rapidjson::Document document (rapidjson::kNullType);
  Jsonipc::JsonValue &docroot = document;
  Jsonipc::JsonAllocator &allocator = document.GetAllocator();
  docroot = Jsonipc::to_json (root_.value_, allocator); // move semantics!
  rapidjson::StringBuffer buffer;
  if (relaxed_)
    {
      constexpr unsigned FLAGS = rapidjson::kWriteNanAndInfFlag;
      rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, FLAGS> writer (buffer);
      writer.SetIndent (' ', 2);
      writer.SetFormatOptions (rapidjson::kFormatSingleLineArray);
      document.Accept (writer);
    }
  else
    {
      rapidjson::Writer<rapidjson::StringBuffer> writer (buffer);
      document.Accept (writer);
    }
  const String output { buffer.GetString(), buffer.GetSize() };
  return output;
}

bool
Writ::from_json (const String &jsonstring)
{
  reset (1);
  Jsonipc::Scope scope (instance_map_);
  rapidjson::Document document;
  Jsonipc::JsonValue &docroot = document;
  constexpr unsigned PARSE_FLAGS =
    rapidjson::kParseFullPrecisionFlag |
    rapidjson::kParseCommentsFlag |
    rapidjson::kParseTrailingCommasFlag |
    rapidjson::kParseNanAndInfFlag |
    rapidjson::kParseEscapedApostropheFlag;
  document.Parse<PARSE_FLAGS> (jsonstring.data(), jsonstring.size());
  if (document.HasParseError())
    {
      // printerr ("%s: JSON-ERROR: %s\n", __func__, jsonstring);
      return false;
    }
  root_.value_ = Jsonipc::from_json<Value> (docroot);
  return true;
}

void
Writ::blank_enum (const String &enumname)
{
  warning ("%s: serialization of enum type without values: %s", "Writ::blank_enum", enumname);
}

void
Writ::not_serializable (const String &classname)
{
  warning ("%s: type not registered as Jsonipc::Serializable<>: %s", "Writ::not_serializable", classname);
}

/// Check for the writable and storage flags in the hints field of typedata.
bool
Writ::typedata_is_loadable (const StringS &typedata, const std::string &fieldname)
{
  return_unless (!typedata.empty() && !fieldname.empty(), true); // avoid constraining unknown fields
  return true; // TODO: check Property hints here
}

/// Check for the readable and storage flags in the hints field of typedata.
bool
Writ::typedata_is_storable  (const StringS &typedata, const std::string &fieldname)
{
  return_unless (!typedata.empty() && !fieldname.empty(), true); // avoid constraining unknown fields
  return true; // TODO: check Property hints here
}

/// Find the minimum value for field of typedata.
bool
Writ::typedata_find_minimum (const StringS &typedata, const std::string &fieldname, long double *limit)
{
  return_unless (!typedata.empty() && !fieldname.empty(), false); // avoid constraining unknown fields
  return false; // TODO: check Property hints here
}

/// Find the maximum value for field of typedata.
bool
Writ::typedata_find_maximum (const StringS &typedata, const std::string &fieldname, long double *limit)
{
  return_unless (!typedata.empty() && !fieldname.empty(), false); // avoid constraining unknown fields
  return false; // TODO: check Property hints here
}

/// Store `Serializable*, ValueP` association when in_save.
void
Writ::prepare_link (Serializable &serializable, ValueP valuep)
{
  assert_return (in_save());
  assert_return (valuep);
  for (LinkEntry &e : links_)
    if (e.sp == &serializable)
      {
        if (e.value)
          warning ("Ase::Writ: duplicate serialization of: (%s)%p", Jsonipc::rtti_typename (serializable), &serializable);
        e.value = valuep;
        return;
      }
  links_.push_back ({ valuep, &serializable, 0 });
}

/// Generate ID to link to a Serializable when in_save.
int64
Writ::use_link (Serializable &serializable)
{
  assert_return (in_save(), 0);
  for (LinkEntry &e : links_)
    if (e.sp == &serializable)
      {
        if (!e.id)
          e.id = ++link_counter_;
        return e.id;
      }
  links_.push_back ({ nullptr, &serializable, ++link_counter_ });
  return links_.back().id;
}

static const char *const ase_linkid = "\u0012.ID"; // DC2 - http://fileformats.archiveteam.org/wiki/C0_controls

/// Insert link ID to store with each Serializable when in_save.
void
Writ::insert_links ()
{
  assert_return (in_save());
  for (LinkEntry &e : links_)
    if (e.id)
      {
        if (e.value && e.value->index() == Value::RECORD)
          {
            ValueR &rec = std::get<ValueR> (*e.value);
            rec.insert (rec.begin(), { ase_linkid, e.id });
          }
        else
          warning ("Ase::Writ: missing serialization of: (%s)%p", Jsonipc::rtti_typename (*e.sp), e.sp);
      }
}

/// Remember link ID encountered during in_load.
void
Writ::collect_link (int64 id, Serializable &serializable)
{
  assert_return (in_load());
  links_.push_back ({ nullptr, &serializable, id });
}

/// Provide `Serializable*` for link ID at the end of in_load.
Serializable*
Writ::resolve_link (int64 id)
{
  assert_return (in_load(), nullptr);
  for (LinkEntry &e : links_)
    if (id == e.id)
      return e.sp;
  return nullptr;
}

/// Resolve and assign all link pointers at the end of in_load.
void
Writ::assign_links ()
{
  assert_return (in_load());
  for (LinkPtr &e : linkptrs_)
    {
      Serializable *serializable = resolve_link (e.id);
      if (serializable)
        *e.spp = serializable;
      else
        warning ("Ase::Writ: failed to resolve serialization link: %d", e.id);
    }
  linkptrs_.clear();
}

// == WritNode ==
WritNode::WritNode (Writ &writ, ValueP vp) :
  writ_ (writ), valuep_ (vp), value_ (*vp)
{
  assert_return (valuep_ != nullptr);
}

/// Refer to a RECORD serialization field by name, insert if needed.
WritNode
WritNode::recfield (const String &fieldname, bool front)
{
  if (in_save())
    {
      assert_return (value_.index() == Value::RECORD, { writ_, dummy() });
      ValueR &rec = std::get<ValueR> (value_);
      return WritNode { writ_, rec.valuep (fieldname, front) };
    }
  if (in_load() && value_.index() == Value::RECORD)
    {
      ValueR &rec = std::get<ValueR> (value_);
      ValueP vp = rec.peek (fieldname);
      if (vp)
        return WritNode { writ_, vp };
    }
  return WritNode { writ_, dummy() };
}

/// Create `std::vector<WritNode>` for serialized arrays during `in_load()`.
WritNodeS
WritNode::to_nodes ()
{
  WritNodeS nodes;
  if (in_load())
    {
      const ValueS &values = value_.as_array();
      nodes.reserve (values.size());
      for (auto &valp : values)
        nodes.push_back ({ writ_, valp });
    }
  return nodes;
}

/// Append new WritNode for serializing arrays during `in_save()`.
WritNode
WritNode::push ()
{
  if (in_save())
    {
      if (value_.index() == Value::NONE)
        value_ = ValueS::empty_array;
      if (value_.index() == Value::ARRAY)
        {
          ValueS &array = std::get<ValueS> (value_);
          array.push_back (Value());
          return { writ_, array.back() };
        }
    }
  const WritNode fallback { writ_, dummy() };
  assert_return (in_save(), fallback);
  assert_return (value_.index() == Value::ARRAY, fallback);
  return fallback;
}

/// Write an object link during saving, queue a deferred pointer during loading.
bool
WritNode::operator& (const WritLink &l)
{
  if (in_save())
    {
      if (*l.spp_)
        {
          const int64 linkid = writ_.use_link (**l.spp_);
          value_ = linkid;
        }
      else
        value_ = Value(); // null
      return true;
    }
  if (in_load())
    {
      const int64 linkid = value_.as_int();
      *l.spp_ = nullptr;
      if (linkid)
        writ_.linkptrs_.push_back ({ l.spp_, linkid });
      return true;
    }
  return false;
}

bool
WritNode::serialize (Serializable &serializable)
{
  if (in_save())
    {
      assert_return (value_.index() == Value::NONE, false);
      value_ = ValueR::empty_record;
      writ_.prepare_link (serializable, valuep_);
      serializable.serialize (*this);
    }
  if (in_load())
    {
      if (value_.index() == Value::RECORD)
        {
          const int64 linkid = (*this)[ase_linkid].as_int();
          if (linkid)
            writ_.collect_link (linkid, serializable);
        }
      serializable.serialize (*this);
    }
  return true;
}

void
WritNode::purge_value ()
{
  const bool purge_emptystring = skip_emptystring(), purge_zero = skip_zero();
  return_unless (purge_emptystring || purge_zero);
  value_.filter ([purge_emptystring,purge_zero] (const ValueField &field) {
    return_unless (field.value, false);
    if (purge_emptystring && field.value->index() == Value::STRING && std::get<String> (*field.value) == "")
      return true;
    if (purge_zero && field.value->index() == Value::INT64 && std::get<int64> (*field.value) == 0)
      return true;
    if (purge_zero && field.value->index() == Value::DOUBLE && std::get<double> (*field.value) == 0)
      return true;
    return false;
  });
}

// == WritLink ==
WritLink::WritLink (Serializable **spp) :
  spp_(spp)
{
  assert_return (spp != nullptr);
}

// == Serializable ==
void
Serializable::serialize (WritNode &xs)
{
  // present, so it can be chained to
}

} // Ase

#include "api.hh"
#include "testing.hh"

namespace { // Anon
using namespace Ase;

struct SBase : Serializable {
  String hi;
  double d;
  int32 i;
  bool b0, b1;
  Error e = Error (0);
  std::vector<char> chars;
  DriverEntry r;
  ServerP serverp;
  void
  fill()
  {
    hi = "hi";
    d = 17.5;
    i = 32;
    b0 = false;
    b1 = true;
    e = Error::IO;
    serverp = Server::instancep();
    chars.push_back ('A');
    chars.push_back ('B');
    chars.push_back ('C');
    r = { .devid = "RS232", .device_name = "Serial", .capabilities = "IO++",
          .device_info = "DEVinfo", .notice = "Handle with care", .priority = 17,
          .readonly = false, .writeonly = false };
  }
  void
  serialize (WritNode &xs) override
  {
    xs["b0"] & b0;
    xs["b1"] & b1;
    xs["i"] & i;
    xs["d"] & d;
    xs["s"] & hi;
    xs["e"] & e;
    xs["chars"] & chars;
    xs["driver"] & r;
    // xs["server"] & serverp; // <- pointer is not persistent
  }
};

template<typename T> static T via_json (const T &v) { return json_parse<T> (json_stringify (v)); }

TEST_INTEGRITY (ase_serialize);
static void
ase_serialize()
{
  // Basics
  TASSERT (false == via_json (false));                  // BOOL
  TASSERT (true == via_json (true));
  TASSERT (-2 == via_json (-2));                        // INT64
  TASSERT (+3.5 == via_json (+3.5));                    // DOUBLE
  TASSERT ("" == via_json (String ("")));               // STRING
  TASSERT ("x123y" == via_json (String ("x123y")));
  TASSERT (Error::PERMS == via_json (Error::PERMS));    // ENUM
  { // ARRAY
    std::vector<double> floats = { 1e3, 0, -9, +0.75, +6, -0.5, 32767 }, floats2 = floats;
    TASSERT (floats == via_json (floats2));
  }
  { // RECORD
    using namespace MakeIcon;
    Choice choice = { "grump", "¿"_uc, "Grump", "A flashy Grump", "Notice", "Warn" }, choice2 = via_json (choice);
    TASSERT (choice.ident == choice2.ident && choice.icon == choice2.icon && choice.label == choice2.label &&
             choice.blurb == choice2.blurb && choice.notice == choice2.notice && choice.warning == choice2.warning);
  }
  { // tuple
    using Tuple = std::tuple<String,long,double>;
    Tuple tuple = { "TUPLE", -618033988, 1.6180339887498948482 };
    Tuple u = via_json (tuple);
    TASSERT (std::get<0> (u) == "TUPLE" && std::get<long> (u) == -618033988);
    TASSERT (fabs (std::get<2> (u) - 1.6180339887498948482) < 1e-16);
  }
  { // Json types
    std::string s;
    bool b;
    s = json_stringify (true);                            TASSERT (s == "true");
    TASSERT (json_parse (s, b) && true == b);
    double d;
    s = json_stringify (-0.17);                           TASSERT (s == "-0.17");
    TASSERT (json_parse (s, d) && -0.17 == d);
    int i;
    s = json_stringify (32768);                           TASSERT (s == "32768");
    TASSERT (json_parse (s, i) && 32768 == i);
    Error e;
    s = json_stringify (Ase::Error::IO);                  TASSERT (s == "\"Ase.Error.IO\"");
    TASSERT (json_parse (s, e) && Error::IO == e);
    String str;
    s = json_stringify (String ("STRing"));               TASSERT (s == "\"STRing\"");
    TASSERT (json_parse (s, str) && "STRing" == str);
    Value val;
    s = json_stringify (Value (0.5));                     TASSERT (s == "0.5");
    TASSERT (json_parse (s, val) && Value (0.5) == val);
    ValueS vs;
    s = json_stringify (ValueS ({ true, 5, "HI" }));      TASSERT (s == "[true,5,\"HI\"]");
    TASSERT (json_parse (s, vs) && ValueS ({ true, 5, "HI" }) == vs);
    ValueR vr;
    s = json_stringify (ValueR ({ {"a", 1}, {"b", "B"} })); TASSERT (s == "{\"a\":1,\"b\":\"B\"}");
    TASSERT (json_parse (s, vr) && ValueR ({ {"a", 1}, {"b", "B"} }) == vr);
  }
  { // Jsonipc::Serializable<>
    struct Test1 {
      int i; std::string t;
      bool operator== (const Test1 &o) const { return i == o.i && t == o.t; }
    };
    Jsonipc::Serializable<Test1> test1;
    test1 .set ("i", &Test1::i) .set ("t", &Test1::t) ;
    Test1 t1;
    String s = json_stringify (Test1 ({ 256, "tree" }));        TASSERT (s == "{\"i\":256,\"t\":\"tree\"}");
    TASSERT (json_parse (s, t1) && Test1 ({ 256, "tree" }) == t1);
    struct Test2 {
      int i;
      Test1 t1;
      bool operator== (const Test2 &o) const {
        return i == o.i && t1 == o.t1 && server == o.server && serverp == o.serverp;
      }
      Server *server = nullptr; // not-persistent
      ServerP serverp; // not-persistent
    };
    Jsonipc::Serializable<Test2> test2;
    test2
      .set ("i", &Test2::i)
      .set ("t1", &Test2::t1)
      .set ("server", &Test2::server) // <- pointer is not persistent
      // .set ("serverp", &Test2::serverp) // <- pointer is not persistent
      ;
    Test2 t2, t2orig = Test2 ({ 777, { 9, "nine" }, });
    s = json_stringify (Test2 (t2orig));
    TASSERT (json_parse (s, t2));
    TASSERT (json_parse (s, t2) && t2 == t2orig);
    TASSERT (s == R"'''({"i":777,"server":null,"t1":{"i":9,"t":"nine"}})'''");
  }
  { // Ase::Serializable derived classes
    SBase sbase1;
    sbase1.fill();
    const String json1 = json_stringify (sbase1);
    // printerr ("SBase1: %s\n", json1);
    SBase sbase2;
    json_parse (json1, sbase2);
    TASSERT (sbase1.chars == sbase2.chars && sbase1.d == sbase2.d && sbase1.e == sbase2.e);
    const String json2 = json_stringify (sbase2);
    // printerr ("SBase2: %s\n", json2);
    TASSERT (json1 == json2);
  }
}

// Ase::Serializable hierarchy test
struct FrobnicatorBase : public virtual Serializable {
  uint32_t flags_ = 0x01020304;
protected:
  void
  serialize (WritNode &xs) override
  {
    xs["flags"] & flags_;
  }
};
struct FrobnicatorSpecial : public FrobnicatorBase {
  String   kind_;
  float    factor_ = 0;
  int64    state_ = 0;
  explicit FrobnicatorSpecial (const String &k) : kind_ (k)
  {
    TASSERT (kind_ == "K321" || kind_ == "Special");
  }
protected:
  void
  serialize (WritNode &xs) override
  {
    // serialize *constructor* value, see caller/creator
    xs["kind"] & kind_;
    // chaining for base class fields
    FrobnicatorBase::serialize (xs);
    // avoid saving defaults according to Writ config
    if (!xs.skip_emptystring() || state_)
      xs["state"] & this->state_;
    if (!xs.skip_zero() || factor_ != 0)
      xs["factor"] & factor_;
  }
};
using FrobnicatorSpecialP = std::shared_ptr<FrobnicatorSpecial>;
struct FrobnicatorImpl : public FrobnicatorBase {
  String           kind_;
  bool             yesno_ = 0;
  Error            error_ = {};
  FrobnicatorSpecialP special_;
  std::vector<Serializable*> children_;
  std::vector<std::vector<float>> matrix_;
  std::vector<int> nums_;
  Serializable *sibling_ = nullptr;
  explicit FrobnicatorImpl (const String &t = "") : kind_ (t) {}
  Serializable&
  create_track  (const String &kind)
  {
    Serializable *c;
    if (kind == "Special")
      c = new FrobnicatorSpecial (kind);
    else
      c = new FrobnicatorImpl (kind);
    return *children_.emplace_back (c);
  }
  void
  populate (bool extended = false)
  {
    if (extended)
      {
        special_ = std::make_shared<FrobnicatorSpecial> ("K321");
        special_->flags_ = 321;
      }
    nums_ = { 1, 2, 3 };
    if (extended)
      matrix_ = { { 9, 8, 7 }, { 6, 5, 4 }, };
    error_ = Error::NO_MEMORY;
  }
protected:
  void
  serialize (WritNode &xs) override
  {
    // always skips empty default
    if (xs.in_load() || !kind_.empty())
      xs["kind"] & kind_;
    // chaining for base class fields
    FrobnicatorBase::serialize (xs);
    // flat arrays
    xs["nums"] & nums_;
    // nested arrays
    xs["matrix"] & matrix_;
    // conditional serialization
    if (kind_ == "")
      xs["error"] & error_;
    // nested object, maybe null
    if (xs.loadable ("special"))
      special_ = std::make_shared<FrobnicatorSpecial> (xs["special"]["kind"].as_string());
    if (special_)
      xs["special"] & *special_;
    // saving children
    if (xs.in_save())                           // children_ in_save
      for (auto &childp : children_)
        {
          WritNode xc = xs["tracks"].push();
          xc & *childp;
        }
    // loading children
    if (xs.in_load())                           // children_ in_load
      for (auto &xc : xs["tracks"].to_nodes())
        { // parses child constructor arg on the fly
          Serializable &child = create_track (xc["kind"].as_string());
          xc & child;
        }
    // the `sibling_` is assigned at the *end* of the in_load serialization phase
    xs["sibling"] & WritLink (&sibling_);
    // serialization through alias
    String yn = yesno_ ? "yes" : "no";
    xs["Q?"] & yn;
    if (xs.in_load() && xs.has ("Q?"))
      yesno_ = yn == "yes";
  }
};

TEST_INTEGRITY (test_serializable_hierarchy);
static void
test_serializable_hierarchy()
{
  const bool V = 0;
  String streamtext1;
  {
    FrobnicatorImpl prjct;
    prjct.flags_ = 1717;
    prjct.populate (true);
    TASSERT (prjct.children_.size() == 0);
    auto &nidi = prjct.create_track ("Nidi");
    dynamic_cast<FrobnicatorBase*> (&nidi)->flags_ = 0x02020202;
    prjct.sibling_ = &prjct.create_track ("Odio");
    FrobnicatorImpl *fimpl = dynamic_cast<FrobnicatorImpl*> (prjct.sibling_);
    fimpl->populate (false);
    fimpl->yesno_ = true;
    TASSERT (prjct.children_.size() == 2);
    FrobnicatorSpecial *special = dynamic_cast<FrobnicatorSpecial*> (&prjct.create_track ("Special"));
    special->flags_ = 0xeaeaeaea;
    special->factor_ = 2.5;
    special->state_ = -17;
    TASSERT (prjct.children_.size() == 3);
    streamtext1 = json_stringify (prjct, Writ::RELAXED);
    TASSERT (prjct.sibling_ && fimpl && (fimpl->nums_ == std::vector<int> { 1, 2, 3 }));
  }
  if (V)
    printerr ("%s\n", streamtext1);
  String streamtext2;
  {
    FrobnicatorImpl prjct;
    const bool success = json_parse (streamtext1, prjct);
    TASSERT (success);
    TASSERT (prjct.children_.size() > 0 && dynamic_cast<FrobnicatorImpl*> (prjct.children_[0])->kind_ == "Nidi");
    TASSERT (prjct.children_.size() > 1 && dynamic_cast<FrobnicatorImpl*> (prjct.children_[1])->kind_ == "Odio");
    TASSERT (prjct.children_.size() > 2 && dynamic_cast<FrobnicatorSpecial*> (prjct.children_[2])->kind_ == "Special");
    streamtext2 = json_stringify (prjct, Writ::RELAXED);
    FrobnicatorImpl *fimpl = dynamic_cast<FrobnicatorImpl*> (prjct.sibling_);
    TASSERT (prjct.sibling_ && fimpl && (fimpl->nums_ == std::vector<int> { 1, 2, 3 }));
  }
  if (V)
    printerr ("%s\n", streamtext2);
  TASSERT (streamtext1 == streamtext2);
}

} // Anon
