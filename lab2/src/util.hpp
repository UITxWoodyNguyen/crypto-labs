// Small utilities: secure wipe, OS random, hex/base64 codecs, strict file I/O.
// Nothing here is cryptographic apart from the OS RNG wrapper.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace std;

namespace aesp {

using Bytes = std::vector<uint8_t>;
enum class Encoding { Hex, Base64, Raw };

// Wipes memory in a way the optimiser may not remove.
void secure_zero(void* p, std::size_t n) noexcept;

// RAII wiper for key material held in a Bytes buffer.
struct WipeOnExit {
  Bytes& b;
  explicit WipeOnExit(Bytes& buf) : b(buf) {}
  ~WipeOnExit() { secure_zero(b.data(), b.size()); }
};

// OS CSPRNG: BCryptGenRandom (Windows), getrandom(2) (Linux), arc4random_buf (BSD/macOS).
// Never rand()/std::mt19937. Throws Error(Failure) if the OS RNG is unavailable.
void secure_random(uint8_t* out, std::size_t n);
Bytes random_bytes(std::size_t n);

std::string to_hex(const uint8_t* d, std::size_t n);
inline std::string to_hex(const Bytes& b) { return to_hex(b.data(), b.size()); }
Bytes from_hex(const std::string& s);  // strict: even length, [0-9a-fA-F] only
std::string to_base64(const uint8_t* d, std::size_t n);
Bytes from_base64(const std::string& s);  // strict RFC 4648 with padding

Encoding parse_encoding(const std::string& s);               // hex | base64 | raw
std::string encode_bytes(const Bytes& b, Encoding e);        // Raw => bytes copied into a string
Bytes decode_text(const std::string& text, Encoding e);      // hex/base64: ASCII whitespace ignored

// "1048576", "64KiB", "1MiB", "1GiB" (binary prefixes only).
std::size_t parse_size(const std::string& s);

// UTF-8 path -> std::filesystem::path (works for non-ASCII names on Windows).
std::filesystem::path to_path(const std::string& utf8);
bool file_exists(const std::string& path);
Bytes read_file(const std::string& path, std::size_t max_bytes);
// Reads a file that must contain exactly `expected` bytes (keys, IVs). `what` is used in errors.
Bytes read_file_exact(const std::string& path, std::size_t expected, const char* what);
// Writes via "<path>.tmp" + rename, so a failed write never leaves a truncated output.
void write_file(const std::string& path, const uint8_t* d, std::size_t n, bool overwrite);

}  // namespace aesp
