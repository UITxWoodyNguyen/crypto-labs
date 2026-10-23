// Minimal strict JSON parser (RFC 8259 subset sufficient for KAT vector files).
// Fails closed: any syntax error, trailing garbage or excessive nesting throws Error(InvalidInput).
#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace aesp {

struct Json {
  enum class Type { Null, Bool, Number, String, Array, Object };
  Type type = Type::Null;
  bool boolean = false;
  double number = 0;
  std::string string;
  std::vector<Json> array;
  std::vector<std::pair<std::string, Json>> object;  // insertion order, duplicates rejected by parser

  bool has(const std::string& key) const;
  const Json* find(const std::string& key) const;
  const Json& at(const std::string& key) const;          // throws if missing
  const std::string& str(const std::string& key) const;  // object member that must be a string
  std::size_t size_value(const std::string& key) const;  // object member: non-negative integer
};

Json parse_json(const std::string& text);

}  // namespace aesp
