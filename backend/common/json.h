// common/json.h
// -----------------------------------------------------------------------------
// A tiny dependency-free JSON helper shared by every module.
//
//   json::Writer  - builds JSON text:  w.beginObject().field("a", 1).endObject()
//   json::Value   - parsed JSON tree:  json::parse(text, value, err)
//
// This is plumbing, not part of the DSA syllabus, so it uses std::vector/string.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

namespace json {

// ============================== Writer =========================================
class Writer {
public:
    Writer& beginObject() { sep(); out_ += '{'; first_.push_back(true); return *this; }
    Writer& endObject() { out_ += '}'; first_.pop_back(); return *this; }
    Writer& beginArray() { sep(); out_ += '['; first_.push_back(true); return *this; }
    Writer& endArray() { out_ += ']'; first_.pop_back(); return *this; }

    Writer& key(const std::string& k) { sep(); appendQuoted(k); out_ += ':'; afterKey_ = true; return *this; }

    Writer& value(const std::string& s) { sep(); appendQuoted(s); return *this; }
    Writer& value(const char* s) { return value(std::string(s)); }
    Writer& value(bool b) { sep(); out_ += b ? "true" : "false"; return *this; }
    Writer& value(double d) {
        sep();
        char buf[48];
        std::snprintf(buf, sizeof buf, "%.2f", d);
        out_ += buf;
        return *this;
    }
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value>::type>
    Writer& value(T v) { sep(); out_ += std::to_string(v); return *this; }
    Writer& null() { sep(); out_ += "null"; return *this; }
    Writer& raw(const std::string& jsonText) { sep(); out_ += jsonText; return *this; }

    template <typename T>
    Writer& field(const std::string& k, const T& v) { key(k); return value(v); }

    const std::string& str() const { return out_; }

private:
    std::string out_;
    std::vector<bool> first_;
    bool afterKey_ = false;

    void sep() {
        if (afterKey_) { afterKey_ = false; return; }
        if (!first_.empty()) {
            if (!first_.back()) out_ += ',';
            first_.back() = false;
        }
    }
    void appendQuoted(const std::string& s) {
        out_ += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': out_ += "\\\""; break;
                case '\\': out_ += "\\\\"; break;
                case '\n': out_ += "\\n"; break;
                case '\r': out_ += "\\r"; break;
                case '\t': out_ += "\\t"; break;
                default:
                    if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out_ += b; }
                    else out_ += static_cast<char>(c);
            }
        }
        out_ += '"';
    }
};

// ============================== Parser =========================================
struct Value {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> items;          // Array elements
    std::vector<std::string> keys;     // Object keys   (parallel to `items`)

    bool isNull() const { return type == Null; }
    bool isString() const { return type == String; }
    bool isNumber() const { return type == Number; }
    bool isArray() const { return type == Array; }
    bool isObject() const { return type == Object; }

    // Object member lookup (nullptr when missing or not an object)
    const Value* get(const std::string& k) const {
        if (type != Object) return nullptr;
        for (size_t i = 0; i < keys.size(); ++i) if (keys[i] == k) return &items[i];
        return nullptr;
    }
    std::string asString(const std::string& def = "") const { return type == String ? string : def; }
    double asNumber(double def = 0) const { return type == Number ? number : def; }
};

namespace detail {
class Parser {
public:
    Parser(const std::string& s, std::string& err) : s_(s), err_(err) {}
    bool run(Value& out) {
        skipWs();
        if (!parseValue(out, 0)) return false;
        skipWs();
        if (pos_ != s_.size()) return fail("unexpected characters after JSON value");
        return true;
    }
private:
    const std::string& s_;
    std::string& err_;
    size_t pos_ = 0;

    bool fail(const char* m) { err_ = std::string(m) + " at position " + std::to_string(pos_); return false; }
    void skipWs() { while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\t' || s_[pos_] == '\r')) ++pos_; }
    bool lit(const char* word) {
        size_t n = std::char_traits<char>::length(word);
        if (s_.compare(pos_, n, word) == 0) { pos_ += n; return true; }
        return false;
    }
    static void appendUtf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o += static_cast<char>(cp);
        else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += static_cast<char>(0xE0 | (cp >> 12)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { o += static_cast<char>(0xF0 | (cp >> 18)); o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    bool hex4(unsigned& cp) {
        if (pos_ + 4 > s_.size()) return fail("bad \\u escape");
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= c - '0';
            else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool parseString(std::string& out) {
        ++pos_;  // opening quote
        out.clear();
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (pos_ >= s_.size()) break;
            char e = s_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && s_.compare(pos_, 2, "\\u") == 0) {
                        pos_ += 2;
                        unsigned lo = 0;
                        if (!hex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool parseValue(Value& v, int depth) {
        if (depth > 64) return fail("JSON nested too deeply");
        if (pos_ >= s_.size()) return fail("unexpected end of JSON");
        char c = s_[pos_];
        if (c == '{') {
            v.type = Value::Object;
            ++pos_; skipWs();
            if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
            while (true) {
                skipWs();
                if (pos_ >= s_.size() || s_[pos_] != '"') return fail("expected object key");
                std::string k;
                if (!parseString(k)) return false;
                skipWs();
                if (pos_ >= s_.size() || s_[pos_] != ':') return fail("expected ':'");
                ++pos_; skipWs();
                Value child;
                if (!parseValue(child, depth + 1)) return false;
                v.keys.push_back(k);
                v.items.push_back(std::move(child));
                skipWs();
                if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
                if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = Value::Array;
            ++pos_; skipWs();
            if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
            while (true) {
                skipWs();
                Value child;
                if (!parseValue(child, depth + 1)) return false;
                v.items.push_back(std::move(child));
                skipWs();
                if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
                if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = Value::String; return parseString(v.string); }
        if (lit("true")) { v.type = Value::Bool; v.boolean = true; return true; }
        if (lit("false")) { v.type = Value::Bool; v.boolean = false; return true; }
        if (lit("null")) { v.type = Value::Null; return true; }
        // number
        const char* start = s_.c_str() + pos_;
        char* end = nullptr;
        double d = std::strtod(start, &end);
        if (end == start) return fail("unexpected character");
        v.type = Value::Number;
        v.number = d;
        pos_ += static_cast<size_t>(end - start);
        return true;
    }
};
}  // namespace detail

// Parse `text` into `out`. On failure returns false and fills `err`.
inline bool parse(const std::string& text, Value& out, std::string& err) {
    detail::Parser p(text, err);
    return p.run(out);
}

}  // namespace json
