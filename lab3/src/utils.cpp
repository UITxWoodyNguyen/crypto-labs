#include "include/utils.hpp"

#include <iostream>
#include <array>
#include <ctime>
#include <fstream>

#include <openssl/rand.h>

using namespace std;

namespace rsalab {

void secure_wipe (Bytes& b) noexcept {
    if (!b.empty())
        OPENSSL_cleanse(b.data(), b.size());
}

Encoding parse_encoding(const string& name) {
    if (name == "raw") return Encoding::Raw;
    if (name == "hex") return Encoding::Hex;
    if (name == "base64") return Encoding::Base64;
    throw UsageError("unsupported encoding (use hex|base64|raw)");
}

string hex_encode(const Bytes& data) {
    static const char* digits = "0123456789abcdef";
    string out;
    out.reserve(data.size() * 2);
    for (auto b : data) {
        out += digits[b >> 4];
        out += digits[b & 0x0F];
    }
    return out;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

Bytes hex_decode (const string& text) {
    if (text.size() % 2 != 0) throw FormatError("invalid hex length");
    Bytes out;
    out.reserve(text.size() / 2);
    for (size_t i = 0; i < text.size(); i += 2) {
        int hi = hex_val(text[i]);
        int lo = hex_val(text[i + 1]);
        if (hi < 0 || lo < 0) throw FormatError("invalid hex character");
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

string base64_encode(const Bytes& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string out;
    out.reserve((data.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += tbl[n & 63];
    }
    size_t rem = data.size() - i;
    if (rem == 1) {
        uint32_t n = uint32_t(data[i]) << 16;
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += "==";
    } else if (rem == 2) {
        uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

static array<int, 256> make_b64_table() {
    array<int, 256> t{};
    t.fill(-1);
    const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) t[static_cast<unsigned char>(tbl[i])] = i;
    return t;
}

static const array<int, 256> b64_table = make_b64_table();

Bytes base64_decode(const string& in) {
    if (in.size() % 4 != 0) throw FormatError("invalid base64 length");
    Bytes out;
    out.reserve(in.size() / 4 * 3);
    for (size_t i = 0; i < in.size(); i += 4) {
        const bool last = (i + 4 == in.size());
        int v[4] = {0, 0, 0, 0};
        int pad = 0;
        for (int j = 0; j < 4; ++j) {
            const char c = in[i + static_cast<size_t>(j)];
            if (c == '=') {
                if (!last || j < 2) throw FormatError("invalid base64 padding");
                ++pad;
            } else {
                if (pad > 0) throw FormatError("invalid base64 padding");
                int d = b64_table[static_cast<unsigned char>(c)];
                if (d < 0) throw FormatError("invalid base64 character");
                v[j] = d;
            }
        }
        const uint32_t n = (uint32_t(v[0]) << 18) | (uint32_t(v[1]) << 12) |
                           (uint32_t(v[2]) << 6) | uint32_t(v[3]);
        out.push_back(static_cast<uint8_t>(n >> 16));
        if (pad < 2) out.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
        if (pad < 1) out.push_back(static_cast<uint8_t>(n & 0xFF));
        // Bit dư phải bằng 0 (canonical) để chống biến thể mã hoá không chuẩn
        if (pad == 2 && (v[1] & 0x0F)) throw FormatError("non-canonical base64");
        if (pad == 1 && (v[2] & 0x03)) throw FormatError("non-canonical base64");
    }
    return out;
}

string strip_ascii_whitespace(const string& s) {
    string out;
    out.reserve(s.size());
    for (char c : s)
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') out += c;
    return out;
}

Bytes to_bytes(const string& s) { return Bytes(s.begin(), s.end()); }
string to_string(const Bytes& b) { return string(b.begin(), b.end()); }

Bytes read_file (const string& path, size_t max_bytes) {
    ifstream f(path, ios::binary | ios::ate);
    if (!f) throw IoError("cannot open input file: " + path);
    const streamoff size = f.tellg();
    if (size < 0) throw IoError("cannot determine file size: " + path);
    if (static_cast<uintmax_t>(size) > max_bytes) throw IoError("input file too large: " + path);
    f.seekg(0, ios::beg);
    Bytes data(static_cast<size_t>(size));
    if (size > 0) {
        f.read(reinterpret_cast<char*>(data.data()), size);
        if (!f) throw IoError("read error: " + path);
    }
    return data;
}

void write_file(const string& path, const Bytes& data) {
    ofstream f(path, ios::binary | ios::trunc);
    if (!f) throw IoError("cannot open output file: " + path);
    if (!data.empty()) f.write(reinterpret_cast<const char*>(data.data()), static_cast<streamsize>(data.size()));
    f.flush();
    if (!f) throw IoError("write error: " + path);
}

void write_file(const string& path, const string& text) {
    write_file(path, to_bytes(text));
}

Bytes random_bytes(size_t n) {
    Bytes out(n);
    if (n == 0) return out;
    if (n > static_cast<size_t>(numeric_limits<int>::max()))
        throw CryptoError("random request too large");
    if (RAND_bytes(out.data(), static_cast<int>(n)) != 1) throw CryptoError("secure random generator failure");
    return out;
}

string utc_timestamp_iso8601() {
    time_t t = time(nullptr);
    tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

} // end namespace rsalab