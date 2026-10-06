#include <catch2/catch_test_macros.hpp>
#include "aescore.hpp"
using namespace aesl;

static Opts mk(const std::string& mode) {
    Opts o; o.mode = mode;
    o.key = randomBytes(mode == "xts" ? 64 : 32);
    o.iv = randomBytes(defaultIvLen(mode));
    return o;
}
static const std::string MSG = "Hello Crypto++ lab 1 - thong diep tieng Viet co dau: Xin chao!";

TEST_CASE("round trip all modes") {
    for (std::string m : {"ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm"}) {
        Opts o = mk(m);
        Out r = encrypt(o, MSG);
        REQUIRE(decrypt(o, r.data, r.tag) == MSG);
    }
}
TEST_CASE("wrong key -> wrong plaintext or failure") {
    for (std::string m : {"ctr", "ofb", "cfb"}) {
        Opts o = mk(m); Out r = encrypt(o, MSG);
        o.key[0] ^= 1;
        REQUIRE(decrypt(o, r.data, r.tag) != MSG);
    }
    Opts o = mk("cbc"); Out r = encrypt(o, MSG); o.key[0] ^= 1;
    bool bad = false;
    try { bad = decrypt(o, r.data, "") != MSG; } catch (const Error&) { bad = true; }
    REQUIRE(bad);
}
TEST_CASE("wrong IV -> wrong plaintext") {
    Opts o = mk("ctr"); Out r = encrypt(o, MSG);
    o.iv[0] ^= 1;
    REQUIRE(decrypt(o, r.data, "") != MSG);
}
TEST_CASE("tampered ciphertext (non-AEAD) -> corrupted output, no detection") {
    Opts o = mk("ctr"); Out r = encrypt(o, MSG);
    r.data[3] ^= 0x80;
    std::string p = decrypt(o, r.data, "");
    REQUIRE(p != MSG);            // CTR: lat bit-flip -> chi bit tuong ung doi
    REQUIRE(p.size() == MSG.size());
}
TEST_CASE("AEAD: tampered ct / tag / aad / wrong key -> failure") {
    for (std::string m : {"gcm", "ccm"}) {
        Opts o = mk(m); o.aad = "header";
        Out r = encrypt(o, MSG);
        REQUIRE(decrypt(o, r.data, r.tag) == MSG);

        Out c = r; c.data[0] ^= 1;
        REQUIRE_THROWS_AS(decrypt(o, c.data, c.tag), Error);
        Out t = r; t.tag[0] ^= 1;
        REQUIRE_THROWS_AS(decrypt(o, t.data, t.tag), Error);
        Opts a = o; a.aad = "Header";
        REQUIRE_THROWS_AS(decrypt(a, r.data, r.tag), Error);
        Opts k = o; k.key[0] ^= 1;
        REQUIRE_THROWS_AS(decrypt(k, r.data, r.tag), Error);
        REQUIRE_THROWS_AS(decrypt(o, r.data, r.tag.substr(1)), Error);   // tag sai do dai
    }
}
TEST_CASE("invalid key / IV lengths rejected") {
    Opts o = mk("gcm"); o.iv.resize(8);
    REQUIRE_THROWS_AS(encrypt(o, MSG), Error);
    o = mk("cbc"); o.iv.resize(12);
    REQUIRE_THROWS_AS(encrypt(o, MSG), Error);
    o = mk("ctr"); o.key.resize(20);
    REQUIRE_THROWS_AS(encrypt(o, MSG), Error);
    o = mk("xts"); o.key.resize(48);
    REQUIRE_THROWS_AS(encrypt(o, MSG), Error);
    o = mk("ccm"); o.iv.resize(6);
    REQUIRE_THROWS_AS(encrypt(o, MSG), Error);
}
TEST_CASE("CBC: truncated ciphertext fails closed") {
    Opts o = mk("cbc"); Out r = encrypt(o, MSG);
    REQUIRE_THROWS_AS(decrypt(o, r.data.substr(0, r.data.size() - 3), ""), Error);
}
TEST_CASE("malformed encodings rejected") {
    REQUIRE_THROWS_AS(fromHex("abc"), Error);
    REQUIRE_THROWS_AS(fromHex("zz"), Error);
    REQUIRE_THROWS_AS(fromB64("@@@@"), Error);
    REQUIRE_THROWS_AS(parseJsonObjects("not json"), Error);
}
TEST_CASE("nonce registry detects reuse") {
    const std::string db = "test_nonce_db.tmp";
    std::remove(db.c_str());
    Opts o = mk("gcm");
    REQUIRE_FALSE(nonceUsed(db, o));
    nonceRecord(db, o);
    REQUIRE(nonceUsed(db, o));
    Opts o2 = o; o2.iv[0] ^= 1;
    REQUIRE_FALSE(nonceUsed(db, o2));
    Opts o3 = o; o3.key[0] ^= 1;
    REQUIRE_FALSE(nonceUsed(db, o3));
    std::remove(db.c_str());
}
