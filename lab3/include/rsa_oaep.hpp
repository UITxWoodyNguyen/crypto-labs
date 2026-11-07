/*
    File: rsa_oaep.hpp
    Usage: RSA-OAEP (SHA-256, MGF1-SHA256) via OpenSSL 3.x EVP API.
*/

#pragma once
#include <string>
#include "utils.hpp"

using namespace std;

namespace rsalab {

inline constexpr size_t kSha256Len = 32;  // hLen

size_t oaep_max_plaintext(size_t modulus_bytes);  // mLen_max = k - 2*hLen - 2. Return 0 if k too small.
size_t oaep_max_plaintext(const EVP_PKEY* key);

// Encrypt directly. Throws UsageError if plaintext > mLen_max (caller should switch to hybrid).
Bytes rsa_oaep_encrypt(EVP_PKEY* pub_key, const Bytes& plaintext, const string& label = "");

// Decrypt. ALL errors (length, padding, label, wrong key) => CryptoError(kDecryptFailedMsg).
Bytes rsa_oaep_decrypt(EVP_PKEY* priv_key, const Bytes& ciphertext, const string& label = "");

} // end namespace rsalab