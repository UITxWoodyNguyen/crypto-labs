#include "include/kat_runner.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <map>

#include "include/hybrid_engine.hpp"
#include "include/json.hpp"
#include "include/key_manager.hpp"
#include "include/rsa_oaep.hpp"

namespace fs = std::filesystem;

namespace rsalab {

namespace {

const std::string& get_str(const minijson::Value& o, const char* key) {
    const minijson::Value* v = o.find(key);
    if (!v || !v->is_string()) throw FormatError(std::string("missing/invalid field: ") + key);
    return v->str;
}

std::string get_str_or(const minijson::Value& o, const char* key, const std::string& def) {
    const minijson::Value* v = o.find(key);
    if (!v) return def;
    if (!v->is_string()) throw FormatError(std::string("invalid field: ") + key);
    return v->str;
}

int get_int(const minijson::Value& o, const char* key) {
    const minijson::Value* v = o.find(key);
    if (!v || !v->is_number()) throw FormatError(std::string("missing/invalid field: ") + key);
    return static_cast<int>(v->num);
}

// Dữ liệu tổng hợp xác định (chỉ dùng cho test, KHÔNG dùng cho khoá/nonce).
Bytes synthetic(std::size_t n) {
    Bytes b(n);
    for (std::size_t i = 0; i < n; ++i) b[i] = static_cast<std::uint8_t>((i * 31 + 7) & 0xFF);
    return b;
}

// Cache khoá theo số bit để không sinh lại nhiều lần.
EVP_PKEY* cached_key(int bits) {
    static std::map<int, PKeyPtr> cache;
    auto it = cache.find(bits);
    if (it == cache.end()) it = cache.emplace(bits, generate_rsa_key(bits)).first;
    return it->second.get();
}

bool decrypt_fails_closed(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const CryptoError& e) {
        return std::string(e.what()) == kDecryptFailedMsg;
    } catch (...) {
        return false;
    }
    return false;  // giải mã thành công => KHÔNG fail-closed
}

// ---- Các loại test ----

bool kat_aes_gcm(const minijson::Value& t) {
    const Bytes key = hex_decode(get_str(t, "key"));
    const Bytes iv = hex_decode(get_str(t, "iv"));
    const Bytes aad = hex_decode(get_str_or(t, "aad", ""));
    const Bytes pt = hex_decode(get_str_or(t, "pt", ""));
    const Bytes ct = hex_decode(get_str_or(t, "ct", ""));
    const Bytes tag = hex_decode(get_str(t, "tag"));

    Bytes tag_out;
    const Bytes ct_out = aes256gcm_encrypt(key, iv, aad, pt, tag_out);
    if (ct_out != ct || tag_out != tag) return false;

    Bytes pt_out;
    if (!aes256gcm_decrypt(key, iv, aad, ct, tag, pt_out) || pt_out != pt) return false;

    Bytes bad_tag = tag;
    bad_tag[0] ^= 0x01;  // tag sai phải bị từ chối
    if (aes256gcm_decrypt(key, iv, aad, ct, bad_tag, pt_out) || !pt_out.empty()) return false;
    return true;
}

bool kat_rsa_roundtrip(const minijson::Value& t) {
    const int bits = get_int(t, "bits");
    const std::string label = get_str_or(t, "label", "");
    const Bytes pt = hex_decode(get_str(t, "pt"));
    EVP_PKEY* key = cached_key(bits);

    const Bytes ct = rsa_oaep_encrypt(key, pt, label);
    if (ct.size() != static_cast<std::size_t>(EVP_PKEY_get_size(key))) return false;
    if (rsa_oaep_decrypt(key, ct, label) != pt) return false;
    // Nhãn sai phải thất bại
    return decrypt_fails_closed([&] { (void)rsa_oaep_decrypt(key, ct, label + "x"); });
}

bool kat_hybrid_roundtrip(const minijson::Value& t) {
    const int bits = get_int(t, "bits");
    const std::string label = get_str_or(t, "label", "");
    const Bytes pt = synthetic(static_cast<std::size_t>(get_int(t, "size")));
    EVP_PKEY* key = cached_key(bits);

    const Envelope env = hybrid_encrypt(key, pt, label);
    const Envelope parsed = envelope_from_json(envelope_to_json(env));
    if (hybrid_decrypt(key, parsed, label) != pt) return false;

    if (!pt.empty()) {
        Envelope tampered = parsed;
        tampered.ciphertext[0] ^= 0x01;
        if (!decrypt_fails_closed([&] { (void)hybrid_decrypt(key, tampered, label); })) return false;
    }
    Envelope bad_tag = parsed;
    bad_tag.tag[0] ^= 0x01;
    return decrypt_fails_closed([&] { (void)hybrid_decrypt(key, bad_tag, label); });
}

// Vector bên ngoài (vd Wycheproof / CAVS): khoá private PEM + ciphertext => plaintext kỳ vọng.
bool kat_rsa_decrypt(const minijson::Value& t, const fs::path& base_dir) {
    const fs::path key_path = base_dir / get_str(t, "priv");
    PKeyPtr key = load_private_key(key_path.string());
    const Bytes label_bytes = hex_decode(get_str_or(t, "label_hex", ""));
    const Bytes ct = hex_decode(get_str(t, "ct"));
    const Bytes expect = hex_decode(get_str(t, "pt"));
    return rsa_oaep_decrypt(key.get(), ct, to_string(label_bytes)) == expect;
}

}  // namespace

int run_kat(const std::string& vectors_path, bool verbose) {
    const std::string text = to_string(read_file(vectors_path, 64u << 20));
    const minijson::Value root = minijson::parse(text);
    const minijson::Value* tests = root.find("tests");
    if (!root.is_object() || !tests || !tests->is_array()) throw FormatError("KAT file: missing \"tests\" array");

    const fs::path base_dir = fs::path(vectors_path).parent_path();
    int total = 0, passed = 0;

    for (const minijson::Value& t : tests->arr) {
        ++total;
        std::string name = "(unnamed)";
        bool ok = false;
        std::string err;
        try {
            if (!t.is_object()) throw FormatError("test case must be an object");
            name = get_str_or(t, "name", name);
            const std::string type = get_str(t, "type");
            if (type == "aes-256-gcm") ok = kat_aes_gcm(t);
            else if (type == "rsa-oaep-roundtrip") ok = kat_rsa_roundtrip(t);
            else if (type == "hybrid-roundtrip") ok = kat_hybrid_roundtrip(t);
            else if (type == "rsa-oaep-decrypt") ok = kat_rsa_decrypt(t, base_dir);
            else throw FormatError("unknown test type: " + type);
        } catch (const std::exception& e) {
            ok = false;
            err = e.what();
        }
        if (ok) ++passed;
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name;
        if (!ok && verbose && !err.empty()) std::cout << "  (" << err << ")";
        std::cout << "\n";
    }

    std::cout << "----------------------------------------\n"
              << "Total: " << total << "  Passed: " << passed << "  Failed: " << (total - passed) << "\n";
    return (total > 0 && passed == total) ? 0 : 1;
}

}  // namespace rsalab