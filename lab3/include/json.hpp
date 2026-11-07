/*
    File: json.hpp
    Usage: Minimal JSON (header-only) for envelope, metadata, and KAT vectors.
           No external library dependencies. Strict parser: syntax errors => FormatError (fail-closed).
*/

#pragma once

#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "utils.hpp"

using namespace std;

namespace minijson {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool b = false;
    double num = 0.0;
    string str;
    vector<Value> arr;
    vector<string> keys;  // Obj: keys (preserve order)
    vector<Value> vals;        // Object: values parallel to keys

    static Value make_string(string s) { Value v; v.type = Type::String; v.str = std::move(s); return v; }
    static Value make_number(double d) { Value v; v.type = Type::Number; v.num = d; return v; }
    static Value make_bool(bool x) { Value v; v.type = Type::Bool; v.b = x; return v; }
    static Value make_object() { Value v; v.type = Type::Object; return v; }
    static Value make_array() { Value v; v.type = Type::Array; return v; }

    bool is_string() const { return type == Type::String; }
    bool is_number() const { return type == Type::Number; }
    bool is_object() const { return type == Type::Object; }
    bool is_array() const { return type == Type::Array; }

    void set(const string& k, Value v) { keys.push_back(k); vals.push_back(std::move(v)); }

    const Value* find(const string& k) const {
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == k) return &vals[i];
        return nullptr;
    }
};

// ---- Serializer ----
inline string quote(const string& s) {
    string out = "\"";
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
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);  // Keep 
                }
        }
    }
    out += '"';
    return out;
}

inline string dump(const Value& v, int indent = 2, int level = 0) {
    const string pad(static_cast<size_t>(indent * (level + 1)), ' ');
    const string padEnd(static_cast<size_t>(indent * level), ' ');
    switch (v.type) {
        case Value::Type::Null: return "null";
        case Value::Type::Bool: return v.b ? "true" : "false";
        case Value::Type::Number: {
            char buf[64];
            if (v.num == static_cast<double>(static_cast<long long>(v.num)))
                std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v.num));
            else
                std::snprintf(buf, sizeof buf, "%.17g", v.num);
            return buf;
        }
        case Value::Type::String: return quote(v.str);
        case Value::Type::Array: {
            if (v.arr.empty()) return "[]";
            string s = "[\n";
            for (size_t i = 0; i < v.arr.size(); ++i) {
                s += pad + dump(v.arr[i], indent, level + 1);
                s += (i + 1 < v.arr.size()) ? ",\n" : "\n";
            }
            return s + padEnd + "]";
        }
        case Value::Type::Object: {
            if (v.keys.empty()) return "{}";
            string s = "{\n";
            for (size_t i = 0; i < v.keys.size(); ++i) {
                s += pad + quote(v.keys[i]) + ": " + dump(v.vals[i], indent, level + 1);
                s += (i + 1 < v.keys.size()) ? ",\n" : "\n";
            }
            return s + padEnd + "}";
        }
    }
    return "null";
}

// ---- Parser ----
class Parser {
public:
    explicit Parser(const string& text) : s_(text) {}

    Value parse() {
        Value v = parse_value(0);
        skip_ws();
        if (pos_ != s_.size()) fail();
        return v;
    }

private:
    const string& s_;
    size_t pos_ = 0;
    static constexpr int kMaxDepth = 64;

    [[noreturn]] void fail() const { throw rsalab::FormatError("malformed JSON"); }

    void skip_ws() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }
    char peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }
    bool consume(char c) { if (peek() == c && pos_ < s_.size()) { ++pos_; return true; } return false; }
    void expect(char c) { if (!consume(c)) fail(); }
    void expect_literal(const char* lit) {
        for (const char* p = lit; *p; ++p) {
            if (pos_ >= s_.size() || s_[pos_] != *p) fail();
            ++pos_;
        }
    }

    Value parse_value(int depth) {
        if (depth > kMaxDepth) fail();
        skip_ws();
        if (pos_ >= s_.size()) fail();
        char c = s_[pos_];
        if (c == '{') return parse_object(depth);
        if (c == '[') return parse_array(depth);
        if (c == '"') return Value::make_string(parse_string());
        if (c == 't') { expect_literal("true"); return Value::make_bool(true); }
        if (c == 'f') { expect_literal("false"); return Value::make_bool(false); }
        if (c == 'n') { expect_literal("null"); return Value(); }
        return parse_number();
    }

    Value parse_object(int depth) {
        expect('{');
        Value v = Value::make_object();
        skip_ws();
        if (consume('}')) return v;
        for (;;) {
            skip_ws();
            if (peek() != '"') fail();
            string k = parse_string();
            skip_ws();
            expect(':');
            Value val = parse_value(depth + 1);
            if (v.find(k)) fail();  // Same key --> reject
            v.set(k, std::move(val));
            skip_ws();
            if (consume(',')) continue;
            expect('}');
            return v;
        }
    }

    Value parse_array(int depth) {
        expect('[');
        Value v = Value::make_array();
        skip_ws();
        if (consume(']')) return v;
        for (;;) {
            v.arr.push_back(parse_value(depth + 1));
            skip_ws();
            if (consume(',')) continue;
            expect(']');
            return v;
        }
    }

    Value parse_number() {
        size_t start = pos_;
        if (peek() == '-') ++pos_;
        if (pos_ >= s_.size() || !(s_[pos_] >= '0' && s_[pos_] <= '9')) fail();
        while (pos_ < s_.size() && ((s_[pos_] >= '0' && s_[pos_] <= '9') || s_[pos_] == '.' ||
                                    s_[pos_] == 'e' || s_[pos_] == 'E' || s_[pos_] == '+' || s_[pos_] == '-'))
            ++pos_;
        const string tok = s_.substr(start, pos_ - start);
        char* end = nullptr;
        double d = std::strtod(tok.c_str(), &end);
        if (end != tok.c_str() + tok.size()) fail();
        return Value::make_number(d);
    }

    static void append_utf8(string& out, uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    uint32_t parse_hex4() {
        if (pos_ + 4 > s_.size()) fail();
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
            else fail();
        }
        return v;
    }

    string parse_string() {
        expect('"');
        string out;
        for (;;) {
            if (pos_ >= s_.size()) fail();
            unsigned char c = static_cast<unsigned char>(s_[pos_++]);
            if (c == '"') return out;
            if (c < 0x20) fail();
            // Nếu là ký tự đặc biệt, xử lý
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (pos_ >= s_.size()) fail();
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
                    uint32_t cp = parse_hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos_ + 2 > s_.size() || s_[pos_] != '\\' || s_[pos_ + 1] != 'u') fail();
                        pos_ += 2;
                        uint32_t lo = parse_hex4();
                        if (lo < 0xDC00 || lo > 0xDFFF) fail();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail();
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail();
            }
        }
    }
};

inline Value parse(const string& text) { return Parser(text).parse(); }

}  // namespace minijson