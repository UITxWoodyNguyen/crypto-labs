/*
    File: hybrid_engine.hpp
    Usage: Envelope encryption: AES-256-GCM (data) + RSA-OAEP (key wrapping)
*/

#pragma once

#include <string>
#include "utils.hpp"

using namespace std;

namespace rsalab {

inline constexpr size_t kAesKeyLen = 32; // 256-bit
inline constexpr size_t kGcmIvLen = 12;  // 96-bit
inline constexpr size_t kGcmTagLen = 16; // 128-bit

inline constexpr const char* kEnvMode = "RSA-OAEP-AES-GCM";
inline constexpr const char* kEnvHash = "SHA-256";
inline constexpr const char* kEnvMgf = "MGF1-SHA256";

// Envelope structure
struct Envelope {
    string mode = kEnvMode;
    int rsa_modulus = 0;
    string hash = kEnvHash;
    string mgf = kEnvMgf;
    string label;
    Bytes wrapped_key;  // RSA-OAEP(AES key), k byte
    Bytes iv;           // 12 byte
    Bytes tag;          // 16 byte
    Bytes ciphertext;   // AES-GCM ciphertext (same length as plaintext)
};

// AES 256-GCM encryption (used for hybrid, KAT, bench)
Bytes aes256gcm_encrypt (const Bytes& key, const Bytes& iv, const Bytes& aad, const Bytes& plaintext, Bytes& tag_out);

// Returns false if authentication fails; in that case, plaintext_out is wiped and empty.
bool aes256gcm_decrypt (const Bytes& key, const Bytes& iv, const Bytes& aad, const Bytes& ciphertext, const Bytes& tag, Bytes& plaintext_out);

// Hybrid encryption
Envelope hybrid_encrypt(EVP_PKEY* pub_key, const Bytes& plaintext, const string& label = "");

// Errors => CryptoError(kDecryptFailedMsg). Does not return data if tag is incorrect.
Bytes hybrid_decrypt(EVP_PKEY* priv_key, const Envelope& env, const string& label = "");

// (De)serialisation JSON
string envelope_to_json(const Envelope& env);
Envelope envelope_from_json(const string& json); // strict; errors => FormatError
bool looks_like_envelope(const Bytes& data); // first non-whitespace byte is '{'

}   // end namespace rsalab