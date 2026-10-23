#include "json.hpp"

#include <cmath>
#include <cstdlib>

#include "errors.hpp"

using namespace std;

namespace aesp {

bool Json::has(const std::string& key) const { return find(key) != nullptr; }

const Json* Json::find(const std::string& key) const {
  if (type != Type::Object) return nullptr;
  for (const auto& kv : object)
    if (kv.first == key) return &kv.second;
  return nullptr;
}

const Json& Json::at(const std::string& key) const {
  const Json* j = find(key);
  if (!j) throw Error(ExitCode::InvalidInput, "JSON: missing field '" + key + "'");
  return *j;
}

const std::string& Json::str(const std::string& key) const {
  const Json& j = at(key);
  if (j.type != Type::String) throw Error(ExitCode::InvalidInput, "JSON: field '" + key + "' must be a string");
  return j.string;
}

std::size_t Json::size_value(const std::string& key) const {
  const Json& j = at(key);
  if (j.type != Type::Number || j.number < 0 || j.number != std::floor(j.number) || j.number > 1e12)
    throw Error(ExitCode::InvalidInput, "JSON: field '" + key + "' must be a non-negative integer");
  return static_cast<std::size_t>(j.number);
}

namespace {

class Parser {
 public:
  explicit Parser(const std::string& s) : s_(s) {}

  Json parse() {
    Json v = value(0);
    ws();
    if (pos_ != s_.size()) fail("trailing characters after JSON value");
    return v;
  }

 private:
  [[noreturn]] void fail(const std::string& m) const {
    throw Error(ExitCode::InvalidInput, "JSON parse error at offset " + std::to_string(pos_) + ": " + m);
  }
  void ws() {
    while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
  }
  char peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }
  void expect(char c) {
    if (peek() != c) fail(std::string("expected '") + c + "'");
    ++pos_;
  }
  bool literal(const char* lit) {
    std::size_t n = 0;
    while (lit[n]) ++n;
    if (s_.compare(pos_, n, lit) == 0) {
      pos_ += n;
      return true;
    }
    return false;
  }

  Json value(int depth) {
    if (depth > 64) fail("nesting too deep");
    ws();
    Json j;
    const char c = peek();
    if (c == '{') {
      ++pos_;
      j.type = Json::Type::Object;
      ws();
      if (peek() == '}') { ++pos_; return j; }
      for (;;) {
        ws();
        if (peek() != '"') fail("object key must be a string");
        std::string k = string_lit();
        for (const auto& kv : j.object)
          if (kv.first == k) fail("duplicate key '" + k + "'");
        ws();
        expect(':');
        j.object.emplace_back(std::move(k), value(depth + 1));
        ws();
        if (peek() == ',') { ++pos_; continue; }
        expect('}');
        return j;
      }
    }
    if (c == '[') {
      ++pos_;
      j.type = Json::Type::Array;
      ws();
      if (peek() == ']') { ++pos_; return j; }
      for (;;) {
        j.array.push_back(value(depth + 1));
        ws();
        if (peek() == ',') { ++pos_; continue; }
        expect(']');
        return j;
      }
    }
    if (c == '"') {
      j.type = Json::Type::String;
      j.string = string_lit();
      return j;
    }
    if (literal("true")) { j.type = Json::Type::Bool; j.boolean = true; return j; }
    if (literal("false")) { j.type = Json::Type::Bool; j.boolean = false; return j; }
    if (literal("null")) return j;
    if (c == '-' || (c >= '0' && c <= '9')) {
      const std::size_t start = pos_;
      if (peek() == '-') ++pos_;
      if (!(peek() >= '0' && peek() <= '9')) fail("invalid number");
      if (peek() == '0') {
        ++pos_;
        if (peek() >= '0' && peek() <= '9') fail("invalid number (leading zero)");
      } else {
        while (peek() >= '0' && peek() <= '9') ++pos_;
      }
      if (peek() == '.') {
        ++pos_;
        if (!(peek() >= '0' && peek() <= '9')) fail("invalid number");
        while (peek() >= '0' && peek() <= '9') ++pos_;
      }
      if (peek() == 'e' || peek() == 'E') {
        ++pos_;
        if (peek() == '+' || peek() == '-') ++pos_;
        if (!(peek() >= '0' && peek() <= '9')) fail("invalid number");
        while (peek() >= '0' && peek() <= '9') ++pos_;
      }
      j.type = Json::Type::Number;
      j.number = std::strtod(s_.substr(start, pos_ - start).c_str(), nullptr);
      return j;
    }
    fail("unexpected character");
  }

  static void append_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  unsigned hex4() {
    if (pos_ + 4 > s_.size()) fail("truncated \\u escape");
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = s_[pos_++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
      else fail("invalid \\u escape");
    }
    return v;
  }

  std::string string_lit() {
    expect('"');
    std::string out;
    for (;;) {
      if (pos_ >= s_.size()) fail("unterminated string");
      const unsigned char c = static_cast<unsigned char>(s_[pos_++]);
      if (c == '"') return out;
      if (c < 0x20) fail("control character in string");
      if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
      if (pos_ >= s_.size()) fail("unterminated escape");
      const char e = s_[pos_++];
      switch (e) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          unsigned cp = hex4();
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (s_.compare(pos_, 2, "\\u") != 0) fail("lone high surrogate");
            pos_ += 2;
            const unsigned lo = hex4();
            if (lo < 0xDC00 || lo > 0xDFFF) fail("invalid surrogate pair");
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            fail("lone low surrogate");
          }
          append_utf8(out, cp);
          break;
        }
        default: fail("invalid escape");
      }
    }
  }

  const std::string& s_;
  std::size_t pos_ = 0;
};

}  // namespace

Json parse_json(const std::string& text) {
  std::size_t start = 0;
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
    start = 3;  // tolerate a UTF-8 BOM (common on Windows)
  const std::string body = text.substr(start);
  return Parser(body).parse();
}

}  // namespace aesp
