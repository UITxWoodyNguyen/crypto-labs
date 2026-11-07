// Negative tests: mọi trường hợp phải fail-closed với thông điệp lỗi chung, không rò rỉ nguyên nhân.
#include <filesystem>
#include <functional>

#include <catch2/catch_test_macros.hpp>

#include "include/hybrid_engine.hpp"
#include "include/key_manager.hpp"
#include "include/rsa_oaep.hpp"
#include "include/utils.hpp"

using namespace rsalab;

namespace {

EVP_PKEY* keyA() { static PKeyPtr k = generate_rsa_key(3072); return k.get(); }
EVP_PKEY* keyB() { static PKeyPtr k = generate_rsa_key(3072); return k.get(); }

Bytes pattern(std::size_t n) {
    Bytes b(n);
    for (std::size_t i = 0; i < n; ++i) b[i] = static_cast<std::uint8_t>((i * 29 + 3) & 0xFF);
    return b;
}

// Yêu cầu: ném CryptoError với đúng thông điệp chung.
void require_generic_failure(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const CryptoError& e) {
        REQUIRE(std::string(e.what()) == kDecryptFailedMsg);
        return;
    }
    FAIL("expected CryptoError(decryption failed)");
}

}  // namespace

TEST_CASE("1. Altered RSA ciphertext (single bit) fails", "[negative][rsa]") {
    const Bytes ct = rsa_oaep_encrypt(keyA(), pattern(32));
    for (std::size_t pos : {std::size_t(0), ct.size() / 2, ct.size() - 1}) {
        Bytes bad = ct;
        bad[pos] ^= 0x01;
        require_generic_failure([&] { (void)rsa_oaep_decrypt(keyA(), bad); });
    }
}

TEST_CASE("1b. Truncated / extended / empty RSA ciphertext fails", "[negative][rsa]") {
    Bytes ct = rsa_oaep_encrypt(keyA(), pattern(32));
    Bytes shorter(ct.begin(), ct.end() - 1);
    Bytes longer = ct;
    longer.push_back(0);
    require_generic_failure([&] { (void)rsa_oaep_decrypt(keyA(), shorter); });
    require_generic_failure([&] { (void)rsa_oaep_decrypt(keyA(), longer); });
    require_generic_failure([&] { (void)rsa_oaep_decrypt(keyA(), Bytes{}); });
}

TEST_CASE("2. Altered AES-GCM ciphertext fails tag verification, no plaintext returned", "[negative][hybrid]") {
    const Envelope env = hybrid_encrypt(keyA(), pattern(5000));
    for (std::size_t pos : {std::size_t(0), env.ciphertext.size() / 2, env.ciphertext.size() - 1}) {
        Envelope bad = env;
        bad.ciphertext[pos] ^= 0x01;
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), bad); });
    }
}

TEST_CASE("2b. Truncated AES-GCM ciphertext fails", "[negative][hybrid]") {
    Envelope bad = hybrid_encrypt(keyA(), pattern(5000));
    bad.ciphertext.pop_back();
    require_generic_failure([&] { (void)hybrid_decrypt(keyA(), bad); });
}

TEST_CASE("3. Wrong private key fails (direct and hybrid)", "[negative][key]") {
    const Bytes ct = rsa_oaep_encrypt(keyA(), pattern(32));
    require_generic_failure([&] { (void)rsa_oaep_decrypt(keyB(), ct); });

    const Envelope env = hybrid_encrypt(keyA(), pattern(2000));
    require_generic_failure([&] { (void)hybrid_decrypt(keyB(), env); });
}

TEST_CASE("4. Wrong or missing OAEP label fails", "[negative][label]") {
    const Bytes msg = pattern(32);
    const Bytes ct = rsa_oaep_encrypt(keyA(), msg, "SECURE_CONTEXT");
    require_generic_failure([&] { (void)rsa_oaep_decrypt(keyA(), ct, "WRONG_CONTEXT"); });
    require_generic_failure([&] { (void)rsa_oaep_decrypt(keyA(), ct, ""); });
    REQUIRE(rsa_oaep_decrypt(keyA(), ct, "SECURE_CONTEXT") == msg);  // đối chứng

    const Envelope env = hybrid_encrypt(keyA(), pattern(1000), "SECURE_CONTEXT");
    require_generic_failure([&] { (void)hybrid_decrypt(keyA(), env, "WRONG_CONTEXT"); });
    require_generic_failure([&] { (void)hybrid_decrypt(keyA(), env, ""); });
}

TEST_CASE("5. Tampered envelope header fails closed", "[negative][envelope]") {
    const Envelope env = hybrid_encrypt(keyA(), pattern(1000), "lbl");

    SECTION("iv modified") {
        Envelope e = env; e.iv[0] ^= 0x01;
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e, "lbl"); });
    }
    SECTION("tag modified") {
        Envelope e = env; e.tag[15] ^= 0x80;
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e, "lbl"); });
    }
    SECTION("wrapped_key modified") {
        Envelope e = env; e.wrapped_key[100] ^= 0x01;
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e, "lbl"); });
    }
    SECTION("rsa_modulus modified (bound via AAD / key check)") {
        Envelope e = env; e.rsa_modulus = 4096;
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e, "lbl"); });
    }
    SECTION("mode / hash / mgf modified") {
        Envelope e1 = env; e1.mode = "RSA-OAEP-AES-CBC";
        Envelope e2 = env; e2.hash = "SHA-1";
        Envelope e3 = env; e3.mgf = "MGF1-SHA1";
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e1, "lbl"); });
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e2, "lbl"); });
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e3, "lbl"); });
    }
    SECTION("label field inside header modified") {
        Envelope e = env; e.label = "other";
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e, "lbl"); });
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e, "other"); });
    }
    SECTION("wrong iv / tag sizes") {
        Envelope e1 = env; e1.iv.push_back(0);
        Envelope e2 = env; e2.tag.pop_back();
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e1, "lbl"); });
        require_generic_failure([&] { (void)hybrid_decrypt(keyA(), e2, "lbl"); });
    }
}

TEST_CASE("6. Malformed envelope JSON is rejected at parse time", "[negative][json]") {
    const std::string good = envelope_to_json(hybrid_encrypt(keyA(), pattern(500)));
    REQUIRE_NOTHROW(envelope_from_json(good));

    auto replace_first = [](std::string s, const std::string& from, const std::string& to) {
        const auto p = s.find(from);
        REQUIRE(p != std::string::npos);
        s.replace(p, from.size(), to);
        return s;
    };

    REQUIRE_THROWS_AS(envelope_from_json(""), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json("{"), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json("[]"), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json(good.substr(0, good.size() / 2)), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json(good + "garbage"), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json(replace_first(good, "\"iv\": \"", "\"iv\": \"!!")), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json(replace_first(good, "RSA-OAEP-AES-GCM", "XYZ")), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json(replace_first(good, "\"hash\": \"SHA-256\"", "\"hash\": \"MD5\"")), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json(replace_first(good, "\"rsa_modulus\": 3072", "\"rsa_modulus\": 1024")), FormatError);
    REQUIRE_THROWS_AS(envelope_from_json(replace_first(good, "\"mgf\"", "\"mgf_x\"")), FormatError);  // thiếu trường
}

TEST_CASE("7. Malformed encodings are rejected", "[negative][encoding]") {
    REQUIRE_THROWS_AS(hex_decode("0g"), FormatError);
    REQUIRE_THROWS_AS(hex_decode("123"), FormatError);
    REQUIRE_THROWS_AS(base64_decode("@@@@"), FormatError);
    REQUIRE_THROWS_AS(base64_decode("QQ="), FormatError);
    REQUIRE_THROWS_AS(base64_decode("QQ==QQ=="), FormatError);  // padding giữa chuỗi
}

TEST_CASE("8. Corrupted / wrong-type key files are rejected", "[negative][keys]") {
    const std::string dir = (std::filesystem::temp_directory_path() / "rsalab_neg_keys").string();
    std::filesystem::create_directories(dir);
    const std::string pub = dir + "/pub.pem", priv = dir + "/priv.pem";
    save_keypair(keyA(), pub, priv);

    // PEM bị cắt cụt
    Bytes pem = read_file(pub);
    pem.resize(pem.size() / 2);
    write_file(dir + "/trunc.pem", pem);
    REQUIRE_THROWS_AS(load_public_key(dir + "/trunc.pem"), FormatError);

    // Dán nhầm: nạp file public như private và ngược lại
    REQUIRE_THROWS_AS(load_private_key(pub), FormatError);
    REQUIRE_THROWS_AS(load_public_key(priv), FormatError);

    // Rác / file rỗng / file không tồn tại
    write_file(dir + "/junk.pem", std::string("-----BEGIN PUBLIC KEY-----\nAAAA\n-----END PUBLIC KEY-----\n"));
    REQUIRE_THROWS_AS(load_public_key(dir + "/junk.pem"), FormatError);
    write_file(dir + "/empty.pem", Bytes{});
    REQUIRE_THROWS_AS(load_public_key(dir + "/empty.pem"), FormatError);
    REQUIRE_THROWS_AS(load_public_key(dir + "/missing.pem"), IoError);

    std::filesystem::remove_all(dir);
}

TEST_CASE("9. Decrypt errors are generic (no oracle by message)", "[negative][oracle]") {
    const Bytes ct = rsa_oaep_encrypt(keyA(), pattern(32), "L");
    std::string m1, m2, m3;
    try { (void)rsa_oaep_decrypt(keyB(), ct, "L"); } catch (const CryptoError& e) { m1 = e.what(); }       // khoá sai
    try { (void)rsa_oaep_decrypt(keyA(), ct, "X"); } catch (const CryptoError& e) { m2 = e.what(); }       // nhãn sai
    try { Bytes b = ct; b[5] ^= 1; (void)rsa_oaep_decrypt(keyA(), b, "L"); } catch (const CryptoError& e) { m3 = e.what(); }
    REQUIRE(m1 == kDecryptFailedMsg);
    REQUIRE(m1 == m2);
    REQUIRE(m2 == m3);
}
