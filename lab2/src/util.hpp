#pragma once
#include <iostream>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace std;

namespace aesp {
    using Bytes = vector<uint8_t>;
    enum class Encoding { Hex, Base64, Raw };

    void secure_zero(void* p, size_t n) noexcept;

    struct WipeOnExit {
        Bytes& b;
        explicit WipeOnExit(Bytes& buf) : b(buf) {}
        ~WipeOnExit() { secure_zero(b.data(), b.size()); }
    };

    void secure_random(uint8_t* out, size_t n);
    Bytes random_bytes(size_t n);

    string to_hex(const uint8_t* d, size_t n);
    inline string to_hex(const Bytes& b) { return to_hex(b.data(), b.size()); }
    Bytes from_hex(const string& s);
    string to_base64(const uint8_t* d, size_t n);
    Bytes from_base64(const string& s);

    Encoding parse_encoding(const string& s);
    string encode_bytes(const Bytes& b, Encoding e);
    Bytes decode_text(const string& text, Encoding e);

    size_t parse_size(const string& s);

    filesystem::path to_path(const string& utf8);
    bool file_exists(const string& path);
    Bytes read_file(const string& path, size_t max_bytes);
    Bytes read_file_exact(const string& path, size_t expected, const char* what);
    void write_file(const string& path, const uint8_t* d, size_t n, bool overwrite);
}