/*
    File: key_manager.hpp
    Usage: RSA key generation, PEM + DER export/import, metadata JSON.
*/

#pragma once

#include <string>
#include "utils.hpp"

using namespace std;

namespace rsalab {

inline constexpr int kMinRsaBits = 3072;
inline constexpr int kMaxRsaBits = 16384;

struct SavedKeyFiles {
    string pub_pem;
    string priv_pem;
    string pub_der;
    string priv_der;
    string metadata;
};

// Only accept 3072 or 4096 (else UsageError).
PKeyPtr generate_rsa_key(int bits);

// Write PEM (SPKI / PKCS#8) + DER in parallel (change extension to .der) + keys_metadata.json
// (in the same directory as the private file). Private file is chmod 0600 when
SavedKeyFiles save_keypair(EVP_PKEY* key, const string& pub_path, const string& priv_path);

// Auto-detect PEM or DER. Reject non-RSA keys or < 3072 bits (FormatError).
PKeyPtr load_public_key(const string& path);
PKeyPtr load_private_key(const string& path);

// Return the number of bits in the RSA key.
int rsa_key_bits(const EVP_PKEY* key);

} // end namespace rsalab