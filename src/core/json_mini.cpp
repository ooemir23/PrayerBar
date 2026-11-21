#include "json_mini.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace vakit {

void JsonValue::Set(std::string key, JsonValue val) {
    type = Type::Object;
    for (auto& kv : objVal) {
        if (kv.first == key) {
            kv.second = std::move(val);
            return;
        }
    }
    objVal.emplace_back(std::move(key), std::move(val));
}

const JsonValue* JsonValue::Find(std::string_view key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& kv : objVal) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

std::string JsonValue::GetString(std::string_view key, std::string_view def) const {
    const JsonValue* v = Find(key);
    if (v && v->type == Type::String) return v->strVal;
    return std::string(def);
}

double JsonValue::GetDouble(std::string_view key, double def) const {
    const JsonValue* v = Find(key);
    if (!v) return def;
    if (v->type == Type::Number) return v->numVal;
    if (v->type == Type::String) {
        char* end = nullptr;
        double d = std::strtod(v->strVal.c_str(), &end);
        if (end && end != v->strVal.c_str()) return d;
    }
    return def;
}

int JsonValue::GetInt(std::string_view key, int def) const {
    const JsonValue* v = Find(key);
    if (!v) return def;
    if (v->type == Type::Number) return static_cast<int>(std::llround(v->numVal));
    if (v->type == Type::String) {
        char* end = nullptr;
        long n = std::strtol(v->strVal.c_str(), &end, 10);
        if (end && end != v->strVal.c_str()) return static_cast<int>(n);
    }
    return def;
}

bool JsonValue::GetBool(std::string_view key, bool def) const {
    const JsonValue* v = Find(key);
    if (v && v->type == Type::Bool) return v->boolVal;
    return def;
}

namespace {

void AppendUtf8CodePoint(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {
        if (s_.size() >= 3 &&
            static_cast<unsigned char>(s_[0]) == 0xEF &&
            static_cast<unsigned char>(s_[1]) == 0xBB &&
            static_cast<unsigned char>(s_[2]) == 0xBF) {
            pos_ = 3;
        }
    }

    std::optional<JsonValue> ParseRoot() {
        SkipWs();
        auto val = ParseValue(0);
        if (!val) return std::nullopt;
        SkipWs();
        return val;
    }

private:
    std::string_view s_;
    size_t pos_ = 0;

    void SkipWs() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\r' || s_[pos_] == '\n')) {
            ++pos_;
        }
    }

    bool Match(char c) {
        if (pos_ < s_.size() && s_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    std::optional<JsonValue> ParseValue(int depth) {
        if (depth > 64) return std::nullopt;
        SkipWs();
        if (pos_ >= s_.size()) return std::nullopt;
        char c = s_[pos_];
        if (c == '{') return ParseObject(depth + 1);
        if (c == '[') return ParseArray(depth + 1);
        if (c == '"') {
            auto str = ParseString();
            if (!str) return std::nullopt;
            return JsonValue::MakeString(std::move(*str));
        }
        if (c == 't' && s_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return JsonValue::MakeBool(true);
        }
        if (c == 'f' && s_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return JsonValue::MakeBool(false);
        }
        if (c == 'n' && s_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return JsonValue::MakeNull();
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return ParseNumber();
        }
        return std::nullopt;
    }

    std::optional<uint32_t> ParseHex4() {
        if (pos_ + 4 > s_.size()) return std::nullopt;
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            val <<= 4;
            if (c >= '0' && c <= '9') val |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') val |= static_cast<uint32_t>(10 + c - 'a');
            else if (c >= 'A' && c <= 'F') val |= static_cast<uint32_t>(10 + c - 'A');
            else return std::nullopt;
        }
        return val;
    }

    std::optional<std::string> ParseString() {
        if (!Match('"')) return std::nullopt;
        std::string out;
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= s_.size()) return std::nullopt;
                char esc = s_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        auto h1 = ParseHex4();
                        if (!h1) return std::nullopt;
                        uint32_t cp = *h1;
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 2 <= s_.size() &&
                            s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            auto h2 = ParseHex4();
                            if (!h2) return std::nullopt;
                            if (*h2 >= 0xDC00 && *h2 <= 0xDFFF) {
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (*h2 - 0xDC00));
                            }
                        }
                        AppendUtf8CodePoint(out, cp);
                        break;
                    }
                    default:
                        return std::nullopt;
                }
            } else {
                out.push_back(c);
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseNumber() {
        size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        }
        std::string tmp(s_.substr(start, pos_ - start));
        char* end = nullptr;
        double d = std::strtod(tmp.c_str(), &end);
        if (!end || end == tmp.c_str()) return std::nullopt;
        return JsonValue::MakeNumber(d);
    }

    std::optional<JsonValue> ParseArray(int depth) {
        if (!Match('[')) return std::nullopt;
        JsonValue arr = JsonValue::MakeArray();
        SkipWs();
        if (Match(']')) return arr;
        while (pos_ < s_.size()) {
            auto item = ParseValue(depth);
            if (!item) return std::nullopt;
            arr.arrVal.push_back(std::move(*item));
            SkipWs();
            if (Match(']')) return arr;
            if (!Match(',')) return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseObject(int depth) {
        if (!Match('{')) return std::nullopt;
        JsonValue obj = JsonValue::MakeObject();
        SkipWs();
        if (Match('}')) return obj;
        while (pos_ < s_.size()) {
            SkipWs();
            auto key = ParseString();
            if (!key) return std::nullopt;
            SkipWs();
            if (!Match(':')) return std::nullopt;
            auto val = ParseValue(depth);
            if (!val) return std::nullopt;
            obj.Set(std::move(*key), std::move(*val));
            SkipWs();
            if (Match('}')) return obj;
            if (!Match(',')) return std::nullopt;
        }
        return std::nullopt;
    }
};

void AppendEscapedString(std::string& out, std::string_view s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

void SerializeRec(const JsonValue& v, std::string& out, bool pretty, int indent) {
    auto addIndent = [&](int level) {
        if (pretty) out.append(static_cast<size_t>(level * 2), ' ');
    };

    switch (v.type) {
        case JsonValue::Type::Null:
            out += "null";
            break;
        case JsonValue::Type::Bool:
            out += v.boolVal ? "true" : "false";
            break;
        case JsonValue::Type::Number: {
            char buf[64];
            if (std::isfinite(v.numVal) && std::floor(v.numVal) == v.numVal &&
                std::abs(v.numVal) < 9007199254740992.0) {
                snprintf(buf, sizeof(buf), "%.0f", v.numVal);
            } else {
                snprintf(buf, sizeof(buf), "%.10g", v.numVal);
            }
            out += buf;
            break;
        }
        case JsonValue::Type::String:
            AppendEscapedString(out, v.strVal);
            break;
        case JsonValue::Type::Array:
            if (v.arrVal.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            if (pretty) out.push_back('\n');
            for (size_t i = 0; i < v.arrVal.size(); ++i) {
                addIndent(indent + 1);
                SerializeRec(v.arrVal[i], out, pretty, indent + 1);
                if (i + 1 < v.arrVal.size()) out.push_back(',');
                if (pretty) out.push_back('\n');
            }
            addIndent(indent);
            out.push_back(']');
            break;
        case JsonValue::Type::Object:
            if (v.objVal.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            if (pretty) out.push_back('\n');
            for (size_t i = 0; i < v.objVal.size(); ++i) {
                addIndent(indent + 1);
                AppendEscapedString(out, v.objVal[i].first);
                out += pretty ? ": " : ":";
                SerializeRec(v.objVal[i].second, out, pretty, indent + 1);
                if (i + 1 < v.objVal.size()) out.push_back(',');
                if (pretty) out.push_back('\n');
            }
            addIndent(indent);
            out.push_back('}');
            break;
    }
}

} // namespace

std::optional<JsonValue> JsonParse(std::string_view text) {
    Parser p(text);
    return p.ParseRoot();
}

std::string JsonSerialize(const JsonValue& value, bool pretty) {
    std::string out;
    out.reserve(512);
    SerializeRec(value, out, pretty, 0);
    return out;
}

} // namespace vakit
