// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_JSONAPI_HH__
#define __ASE_JSONAPI_HH__

#include <ase/websocket.hh>
#include <ase/value.hh>
#include <jsonipc/jsonipc.hh>

namespace Ase {

void                 jsonapi_require_auth    (const String &subprotocol);
WebSocketConnectionP jsonapi_make_connection (WebSocketConnection::Internals&);

/// Convert between Value and Jsonipc::JsonValue
struct ConvertValue {
  static Value
  from_json (const Jsonipc::JsonValue &v)
  {
    Value val;
    switch (v.GetType())
      {
      case rapidjson::kNullType:
        val = Value{}; // NONE
        break;
      case rapidjson::kFalseType:
        val = false;
        break;
      case rapidjson::kTrueType:
        val = true;
        break;
      case rapidjson::kStringType:
        val = Jsonipc::from_json<std::string> (v);
        break;
      case rapidjson::kNumberType:
        if      (v.IsInt())     val = v.GetInt();
        else if (v.IsUint())    val = v.GetUint();
        else if (v.IsInt64())   val = v.GetInt64();
        else if (v.IsUint64())  val = int64 (v.GetUint64());
        else                    val = v.GetDouble();
        break;
      case rapidjson::kArrayType:
        sequence_from_json_array (val, v);
        break;
      case rapidjson::kObjectType: // RECORD + INSTANCE
        value_from_json_object (val, v);
        break;
    };
    return val;
  }
  static Jsonipc::JsonValue
  to_json (const Value &val, Jsonipc::JsonAllocator &allocator)
  {
    using namespace Jsonipc;
    switch (val.index())
      {
      case Value::BOOL:     return JsonValue (std::get<bool> (val));
      case Value::INT64:    return JsonValue (std::get<int64> (val));
      case Value::DOUBLE:   return JsonValue (std::get<double> (val));
      case Value::STRING:   return Jsonipc::to_json (std::get<String> (val), allocator);
      case Value::ARRAY:    return sequence_to_json_array (std::get<ValueS> (val), allocator);
      case Value::RECORD:   return record_to_json_object (std::get<ValueR> (val), allocator);
      case Value::INSTANCE: return Jsonipc::to_json (std::get<InstanceP> (val), allocator);
      case Value::NONE:     return JsonValue(); // null
      }
    return JsonValue(); // null
  }
  static void
  sequence_from_json_array (Value &val, const Jsonipc::JsonValue &v)
  {
    const size_t l = v.Size();
    ValueS s;
    s.reserve (l);
    for (size_t i = 0; i < l; ++i)
      s.push_back (ConvertValue::from_json (v[i]));
    val = std::move (s);
  }
  static Jsonipc::JsonValue
  sequence_to_json_array (const ValueS &seq, Jsonipc::JsonAllocator &allocator)
  {
    const size_t l = seq.size();
    Jsonipc::JsonValue jarray (rapidjson::kArrayType);
    jarray.Reserve (l, allocator);
    for (size_t i = 0; i < l; ++i)
      if (seq[i])
        jarray.PushBack (ConvertValue::to_json (*seq[i], allocator).Move(), allocator);
    return jarray;
  }
  static void
  value_from_json_object (Value &val, const Jsonipc::JsonValue &v)
  {
    ValueR rec;
    rec.reserve (v.MemberCount());
    for (const auto &field : v.GetObject())
      {
        const std::string key = field.name.GetString();
        if (key == "$class" || key == "$id") // actually INSTANCE
          {
            val = Jsonipc::from_json<InstanceP> (v);
            return;
          }
        rec[key] = ConvertValue::from_json (field.value);
      }
    val = std::move (rec);
  }
  static Jsonipc::JsonValue
  record_to_json_object (const ValueR &rec, Jsonipc::JsonAllocator &allocator)
  {
    Jsonipc::JsonValue jobject (rapidjson::kObjectType);
    jobject.MemberReserve (rec.size(), allocator);
    for (auto const &field : rec)
      if (field.value)
        jobject.AddMember (Jsonipc::JsonValue (field.name.c_str(), allocator),
                           ConvertValue::to_json (*field.value, allocator).Move(), allocator);
    return jobject;
  }
};

/// Convert between ValueP and Jsonipc::JsonValue
struct ConvertValueP {
  static ValueP
  from_json (const Jsonipc::JsonValue &value)
  {
    auto val = ConvertValue::from_json (value);
    return std::make_shared<Value> (std::move (val)); // yields NONE for kNullType
  }
  static Jsonipc::JsonValue
  to_json (const ValueP &valp, Jsonipc::JsonAllocator &allocator)
  {
    if (!valp)
      return Jsonipc::JsonValue(); // yields kNullType for nullptr
    auto val = *valp;
    return ConvertValue::to_json (val, allocator);
  }
};

/// Convert between ValueR and Jsonipc::JsonValue
struct ConvertValueR {
  static ValueR
  from_json (const Jsonipc::JsonValue &v)
  {
    ValueR rec;
    if (v.IsObject())
      {
        rec.reserve (v.MemberCount());
        for (const auto &field : v.GetObject())
          {
            const std::string key = field.name.GetString();
            rec[key] = ConvertValue::from_json (field.value);
          }
      }
    return rec;
  }
  static Jsonipc::JsonValue
  to_json (const ValueR &rec, Jsonipc::JsonAllocator &allocator)
  {
    Jsonipc::JsonValue jobject (rapidjson::kObjectType);
    jobject.MemberReserve (rec.size(), allocator);
    for (auto const &field : rec)
      if (field.value)
        jobject.AddMember (Jsonipc::JsonValue (field.name.c_str(), allocator),
                           ConvertValue::to_json (*field.value, allocator).Move(), allocator);
    return jobject;
  }
};

/// Convert between std::shared_ptr<ValueR> and Jsonipc::JsonValue
struct ConvertValueRP {
  using ValueRP = std::shared_ptr<ValueR>;
  static ValueRP
  from_json (const Jsonipc::JsonValue &value)
  {
    ValueR rec = ConvertValueR::from_json (value);
    return std::make_shared<ValueR> (std::move (rec));
  }
  static Jsonipc::JsonValue
  to_json (const ValueRP &recp, Jsonipc::JsonAllocator &allocator)
  {
    if (!recp)
      return Jsonipc::JsonValue(); // null
    return ConvertValueR::to_json (*recp, allocator);
  }
};

/// Convert between Jsonipc::JsonValue to JsTrigger
struct ConvertJsTrigger {
  static JsTrigger lookup (const String &triggerid);
  static JsTrigger
  from_json (const Jsonipc::JsonValue &v)
  {
    if (v.IsString())
      return lookup (Jsonipc::from_json<std::string> (v));
    return {};
  }
  static Jsonipc::JsonValue
  to_json (const JsTrigger&, Jsonipc::JsonAllocator &allocator)
  {
    return {}; // null
  }
};

} // Ase

namespace Jsonipc {
template<> struct Convert<Ase::Value> : Ase::ConvertValue {};
template<> struct Convert<Ase::ValueP> : Ase::ConvertValueP {};
template<> struct Convert<Ase::ValueR> : Ase::ConvertValueR {};
template<> struct Convert<std::shared_ptr<Ase::ValueR>> : Ase::ConvertValueRP {};
template<> struct Convert<Ase::JsTrigger> : Ase::ConvertJsTrigger {};
} // Jsonipc

#endif // __ASE_JSONAPI_HH__
