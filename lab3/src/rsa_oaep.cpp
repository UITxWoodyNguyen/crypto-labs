#include "include/rsa_oaep.hpp"
#include <openssl/err.h>
#include <openssl/rsa.h>

namespace rsalab {

std::size_t oaep_max_plaintext(std::size_t k) {
    const std::size_t overhead = 2 * kSha256Len + 2;
    return k > overhead ? k - overhead : 0;
}

std::size_t oaep_max_plaintext(const EVP_PKEY* key) {
    const int k = EVP_PKEY_get_size(key);
    return k > 0 ? oaep_max_plaintext(static_cast<std::size_t>(k)) : 0;
}

// Tạo context với OAEP + SHA-256 + MGF1(SHA-256) + label tuỳ chọn.
static PKeyCtxPtr make_oaep_ctx(EVP_PKEY* key, bool encrypt, const std::string& label) {
    PKeyCtxPtr ctx(EVP_PKEY_CTX_new(key, nullptr));
    if (!ctx) throw CryptoError("RSA context initialisation failed");

    const int init = encrypt ? EVP_PKEY_encrypt_init(ctx.get()) : EVP_PKEY_decrypt_init(ctx.get());
    if (init <= 0) throw CryptoError("RSA context initialisation failed");

    if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), EVP_sha256()) <= 0)
        throw CryptoError("RSA-OAEP parameter setup failed");

    if (!label.empty()) {
        // OpenSSL chiếm quyền sở hữu buffer nếu set0 thành công; nếu thất bại ta phải tự free.
        void* copy = OPENSSL_memdup(label.data(), label.size());
        if (!copy) throw CryptoError("out of memory");
        if (EVP_PKEY_CTX_set0_rsa_oaep_label(ctx.get(), copy, static_cast<int>(label.size())) <= 0) {
            OPENSSL_free(copy);
            throw CryptoError("RSA-OAEP label setup failed");
        }
    }
    return ctx;
}

Bytes rsa_oaep_encrypt(EVP_PKEY* pub_key, const Bytes& plaintext, const std::string& label) {
    if (!pub_key) throw UsageError("null public key");
    if (plaintext.size() > oaep_max_plaintext(pub_key))
        throw UsageError("plaintext exceeds RSA-OAEP limit (k - 2*hLen - 2); use hybrid mode");

    PKeyCtxPtr ctx = make_oaep_ctx(pub_key, /*encrypt=*/true, label);

    std::size_t outlen = 0;
    if (EVP_PKEY_encrypt(ctx.get(), nullptr, &outlen, plaintext.data(), plaintext.size()) <= 0) {
        ERR_clear_error();
        throw CryptoError("RSA-OAEP encryption failed");
    }
    Bytes ct(outlen);
    if (EVP_PKEY_encrypt(ctx.get(), ct.data(), &outlen, plaintext.data(), plaintext.size()) <= 0) {
        ERR_clear_error();
        throw CryptoError("RSA-OAEP encryption failed");
    }
    ct.resize(outlen);
    return ct;
}

Bytes rsa_oaep_decrypt(EVP_PKEY* priv_key, const Bytes& ciphertext, const std::string& label) {
    // Mọi nhánh lỗi dùng chung một thông điệp để không tạo oracle.
    auto fail = []() -> void { ERR_clear_error(); throw CryptoError(kDecryptFailedMsg); };

    if (!priv_key) fail();
    const int k = EVP_PKEY_get_size(priv_key);
    if (k <= 0 || ciphertext.size() != static_cast<std::size_t>(k)) fail();

    PKeyCtxPtr ctx;
    try {
        ctx = make_oaep_ctx(priv_key, /*encrypt=*/false, label);
    } catch (const CryptoError&) {
        fail();
    }

    // Buffer k byte là đủ cho mọi plaintext hợp lệ; không gọi lần "query size" riêng.
    Bytes pt(static_cast<std::size_t>(k));
    std::size_t outlen = pt.size();
    if (EVP_PKEY_decrypt(ctx.get(), pt.data(), &outlen, ciphertext.data(), ciphertext.size()) <= 0) {
        secure_wipe(pt);
        fail();
    }
    pt.resize(outlen);
    return pt;
}

}  // namespace rsalab