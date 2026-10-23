// Unit tests for Lab 2 (pure AES-128 + CTR). Run with `ctest` or `./aes_tests`.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

#include "aes128.hpp"
#include "ctr.hpp"
#include "errors.hpp"
#include "json.hpp"
#include "kat.hpp"
#include "stats.hpp"
#include "util.hpp"
#include "xval.hpp"

#ifndef AESP_KAT_FILE
#define AESP_KAT_FILE "kat/vectors.json"
#endif

using namespace aesp;

namespace {

const std::array<Backend, 3> kAll = {Backend::SBoxTable, Backend::SBoxComputed, Backend::TTable};

Bytes H(const std::string& s) { return from_hex(s); }

bool throws_code(const std::function<void()>& f, ExitCode code) {
  try {
    f();
  } catch (const Error& e) {
    return e.code() == code;
  }
  return false;
}

std::filesystem::path temp_file(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("aesp_test_" + name);
}

}  // namespace

// ------------------------------------------------------------------ AES core
TEST_CASE("S-box: table and computed agree, is a bijection, no fixed points", "[aes][sbox]") {
  std::array<bool, 256> seen{};
  for (int x = 0; x < 256; ++x) {
    const uint8_t b = static_cast<uint8_t>(x);
    REQUIRE(sbox_table(b) == sbox_computed(b));
    REQUIRE(inv_sbox_computed(sbox_table(b)) == b);
    REQUIRE(sbox_table(b) != b);
    seen[sbox_table(b)] = true;
  }
  REQUIRE(std::all_of(seen.begin(), seen.end(), [](bool v) { return v; }));
  REQUIRE(sbox_table(0x53) == 0xED);  // FIPS-197 §5.1.1 example
}

TEST_CASE("KeyExpansion matches FIPS-197 Appendix A.1", "[aes][keyschedule]") {
  const Bytes key = H("2b7e151628aed2a6abf7158809cf4f3c");
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    const auto& rk = aes.round_keys();
    REQUIRE(to_hex(rk.data() + 16, 16) == "a0fafe1788542cb123a339392a6c7605");   // w4..w7
    REQUIRE(to_hex(rk.data() + 160, 16) == "d014f9a8c9ee2589e13f0cc8b6630ca6");  // w40..w43
  }
}

TEST_CASE("FIPS-197 Appendix C.1 encrypt/decrypt on every backend", "[aes][kat]") {
  const Bytes key = H("000102030405060708090a0b0c0d0e0f"), pt = H("00112233445566778899aabbccddeeff");
  const std::string ct = "69c4e0d86a7b0430d8cdb78070b4c55a";
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    uint8_t out[16], back[16];
    aes.encrypt_block(pt.data(), out);
    REQUIRE(to_hex(out, 16) == ct);
    aes.decrypt_block(out, back);
    REQUIRE(to_hex(back, 16) == to_hex(pt));
  }
}

TEST_CASE("In-place block operations work (in == out)", "[aes]") {
  const Bytes key = H("2b7e151628aed2a6abf7158809cf4f3c");
  Bytes blk = H("3243f6a8885a308d313198a2e0370734");
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    Bytes b = blk;
    aes.encrypt_block(b.data(), b.data());
    REQUIRE(to_hex(b) == "3925841d02dc09fbdc118597196a0b32");
    aes.decrypt_block(b.data(), b.data());
    REQUIRE(b == blk);
  }
}

TEST_CASE("Random blocks: all backends agree and decrypt inverts encrypt", "[aes]") {
  for (int i = 0; i < 20; ++i) {
    const Bytes key = random_bytes(16), pt = random_bytes(16);
    Aes128 a(key.data(), 16, Backend::SBoxTable), b(key.data(), 16, Backend::SBoxComputed),
        c(key.data(), 16, Backend::TTable);
    uint8_t x[16], y[16], z[16], d[16];
    a.encrypt_block(pt.data(), x);
    b.encrypt_block(pt.data(), y);
    c.encrypt_block(pt.data(), z);
    REQUIRE(std::equal(x, x + 16, y));
    REQUIRE(std::equal(x, x + 16, z));
    c.decrypt_block(z, d);
    REQUIRE(std::equal(d, d + 16, pt.begin()));
  }
}

TEST_CASE("Invalid key length is rejected (fail closed)", "[aes][negative]") {
  for (std::size_t n : {0u, 1u, 15u, 17u, 24u, 32u}) {
    Bytes k(n, 0x11);
    REQUIRE(throws_code([&] { Aes128 a(k.data(), k.size()); }, ExitCode::InvalidInput));
  }
}

// ------------------------------------------------------------------ KAT file
TEST_CASE("KAT runner passes on the shipped vector file", "[kat]") {
  std::ostringstream os;
  const KatSummary s = run_kat_file(AESP_KAT_FILE, false, os);
  INFO(os.str());
  REQUIRE(s.total >= 20);
  REQUIRE(s.failed == 0);
  REQUIRE(s.ok());
}

TEST_CASE("KAT runner detects a wrong expected value and malformed files", "[kat][negative]") {
  const auto p = temp_file("bad_kat.json");
  {
    std::ofstream f(p);
    f << R"({"aes128_block":[{"id":"bad","source":"x","key":"000102030405060708090a0b0c0d0e0f",
        "plaintext":"00112233445566778899aabbccddeeff","ciphertext":"00000000000000000000000000000000"}]})";
  }
  std::ostringstream os;
  const KatSummary s = run_kat_file(p.string(), false, os);
  REQUIRE(s.failed == 1);
  REQUIRE_FALSE(s.ok());

  {
    std::ofstream f(p, std::ios::trunc);
    f << R"({"aes128_block":[{"id":"x","key":"zz","plaintext":"","ciphertext":""}]})";
  }
  REQUIRE(throws_code([&] { std::ostringstream o; run_kat_file(p.string(), false, o); }, ExitCode::InvalidInput));
  {
    std::ofstream f(p, std::ios::trunc);
    f << "{ not json";
  }
  REQUIRE(throws_code([&] { std::ostringstream o; run_kat_file(p.string(), false, o); }, ExitCode::InvalidInput));
  {
    std::ofstream f(p, std::ios::trunc);
    f << "{}";
  }
  std::ostringstream o2;
  REQUIRE_FALSE(run_kat_file(p.string(), false, o2).ok());  // zero tests must not count as success
  std::filesystem::remove(p);
}

// ------------------------------------------------------------------ CTR
TEST_CASE("NIST SP 800-38A F.5.1 CTR-AES128 (4 blocks, all backends)", "[ctr][kat]") {
  const Bytes key = H("2b7e151628aed2a6abf7158809cf4f3c"), iv = H("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
  const Bytes pt = H("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
                     "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
  const Bytes ct = H("874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff"
                     "5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee");
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    REQUIRE(ctr_crypt(aes, iv.data(), pt) == ct);
    REQUIRE(ctr_crypt(aes, iv.data(), ct) == pt);
  }
}

TEST_CASE("CTR: arbitrary length, chunking and empty input", "[ctr]") {
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), key.size(), Backend::TTable);
  REQUIRE(ctr_crypt(aes, iv.data(), Bytes{}).empty());
  for (std::size_t len : {1u, 15u, 16u, 17u, 31u, 100u, 1000u}) {
    const Bytes pt = random_bytes(len);
    const Bytes ct = ctr_crypt(aes, iv.data(), pt);
    REQUIRE(ct.size() == len);  // no padding
    REQUIRE(ctr_crypt(aes, iv.data(), ct) == pt);
    for (std::size_t chunk : {1u, 5u, 16u, 33u}) {
      CtrStream s(aes, iv.data());
      Bytes out(len);
      for (std::size_t off = 0; off < len; off += chunk)
        s.process(pt.data() + off, out.data() + off, std::min<std::size_t>(chunk, len - off));
      REQUIRE(out == ct);
    }
  }
}

TEST_CASE("CTR counter is a 128-bit big-endian integer with carry", "[ctr][counter]") {
  const Bytes key = random_bytes(16);
  Aes128 aes(key.data(), key.size());
  const Bytes iv = H("0000000000000000000000000000ffff");  // ...ffff -> ...010000 -> ...010001
  const Bytes ks = ctr_crypt(aes, iv.data(), Bytes(48, 0));  // keystream = AES(counter blocks)
  const char* ctrs[3] = {"0000000000000000000000000000ffff", "00000000000000000000000000010000",
                         "00000000000000000000000000010001"};
  for (int i = 0; i < 3; ++i) {
    Bytes blk = H(ctrs[i]), expect(16);
    aes.encrypt_block(blk.data(), expect.data());
    REQUIRE(std::equal(expect.begin(), expect.end(), ks.begin() + 16 * i));
  }
  uint8_t c[16];
  std::fill(c, c + 16, static_cast<uint8_t>(0xFF));
  REQUIRE(ctr_increment(c));  // wraps to zero
  REQUIRE(std::all_of(c, c + 16, [](uint8_t v) { return v == 0; }));
}

TEST_CASE("CTR counter overflow is detected and fails closed", "[ctr][counter][negative]") {
  const Bytes key = random_bytes(16);
  Aes128 aes(key.data(), key.size());
  const Bytes ff(16, 0xFF);
  Bytes one(16, 0), two(17, 0);
  REQUIRE_NOTHROW(ctr_crypt(aes, ff.data(), one));  // last valid block
  REQUIRE(throws_code([&] { ctr_crypt(aes, ff.data(), two); }, ExitCode::CounterOverflow));
  Bytes fe(16, 0xFF);
  fe[15] = 0xFE;
  REQUIRE_NOTHROW(ctr_crypt(aes, fe.data(), Bytes(32, 0)));
  REQUIRE(throws_code([&] { ctr_crypt(aes, fe.data(), Bytes(33, 0)); }, ExitCode::CounterOverflow));
}

TEST_CASE("Negative: wrong key / wrong IV give wrong plaintext", "[ctr][negative]") {
  const Bytes key = random_bytes(16), key2 = random_bytes(16), iv = random_bytes(16), iv2 = random_bytes(16);
  const Bytes pt = random_bytes(200);
  Aes128 a(key.data(), 16), wrong(key2.data(), 16);
  const Bytes ct = ctr_crypt(a, iv.data(), pt);
  REQUIRE(ctr_crypt(wrong, iv.data(), ct) != pt);
  REQUIRE(ctr_crypt(a, iv2.data(), ct) != pt);
  REQUIRE(ctr_crypt(a, iv.data(), ct) == pt);
}

TEST_CASE("Negative: tampering is NOT detected; bit flips are malleable (CTR has no integrity)", "[ctr][negative]") {
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), 16);
  const Bytes pt = random_bytes(64);
  Bytes ct = ctr_crypt(aes, iv.data(), pt);
  ct[20] ^= 0x04;  // attacker flips one ciphertext bit
  const Bytes dec = ctr_crypt(aes, iv.data(), ct);  // no error is raised...
  for (std::size_t i = 0; i < pt.size(); ++i)
    REQUIRE(dec[i] == (i == 20 ? static_cast<uint8_t>(pt[i] ^ 0x04) : pt[i]));  // ...and exactly that bit flips
}

TEST_CASE("Two-time pad: IV reuse leaks P1 xor P2", "[ctr][security]") {
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), 16);
  const std::string a = "ATTACK AT DAWN!!", b = "RETREAT AT NOON!";
  const Bytes c1 = ctr_crypt(aes, iv.data(), Bytes(a.begin(), a.end()));
  const Bytes c2 = ctr_crypt(aes, iv.data(), Bytes(b.begin(), b.end()));  // catastrophic reuse
  for (std::size_t i = 0; i < a.size(); ++i)
    REQUIRE((c1[i] ^ c2[i]) == (static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i])));
}

// ------------------------------------------------------------------ file streaming
TEST_CASE("Streaming file encrypt/decrypt round trip (odd size > chunk) and failure cleanup", "[ctr][file]") {
  namespace fs = std::filesystem;
  const fs::path in = temp_file("in.bin"), enc = temp_file("enc.bin"), dec = temp_file("dec.bin");
  fs::remove(enc);
  fs::remove(dec);
  const Bytes data = random_bytes((1u << 20) * 2 + 12345);
  write_file(in.string(), data.data(), data.size(), true);
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), 16, Backend::TTable);
  ctr_crypt_file(aes, iv.data(), in.string(), enc.string(), false);
  ctr_crypt_file(aes, iv.data(), enc.string(), dec.string(), false);
  REQUIRE(read_file(dec.string(), 1u << 30) == data);
  REQUIRE(read_file(enc.string(), 1u << 30) == ctr_crypt(aes, iv.data(), data));

  // refuses to overwrite, refuses in==out, missing input -> I/O error and no partial output
  REQUIRE(throws_code([&] { ctr_crypt_file(aes, iv.data(), in.string(), enc.string(), false); }, ExitCode::Io));
  REQUIRE(throws_code([&] { ctr_crypt_file(aes, iv.data(), in.string(), in.string(), true); }, ExitCode::InvalidInput));
  const fs::path missing = temp_file("missing.bin"), out2 = temp_file("out2.bin");
  fs::remove(out2);
  REQUIRE(throws_code([&] { ctr_crypt_file(aes, iv.data(), missing.string(), out2.string(), false); }, ExitCode::Io));
  REQUIRE_FALSE(fs::exists(out2));

  // overflow in the middle of a stream must not leave output behind
  const Bytes ff(16, 0xFF);
  Aes128 aes2(key.data(), 16);
  REQUIRE(throws_code([&] { ctr_crypt_file(aes2, ff.data(), in.string(), out2.string(), true); }, ExitCode::CounterOverflow));
  REQUIRE_FALSE(fs::exists(out2));
  REQUIRE_FALSE(fs::exists(fs::path(out2.string() + ".tmp")));
  for (const auto& p : {in, enc, dec}) fs::remove(p);
}

// ------------------------------------------------------------------ codecs / parsers / stats
TEST_CASE("Hex and base64 codecs: round trip and strict rejection", "[util][negative]") {
  const Bytes b = random_bytes(37);
  REQUIRE(from_hex(to_hex(b)) == b);
  REQUIRE(from_base64(to_base64(b.data(), b.size())) == b);
  REQUIRE(to_base64(reinterpret_cast<const uint8_t*>("Man"), 3) == "TWFu");
  REQUIRE(to_base64(reinterpret_cast<const uint8_t*>("Ma"), 2) == "TWE=");
  REQUIRE(to_base64(reinterpret_cast<const uint8_t*>("M"), 1) == "TQ==");
  for (const char* bad : {"abc", "zz", "0g", " 00"}) REQUIRE(throws_code([&] { from_hex(bad); }, ExitCode::InvalidInput));
  for (const char* bad : {"TWF", "TW=u", "T===", "TWFu!", "=WFu", "TQ==TQ=="})
    REQUIRE(throws_code([&] { from_base64(bad); }, ExitCode::InvalidInput));
  REQUIRE_THROWS_AS(parse_encoding("utf16"), Error);
}

TEST_CASE("Key/IV files must be exactly 16 bytes", "[util][negative]") {
  const auto p = temp_file("key.bin");
  for (std::size_t n : {0u, 15u, 17u, 32u}) {
    const Bytes b(n, 0x42);
    write_file(p.string(), b.data(), b.size(), true);
    REQUIRE(throws_code([&] { read_file_exact(p.string(), 16, "key"); }, ExitCode::InvalidInput));
  }
  const Bytes ok(16, 0x42);
  write_file(p.string(), ok.data(), ok.size(), true);
  REQUIRE(read_file_exact(p.string(), 16, "key") == ok);
  REQUIRE(throws_code([&] { read_file_exact(temp_file("nope").string(), 16, "key"); }, ExitCode::Io));
  std::filesystem::remove(p);
}

TEST_CASE("Secure RNG returns fresh bytes", "[util][rng]") {
  const Bytes a = random_bytes(32), b = random_bytes(32);
  REQUIRE(a != b);
  REQUIRE(a != Bytes(32, 0));
}

TEST_CASE("parse_size", "[util]") {
  REQUIRE(parse_size("1MiB") == (1u << 20));
  REQUIRE(parse_size("16") == 16);
  REQUIRE(parse_size("2GiB") == (2ull << 30));
  REQUIRE_THROWS_AS(parse_size("1MB"), Error);
  REQUIRE_THROWS_AS(parse_size("-1"), Error);
  REQUIRE_THROWS_AS(parse_size(""), Error);
}

TEST_CASE("JSON parser: valid and malformed", "[json][negative]") {
  const Json j = parse_json(R"({"a":[1,2.5,true,null],"s":"x\u00e9\n","o":{}})");
  REQUIRE(j.at("a").array.size() == 4);
  REQUIRE(j.str("s") == "x\xC3\xA9\n");
  for (const char* bad : {"", "{", "{\"a\":}", "[1,]", "{\"a\":1,\"a\":2}", "{} x", "\"\\ud800\"", "[01]", "{\"a\" 1}"})
    REQUIRE(throws_code([&] { parse_json(bad); }, ExitCode::InvalidInput));
  std::string deep(200, '[');
  REQUIRE(throws_code([&] { parse_json(deep); }, ExitCode::InvalidInput));
}

TEST_CASE("Statistics: mean, median, stddev, 95% CI", "[stats]") {
  const Stats s = compute_stats({2, 4, 4, 4, 5, 5, 7, 9});
  REQUIRE(s.mean == 5.0);
  REQUIRE(s.median == 4.5);
  REQUIRE(s.stddev > 2.13);
  REQUIRE(s.stddev < 2.14);
  REQUIRE(s.ci95_lo < s.mean);
  REQUIRE(s.ci95_hi > s.mean);
  REQUIRE(t_critical_95(29) == 2.045);
}

// ------------------------------------------------------------------ cross-validation (optional libs)
TEST_CASE("Cross-validation against reference libraries (if compiled in)", "[xval]") {
  const auto refs = available_references("all");
  if (refs.empty()) SUCCEED("no reference library available in this build");
  std::ostringstream os;
  if (!refs.empty()) {
    const XvalSummary s = run_xval("all", 20, os);
    INFO(os.str());
    REQUIRE(s.total > 0);
    REQUIRE(s.failed == 0);
  }
}
