#include "include/hybrid_engine.hpp"

#include <algorithm>

#include <openssl/err.h>
#include "include/json.hpp"
#include "include/key_manager.hpp"
#include "include/rsa_oaep.hpp"

using namespace std;

namespace rsalab {

namespace {

const size_t kChunk = std::size_t(1) << 30;  // 1 GiB for each EVP call (int limit)
Bytes build_aad(const Envelope& e) {
    std::string s = e.mode + "|" + std::to_string(e.rsa_modulus) + "|" + e.hash + "|" + e.mgf + "|" + e.label;
    return to_bytes(s);
}

}   // end namespace

// AES-256-GCM APIs
Bytes aes256gcm_encrypt(const Bytes& key, const Bytes& iv, const Bytes& aad, const Bytes& pt, Bytes& tag_out) {
    if (key.size() != kAesKeyLen || iv.size() != kGcmIvLen) throw UsageError("invalid AES-GCM key/IV length");

    CipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) throw CryptoError("AES-GCM initialisation failed");
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kGcmIvLen), nullptr) != 1 ||
        EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()) != 1)
        throw CryptoError("AES-GCM initialisation failed");

    int len = 0;
    if (!aad.empty() &&
        EVP_EncryptUpdate(ctx.get(), nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1)
        throw CryptoError("AES-GCM encryption failed");

    Bytes ct(pt.size());
    size_t off = 0;
    while (off < pt.size()) {
        const std::size_t n = std::min(kChunk, pt.size() - off);
        if (EVP_EncryptUpdate(ctx.get(), ct.data() + off, &len, pt.data() + off, static_cast<int>(n)) != 1)
            throw CryptoError("AES-GCM encryption failed");
        off += n;
    }
    if (EVP_EncryptFinal_ex(ctx.get(), ct.data() + off, &len) != 1) throw CryptoError("AES-GCM encryption failed");

    tag_out.assign(kGcmTagLen, 0);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kGcmTagLen), tag_out.data()) != 1)
        throw CryptoError("AES-GCM encryption failed");
    return ct;
}

bool aes256gcm_decrypt(const Bytes& key, const Bytes& iv, const Bytes& aad, const Bytes& ct, const Bytes& tag,
                       Bytes& pt_out) {
    pt_out.clear();
    if (key.size() != kAesKeyLen || iv.size() != kGcmIvLen || tag.size() != kGcmTagLen) return false;

    CipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return false;
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kGcmIvLen), nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()) != 1) {
        ERR_clear_error();
        return false;
    }

    int len = 0;
    if (!aad.empty() &&
        EVP_DecryptUpdate(ctx.get(), nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
        ERR_clear_error();
        return false;
    }

    Bytes out(ct.size());
    std::size_t off = 0;
    while (off < ct.size()) {
        const std::size_t n = std::min(kChunk, ct.size() - off);
        if (EVP_DecryptUpdate(ctx.get(), out.data() + off, &len, ct.data() + off, static_cast<int>(n)) != 1) {
            secure_wipe(out);
            ERR_clear_error();
            return false;
        }
        off += n;
    }

    Bytes tag_copy = tag;  // EVP_CTRL_GCM_SET_TAG receives a non-const pointer
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kGcmTagLen), tag_copy.data()) != 1) {
        secure_wipe(out);
        ERR_clear_error();
        return false;
    }
    // Final check to verify the tag (OpenSSL uses constant-time comparison). Only return the plaintext if successful.
    if (EVP_DecryptFinal_ex(ctx.get(), out.data() + off, &len) != 1) {
        secure_wipe(out);
        ERR_clear_error();
        return false;
    }
    pt_out = std::move(out);
    return true;
}

// Hybrid encryption/decryption APIs
Envelope hybrid_encrypt(EVP_PKEY* pub_key, const Bytes& plaintext, const std::string& label) {
    if (!pub_key) throw UsageError("null public key");

    Envelope env;
    env.rsa_modulus = rsa_key_bits(pub_key);
    env.label = label;

    SecureBytes key(random_bytes(kAesKeyLen));  // Generate a new AES-256 session key for each encryption
    env.iv = random_bytes(kGcmIvLen);           // Generate a unique 96-bit IV for each encryption
    env.wrapped_key = rsa_oaep_encrypt(pub_key, key.get(), label);
    env.ciphertext = aes256gcm_encrypt(key.get(), env.iv, build_aad(env), plaintext, env.tag);
    return env;
}

Bytes hybrid_decrypt(EVP_PKEY* priv_key, const Envelope& env, const std::string& label) {
    auto fail = []() { ERR_clear_error(); throw CryptoError(kDecryptFailedMsg); };

    if (!priv_key) fail();
    // Check the header: any mismatch will return the same generic error.
    if (env.mode != kEnvMode || env.hash != kEnvHash || env.mgf != kEnvMgf) fail();
    if (env.rsa_modulus != rsa_key_bits(priv_key)) fail();
    if (env.label != label) fail();
    if (env.iv.size() != kGcmIvLen || env.tag.size() != kGcmTagLen) fail();

    SecureBytes key;
    try {
        key.get() = rsa_oaep_decrypt(priv_key, env.wrapped_key, label);
    } catch (const CryptoError&) {
        fail();
    }
    if (key.get().size() != kAesKeyLen) fail();

    Bytes pt;
    if (!aes256gcm_decrypt(key.get(), env.iv, build_aad(env), env.ciphertext, env.tag, pt)) fail();
    return pt;
}

// JSON serialization/deserialization for Envelope
std::string envelope_to_json(const Envelope& e) {
    // Precompute the base64 encoding of the ciphertext to reserve enough space in the output string.
    std::string out;
    const std::string ct_b64 = base64_encode(e.ciphertext);
    out.reserve(ct_b64.size() + 1024);
    out += "{\n";
    out += "  \"mode\": " + minijson::quote(e.mode) + ",\n";
    out += "  \"rsa_modulus\": " + std::to_string(e.rsa_modulus) + ",\n";
    out += "  \"hash\": " + minijson::quote(e.hash) + ",\n";
    out += "  \"mgf\": " + minijson::quote(e.mgf) + ",\n";
    out += "  \"label\": " + minijson::quote(e.label) + ",\n";
    out += "  \"wrapped_key\": \"" + base64_encode(e.wrapped_key) + "\",\n";
    out += "  \"iv\": \"" + base64_encode(e.iv) + "\",\n";
    out += "  \"tag\": \"" + base64_encode(e.tag) + "\",\n";
    out += "  \"ciphertext\": \"";
    out += ct_b64;
    out += "\"\n}\n";
    return out;
}

static const std::string& req_string(const minijson::Value& obj, const char* key) {
    const minijson::Value* v = obj.find(key);
    if (!v || !v->is_string()) throw FormatError("invalid envelope header");
    return v->str;
}

Envelope envelope_from_json(const std::string& json) {
    const minijson::Value root = minijson::parse(json);
    if (!root.is_object()) throw FormatError("invalid envelope header");

    Envelope e;
    e.mode = req_string(root, "mode");
    e.hash = req_string(root, "hash");
    e.mgf = req_string(root, "mgf");
    e.label = req_string(root, "label");
    if (e.mode != kEnvMode || e.hash != kEnvHash || e.mgf != kEnvMgf)
        throw FormatError("unsupported envelope parameters");

    const minijson::Value* mod = root.find("rsa_modulus");
    if (!mod || !mod->is_number()) throw FormatError("invalid envelope header");
    const double m = mod->num;
    if (m != static_cast<double>(static_cast<int>(m)) || m < kMinRsaBits || m > kMaxRsaBits)
        throw FormatError("unsupported key size in envelope");
    e.rsa_modulus = static_cast<int>(m);

    e.wrapped_key = base64_decode(req_string(root, "wrapped_key"));
    e.iv = base64_decode(req_string(root, "iv"));
    e.tag = base64_decode(req_string(root, "tag"));
    e.ciphertext = base64_decode(req_string(root, "ciphertext"));

    if (e.iv.size() != kGcmIvLen) throw FormatError("invalid IV length");
    if (e.tag.size() != kGcmTagLen) throw FormatError("invalid tag length");
    if (e.wrapped_key.size() != static_cast<std::size_t>((e.rsa_modulus + 7) / 8))
        throw FormatError("invalid wrapped key length");
    return e;
}

bool looks_like_envelope(const Bytes& data) {
    for (std::uint8_t c : data) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        return c == '{';
    }
    return false;
}

}   // end of namespace rsalab