#include "include/key_manager.hpp"
#include <filesystem>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "include/json.hpp"

namespace fs = std::filesystem;

namespace rsalab {

int rsa_key_bits(const EVP_PKEY* key) { return EVP_PKEY_get_bits(key); }

PKeyPtr generate_rsa_key(int bits) {
    if (bits != 3072 && bits != 4096) throw UsageError("unsupported key size (supported: 3072, 4096)");
    PKeyPtr key(EVP_RSA_gen(static_cast<unsigned int>(bits)));  // e = 65537
    if (!key) {
        ERR_clear_error();
        throw CryptoError("RSA key generation failed");
    }
    return key;
}

static Bytes bio_to_bytes(BIO* bio) {
    char* p = nullptr;
    const long n = BIO_get_mem_data(bio, &p);
    if (n < 0 || !p) throw CryptoError("key serialisation failed");
    return Bytes(reinterpret_cast<std::uint8_t*>(p), reinterpret_cast<std::uint8_t*>(p) + n);
}

static std::string der_path_for(const std::string& path) {
    fs::path p(path);
    if (p.extension() == ".der") throw UsageError("PEM output path must not use the .der extension");
    p.replace_extension(".der");
    return p.string();
}

static void restrict_permissions(const std::string& path) {
    try {
        fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
    } catch (...) {
        // Trên một số FS (Windows/FAT) không hỗ trợ; bỏ qua.
    }
}

SavedKeyFiles save_keypair(EVP_PKEY* key, const std::string& pub_path, const std::string& priv_path) {
    if (!key) throw UsageError("null key");
    SavedKeyFiles out;
    out.pub_pem = pub_path;
    out.priv_pem = priv_path;
    out.pub_der = der_path_for(pub_path);
    out.priv_der = der_path_for(priv_path);

    // Public: SubjectPublicKeyInfo
    {
        BioPtr bio(BIO_new(BIO_s_mem()));
        if (!bio || PEM_write_bio_PUBKEY(bio.get(), key) != 1) throw CryptoError("key serialisation failed");
        write_file(out.pub_pem, bio_to_bytes(bio.get()));
    }
    {
        BioPtr bio(BIO_new(BIO_s_mem()));
        if (!bio || i2d_PUBKEY_bio(bio.get(), key) != 1) throw CryptoError("key serialisation failed");
        write_file(out.pub_der, bio_to_bytes(bio.get()));
    }
    // Private: PKCS#8 (không mã hoá bằng passphrase; bảo vệ bằng quyền file)
    {
        BioPtr bio(BIO_new(BIO_s_mem()));
        if (!bio || PEM_write_bio_PKCS8PrivateKey(bio.get(), key, nullptr, nullptr, 0, nullptr, nullptr) != 1)
            throw CryptoError("key serialisation failed");
        Bytes data = bio_to_bytes(bio.get());
        write_file(out.priv_pem, data);
        secure_wipe(data);
        restrict_permissions(out.priv_pem);
    }
    {
        BioPtr bio(BIO_new(BIO_s_mem()));
        if (!bio || i2d_PKCS8PrivateKey_bio(bio.get(), key, nullptr, nullptr, 0, nullptr, nullptr) != 1)
            throw CryptoError("key serialisation failed");
        Bytes data = bio_to_bytes(bio.get());
        write_file(out.priv_der, data);
        secure_wipe(data);
        restrict_permissions(out.priv_der);
    }

    // Metadata JSON, đặt cạnh file private
    minijson::Value meta = minijson::Value::make_object();
    meta.set("creation_time", minijson::Value::make_string(utc_timestamp_iso8601()));
    meta.set("modulus_bits", minijson::Value::make_number(rsa_key_bits(key)));
    meta.set("hash", minijson::Value::make_string("SHA-256"));
    meta.set("mgf", minijson::Value::make_string("MGF1-SHA256"));

    fs::path meta_path = fs::path(priv_path).parent_path() / "keys_metadata.json";
    out.metadata = meta_path.string();
    write_file(out.metadata, minijson::dump(meta) + "\n");
    return out;
}

static void validate_rsa_key(const EVP_PKEY* key) {
    if (!EVP_PKEY_is_a(key, "RSA")) throw FormatError("unsupported key type (RSA required)");
    const int bits = EVP_PKEY_get_bits(key);
    if (bits < kMinRsaBits) throw FormatError("unsupported key size (>= 3072 bits required)");
    if (bits > kMaxRsaBits) throw FormatError("unsupported key size (too large)");
}

static bool looks_like_pem(const Bytes& d) {
    static const char marker[] = "-----BEGIN";
    std::size_t i = 0;
    while (i < d.size() && (d[i] == ' ' || d[i] == '\t' || d[i] == '\r' || d[i] == '\n')) ++i;
    if (d.size() - i < sizeof(marker) - 1) return false;
    for (std::size_t j = 0; j < sizeof(marker) - 1; ++j)
        if (d[i + j] != static_cast<std::uint8_t>(marker[j])) return false;
    return true;
}

static int no_passphrase_cb(char*, int, int, void*) { return 0; }  // không bao giờ hỏi passphrase

PKeyPtr load_public_key(const std::string& path) {
    Bytes d = read_file(path, 1u << 20);
    if (d.empty()) throw FormatError("cannot parse public key file");
    BioPtr bio(BIO_new_mem_buf(d.data(), static_cast<int>(d.size())));
    if (!bio) throw CryptoError("out of memory");

    PKeyPtr key;
    if (looks_like_pem(d)) key.reset(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
    else key.reset(d2i_PUBKEY_bio(bio.get(), nullptr));
    ERR_clear_error();
    if (!key) throw FormatError("cannot parse public key file");
    validate_rsa_key(key.get());
    return key;
}

PKeyPtr load_private_key(const std::string& path) {
    Bytes d = read_file(path, 1u << 20);
    if (d.empty()) throw FormatError("cannot parse private key file");
    BioPtr bio(BIO_new_mem_buf(d.data(), static_cast<int>(d.size())));
    if (!bio) throw CryptoError("out of memory");

    PKeyPtr key;
    if (looks_like_pem(d)) key.reset(PEM_read_bio_PrivateKey(bio.get(), nullptr, no_passphrase_cb, nullptr));
    else key.reset(d2i_PrivateKey_bio(bio.get(), nullptr));
    ERR_clear_error();
    secure_wipe(d);
    if (!key) throw FormatError("cannot parse private key file");
    validate_rsa_key(key.get());
    return key;
}

}  // namespace rsalab