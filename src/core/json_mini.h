#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vakit {

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::vector<std::pair<std::string, JsonValue>> objVal;

    static JsonValue MakeNull() { return {}; }
    static JsonValue MakeBool(bool b) { JsonValue v; v.type = Type::Bool; v.boolVal = b; return v; }
    static JsonValue MakeNumber(double n) { JsonValue v; v.type = Type::Number; v.numVal = n; return v; }
    static JsonValue MakeInt(int n) { return MakeNumber(static_cast<double>(n)); }
    static JsonValue MakeString(std::string s) { JsonValue v; v.type = Type::String; v.strVal = std::move(s); return v; }
    static JsonValue MakeArray() { JsonValue v; v.type = Type::Array; return v; }
    static JsonValue MakeObject() { JsonValue v; v.type = Type::Object; return v; }

    void Set(std::string key, JsonValue val);
    const JsonValue* Find(std::string_view key) const;
    std::string GetString(std::string_view key, std::string_view def = "") const;
    double GetDouble(std::string_view key, double def = 0.0) const;
    int GetInt(std::string_view key, int def = 0) const;
    bool GetBool(std::string_view key, bool def = false) const;
};

std::optional<JsonValue> JsonParse(std::string_view text);
std::string JsonSerialize(const JsonValue& value, bool pretty = true);

} // namespace vakit
