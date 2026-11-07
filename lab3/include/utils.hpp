/*
    File: utils.hpp
    Usage: Common data types, exceptions, RAII for OpenSSL, encode/decode, file I/O
*/

#pragma once

#include <iostream>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// OpenSSL header includes
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>

using namespace std;

namespace rsalab {

using Bytes = vector<uint8_t>;

// EXCEPTIONS (each type corresponds to an exit code in main)
class UsageError : public runtime_error {
    public: 
        using runtime_error::runtime_error;
};

class IoError : public runtime_error {
    public: 
        using runtime_error::runtime_error;
};

class FormatError : public runtime_error {
    public: 
        using runtime_error::runtime_error;
};

class CryptoError : public runtime_error {
    public: 
        using runtime_error::runtime_error;
};

inline constexpr const char* kDecryptFailedMsg = "decryption failed";

// RAII for OpenSSL
struct PKeyDeleter {
    void operator()(EVP_PKEY* p) const noexcept {
        EVP_PKEY_free(p);
    }
};

struct PKeyCtxDeleter {
    void operator()(EVP_PKEY_CTX* p) const noexcept {
        EVP_PKEY_CTX_free(p);
    }
};

struct CipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX* p) const noexcept {
        EVP_CIPHER_CTX_free(p);
    }
};

struct BioDeleter {
    void operator()(BIO* p) const noexcept {
        BIO_free_all(p);
    }
};

using PKeyPtr = unique_ptr<EVP_PKEY, PKeyDeleter>;
using PKeyCtxPtr = unique_ptr<EVP_PKEY_CTX, PKeyCtxDeleter>;
using CipherCtxPtr = unique_ptr<EVP_CIPHER_CTX, CipherCtxDeleter>;
using BioPtr = unique_ptr<BIO, BioDeleter>;

// Secure buffer: automatically wiped (OPENSSL_cleanse) on destruction
class SecureBytes {
    private:
        Bytes data_;

    public:
        SecureBytes() = default;
        explicit SecureBytes(size_t n) : data_(n) {}
        explicit SecureBytes(Bytes b) : data_(move(b)) {}
        SecureBytes(const SecureBytes&) = delete;
        SecureBytes& operator=(const SecureBytes&) = delete;
        SecureBytes(SecureBytes&&) noexcept = default;
        SecureBytes& operator=(SecureBytes&&) = delete;
        ~SecureBytes() { wipe(); }

        Bytes& get() { return data_; }
        const Bytes& get() const { return data_; }
        void wipe() noexcept { if (!data_.empty()) OPENSSL_cleanse(data_.data(), data_.size()); }
};

void secure_wipe(Bytes& b) noexcept;

// Encoding APIs
enum class Encoding { Base64, Hex, Raw };

Encoding parse_encoding(const string& name); // "raw" | "hex" | "base64", invalid => UsageError
string hex_encode(const Bytes& data);
Bytes hex_decode(const string& text); // invalid => FormatError
string base64_encode(const Bytes& data);
Bytes base64_decode(const string& text); // invalid => FormatError
string strip_ascii_whitespace(const string& s);

Bytes to_bytes(const string& s);
string to_string(const Bytes& b);

// File I/O APIs
Bytes read_file(const string& path, size_t max_bytes = numeric_limits<size_t>::max());
void write_file(const string& path, const Bytes& data);
void write_file(const string& path, const string& text);

// Other utility functions
Bytes random_bytes(size_t n);
string utc_timestamp_iso8601(); // e.g., 2026-11-03T06:51:00Z

}   // end namespace rsalab