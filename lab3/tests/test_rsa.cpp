// Unit tests (positive): RSA-OAEP, hybrid, key I/O, encoding, AES-GCM.
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "include/hybrid_engine.hpp"
#include "include/key_manager.hpp"
#include "include/rsa_oaep.hpp"
#include "include/utils.hpp"

using namespace rsalab;
namespace fs = std::filesystem;

namespace {

EVP_PKEY* key3072() {
    static PKeyPtr k = generate_rsa_key(3072);
    return k.get();
}

Bytes pattern(std::size_t n) {
    Bytes b(n);
    for (std::size_t i = 0; i < n; ++i) b[i] = static_cast<std::uint8_t>((i * 131 + 17) & 0xFF);
    return b;
}

}  // namespace

TEST_CASE("OAEP plaintext limit: mLen <= k - 2hLen - 2", "[rsa][limit]") {
    REQUIRE(oaep_max_plaintext(std::size_t(384)) == 318);  // RSA-3072
    REQUIRE(oaep_max_plaintext(std::size_t(512)) == 446);  // RSA-4096
    REQUIRE(oaep_max_plaintext(key3072()) == 318);
}

TEST_CASE("RSA-OAEP round trip, with and without label", "[rsa]") {
    const Bytes msg = pattern(32);

    const Bytes ct = rsa_oaep_encrypt(key3072(), msg);
    REQUIRE(ct.size() == 384);
    REQUIRE(rsa_oaep_decrypt(key3072(), ct) == msg);

    const Bytes ct2 = rsa_oaep_encrypt(key3072(), msg, "ctx");
    REQUIRE(rsa_oaep_decrypt(key3072(), ct2, "ctx") == msg);
}

TEST_CASE("RSA-OAEP is probabilistic", "[rsa]") {
    const Bytes msg = pattern(16);
    REQUIRE(rsa_oaep_encrypt(key3072(), msg) != rsa_oaep_encrypt(key3072(), msg));
}

TEST_CASE("RSA-OAEP boundary: exactly max succeeds, max+1 rejected", "[rsa][limit]") {
    const Bytes at_limit = pattern(318);
    const Bytes ct = rsa_oaep_encrypt(key3072(), at_limit);
    REQUIRE(rsa_oaep_decrypt(key3072(), ct) == at_limit);

    REQUIRE_THROWS_AS(rsa_oaep_encrypt(key3072(), pattern(319)), UsageError);
}

TEST_CASE("RSA-OAEP empty plaintext", "[rsa]") {
    const Bytes ct = rsa_oaep_encrypt(key3072(), Bytes{});
    REQUIRE(rsa_oaep_decrypt(key3072(), ct).empty());
}

TEST_CASE("Hybrid round trip for several sizes", "[hybrid]") {
    for (std::size_t n : {std::size_t(0), std::size_t(1), std::size_t(319), std::size_t(4096), std::size_t(1) << 20}) {
        const Bytes pt = pattern(n);
        const Envelope env = hybrid_encrypt(key3072(), pt, "lbl");
        REQUIRE(env.iv.size() == kGcmIvLen);
        REQUIRE(env.tag.size() == kGcmTagLen);
        REQUIRE(env.wrapped_key.size() == 384);
        REQUIRE(env.ciphertext.size() == n);
        REQUIRE(hybrid_decrypt(key3072(), env, "lbl") == pt);
    }
}

TEST_CASE("Hybrid uses fresh IV and key every time", "[hybrid]") {
    const Bytes pt = pattern(1000);
    const Envelope a = hybrid_encrypt(key3072(), pt);
    const Envelope b = hybrid_encrypt(key3072(), pt);
    REQUIRE(a.iv != b.iv);
    REQUIRE(a.wrapped_key != b.wrapped_key);
    REQUIRE(a.ciphertext != b.ciphertext);
}

TEST_CASE("Envelope JSON round trip", "[hybrid][json]") {
    const Bytes pt = pattern(2000);
    const Envelope env = hybrid_encrypt(key3072(), pt, "nhãn-utf8");
    const std::string json = envelope_to_json(env);

    REQUIRE(looks_like_envelope(to_bytes(json)));
    const Envelope parsed = envelope_from_json(json);
    REQUIRE(parsed.mode == "RSA-OAEP-AES-GCM");
    REQUIRE(parsed.rsa_modulus == 3072);
    REQUIRE(parsed.label == "nhãn-utf8");
    REQUIRE(hybrid_decrypt(key3072(), parsed, "nhãn-utf8") == pt);
}

TEST_CASE("AES-256-GCM matches GCM spec test case 16", "[aes][kat]") {
    const Bytes key = hex_decode("feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308");
    const Bytes iv = hex_decode("cafebabefacedbaddecaf888");
    const Bytes aad = hex_decode("feedfacedeadbeeffeedfacedeadbeefabaddad2");
    const Bytes pt = hex_decode(
        "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657"
        "ba637b39");
    Bytes tag;
    const Bytes ct = aes256gcm_encrypt(key, iv, aad, pt, tag);
    REQUIRE(hex_encode(tag) == "76fc6ece0f4e1768cddf8853bb2d551b");

    Bytes out;
    REQUIRE(aes256gcm_decrypt(key, iv, aad, ct, tag, out));
    REQUIRE(out == pt);
}

TEST_CASE("Key files: PEM + DER + metadata, reload and use", "[keys]") {
    const fs::path dir = fs::temp_directory_path() / "rsalab_test_keys";
    fs::create_directories(dir);
    const std::string pub = (dir / "pub.pem").string();
    const std::string priv = (dir / "priv.pem").string();

    const SavedKeyFiles f = save_keypair(key3072(), pub, priv);
    for (const std::string& p : {f.pub_pem, f.priv_pem, f.pub_der, f.priv_der, f.metadata})
        REQUIRE(fs::exists(p));

    // PEM và DER đều nạp được và tương thích nhau
    const Bytes msg = pattern(40);
    PKeyPtr pub_pem = load_public_key(f.pub_pem);
    PKeyPtr priv_der = load_private_key(f.priv_der);
    PKeyPtr pub_der = load_public_key(f.pub_der);
    PKeyPtr priv_pem = load_private_key(f.priv_pem);
    REQUIRE(rsa_oaep_decrypt(priv_der.get(), rsa_oaep_encrypt(pub_pem.get(), msg)) == msg);
    REQUIRE(rsa_oaep_decrypt(priv_pem.get(), rsa_oaep_encrypt(pub_der.get(), msg)) == msg);

    const std::string meta = to_string(read_file(f.metadata));
    REQUIRE(meta.find("\"modulus_bits\": 3072") != std::string::npos);
    REQUIRE(meta.find("SHA-256") != std::string::npos);

    fs::remove_all(dir);
}

TEST_CASE("Unsupported key sizes are rejected", "[keys]") {
    REQUIRE_THROWS_AS(generate_rsa_key(2048), UsageError);
    REQUIRE_THROWS_AS(generate_rsa_key(1024), UsageError);
}

TEST_CASE("Encoding helpers", "[utils]") {
    const Bytes data = pattern(37);
    REQUIRE(hex_decode(hex_encode(data)) == data);
    REQUIRE(base64_decode(base64_encode(data)) == data);
    REQUIRE(base64_encode(to_bytes("Man")) == "TWFu");
    REQUIRE(base64_encode(to_bytes("Ma")) == "TWE=");
    REQUIRE(base64_encode(to_bytes("M")) == "TQ==");

    REQUIRE_THROWS_AS(hex_decode("abc"), FormatError);       // độ dài lẻ
    REQUIRE_THROWS_AS(hex_decode("zz"), FormatError);        // ký tự sai
    REQUIRE_THROWS_AS(base64_decode("abc"), FormatError);    // độ dài sai
    REQUIRE_THROWS_AS(base64_decode("ab=c"), FormatError);   // padding sai chỗ
    REQUIRE_THROWS_AS(base64_decode("TR=="), FormatError);   // bit dư khác 0 (non-canonical)
    REQUIRE_THROWS_AS(parse_encoding("rot13"), UsageError);
}
