#include "aes128.hpp"

#include <cstring>

#include "aes_tables.hpp"
#include "errors.hpp"
#include "util.hpp"

using namespace std;

namespace aesp {
namespace {

// ---------------------------------------------------------------- GF(2^8) helpers
// Branch-free xtime: multiply by x (i.e. {02}) modulo x^8+x^4+x^3+x+1.
inline uint8_t xt(uint8_t x) { return static_cast<uint8_t>((x << 1) ^ (0x1B * (x >> 7))); }

// Constant-time GF(2^8) multiplication (no secret-dependent branches / indices).
inline uint8_t gmul_ct(uint8_t a, uint8_t b) {
  uint8_t p = 0;
  for (int i = 0; i < 8; ++i) {
    p ^= static_cast<uint8_t>(a & static_cast<uint8_t>(0 - (b & 1)));
    const uint8_t hi = static_cast<uint8_t>(a >> 7);
    a = static_cast<uint8_t>((a << 1) ^ (0x1B & static_cast<uint8_t>(0 - hi)));
    b = static_cast<uint8_t>(b >> 1);
  }
  return p;
}

// x^254 = x^-1 for x != 0, and 0 for x == 0 (254 = 2+4+8+...+128).
inline uint8_t gf_inv_ct(uint8_t x) {
  uint8_t r = 1, t = x;
  for (int i = 0; i < 7; ++i) {
    t = gmul_ct(t, t);
    r = gmul_ct(r, t);
  }
  return r;
}

inline uint8_t rotl8(uint8_t x, int n) { return tables::rotl8(x, n); }

// ---------------------------------------------------------------- S-box policies
struct TableSbox {
  static uint8_t sub(uint8_t x) { return tables::SBOX[x]; }
  static uint8_t inv(uint8_t x) { return tables::INV_SBOX[x]; }
};
struct ComputedSbox {
  static uint8_t sub(uint8_t x) { return sbox_computed(x); }
  static uint8_t inv(uint8_t x) { return inv_sbox_computed(x); }
};

// ---------------------------------------------------------------- KeyExpansion (§5.2)
template <class S>
void key_expansion(const uint8_t* key, uint8_t* w) {
  std::memcpy(w, key, 16);
  for (std::size_t i = 4; i < 44; ++i) {
    uint8_t t[4];
    std::memcpy(t, w + 4 * (i - 1), 4);
    if (i % 4 == 0) {  // RotWord, SubWord, XOR Rcon
      const uint8_t r = t[0];
      t[0] = static_cast<uint8_t>(S::sub(t[1]) ^ tables::RCON[i / 4 - 1]);
      t[1] = S::sub(t[2]);
      t[2] = S::sub(t[3]);
      t[3] = S::sub(r);
    }
    for (int j = 0; j < 4; ++j) w[4 * i + j] = static_cast<uint8_t>(w[4 * (i - 4) + j] ^ t[j]);
  }
}

// State layout: s[4*c + r] == s_{r,c} (FIPS-197 §3.4: input bytes fill the state column by column).
inline void add_round_key(uint8_t* s, const uint8_t* rk) {
  for (int i = 0; i < 16; ++i) s[i] ^= rk[i];
}

template <class S>
inline void sub_bytes(uint8_t* s) {
  for (int i = 0; i < 16; ++i) s[i] = S::sub(s[i]);
}
template <class S>
inline void inv_sub_bytes(uint8_t* s) {
  for (int i = 0; i < 16; ++i) s[i] = S::inv(s[i]);
}

inline void shift_rows(uint8_t* s) {  // row r rotated left by r
  uint8_t t[16];
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r) t[4 * c + r] = s[4 * ((c + r) & 3) + r];
  std::memcpy(s, t, 16);
}
inline void inv_shift_rows(uint8_t* s) {  // row r rotated right by r
  uint8_t t[16];
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r) t[4 * ((c + r) & 3) + r] = s[4 * c + r];
  std::memcpy(s, t, 16);
}

inline void mix_columns(uint8_t* s) {
  for (int c = 0; c < 4; ++c) {
    uint8_t* col = s + 4 * c;
    const uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
    const uint8_t t = static_cast<uint8_t>(a0 ^ a1 ^ a2 ^ a3);
    col[0] = static_cast<uint8_t>(a0 ^ t ^ xt(static_cast<uint8_t>(a0 ^ a1)));  // 2a0+3a1+a2+a3
    col[1] = static_cast<uint8_t>(a1 ^ t ^ xt(static_cast<uint8_t>(a1 ^ a2)));  // a0+2a1+3a2+a3
    col[2] = static_cast<uint8_t>(a2 ^ t ^ xt(static_cast<uint8_t>(a2 ^ a3)));  // a0+a1+2a2+3a3
    col[3] = static_cast<uint8_t>(a3 ^ t ^ xt(static_cast<uint8_t>(a3 ^ a0)));  // 3a0+a1+a2+2a3
  }
}

inline uint8_t m9(uint8_t a) {
  const uint8_t a8 = xt(xt(xt(a)));
  return static_cast<uint8_t>(a8 ^ a);
}
inline uint8_t m11(uint8_t a) {
  const uint8_t a2 = xt(a), a8 = xt(xt(a2));
  return static_cast<uint8_t>(a8 ^ a2 ^ a);
}
inline uint8_t m13(uint8_t a) {
  const uint8_t a4 = xt(xt(a)), a8 = xt(a4);
  return static_cast<uint8_t>(a8 ^ a4 ^ a);
}
inline uint8_t m14(uint8_t a) {
  const uint8_t a2 = xt(a), a4 = xt(a2), a8 = xt(a4);
  return static_cast<uint8_t>(a8 ^ a4 ^ a2);
}

inline void inv_mix_columns(uint8_t* s) {
  for (int c = 0; c < 4; ++c) {
    uint8_t* col = s + 4 * c;
    const uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
    col[0] = static_cast<uint8_t>(m14(a0) ^ m11(a1) ^ m13(a2) ^ m9(a3));
    col[1] = static_cast<uint8_t>(m9(a0) ^ m14(a1) ^ m11(a2) ^ m13(a3));
    col[2] = static_cast<uint8_t>(m13(a0) ^ m9(a1) ^ m14(a2) ^ m11(a3));
    col[3] = static_cast<uint8_t>(m11(a0) ^ m13(a1) ^ m9(a2) ^ m14(a3));
  }
}

// ---------------------------------------------------------------- Cipher / InvCipher
template <class S>
void cipher(const uint8_t* rk, const uint8_t* in, uint8_t* out) {
  uint8_t s[16];
  std::memcpy(s, in, 16);
  add_round_key(s, rk);
  for (std::size_t round = 1; round < Aes128::kRounds; ++round) {
    sub_bytes<S>(s);
    shift_rows(s);
    mix_columns(s);
    add_round_key(s, rk + 16 * round);
  }
  sub_bytes<S>(s);
  shift_rows(s);
  add_round_key(s, rk + 16 * Aes128::kRounds);
  std::memcpy(out, s, 16);
  secure_zero(s, sizeof s);
}

template <class S>
void inv_cipher(const uint8_t* rk, const uint8_t* in, uint8_t* out) {
  uint8_t s[16];
  std::memcpy(s, in, 16);
  add_round_key(s, rk + 16 * Aes128::kRounds);
  for (std::size_t round = Aes128::kRounds - 1; round >= 1; --round) {
    inv_shift_rows(s);
    inv_sub_bytes<S>(s);
    add_round_key(s, rk + 16 * round);
    inv_mix_columns(s);
  }
  inv_shift_rows(s);
  inv_sub_bytes<S>(s);
  add_round_key(s, rk);
  std::memcpy(out, s, 16);
  secure_zero(s, sizeof s);
}

inline uint32_t load_be(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
inline void store_be(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v >> 24);
  p[1] = static_cast<uint8_t>(v >> 16);
  p[2] = static_cast<uint8_t>(v >> 8);
  p[3] = static_cast<uint8_t>(v);
}

void cipher_ttable(const uint32_t* rk, const uint8_t* in, uint8_t* out) {
  using namespace tables;
  uint32_t s0 = load_be(in) ^ rk[0], s1 = load_be(in + 4) ^ rk[1];
  uint32_t s2 = load_be(in + 8) ^ rk[2], s3 = load_be(in + 12) ^ rk[3];
  for (int r = 1; r < 10; ++r) {
    rk += 4;
    const uint32_t t0 = TE0[s0 >> 24] ^ TE1[(s1 >> 16) & 0xFF] ^ TE2[(s2 >> 8) & 0xFF] ^ TE3[s3 & 0xFF] ^ rk[0];
    const uint32_t t1 = TE0[s1 >> 24] ^ TE1[(s2 >> 16) & 0xFF] ^ TE2[(s3 >> 8) & 0xFF] ^ TE3[s0 & 0xFF] ^ rk[1];
    const uint32_t t2 = TE0[s2 >> 24] ^ TE1[(s3 >> 16) & 0xFF] ^ TE2[(s0 >> 8) & 0xFF] ^ TE3[s1 & 0xFF] ^ rk[2];
    const uint32_t t3 = TE0[s3 >> 24] ^ TE1[(s0 >> 16) & 0xFF] ^ TE2[(s1 >> 8) & 0xFF] ^ TE3[s2 & 0xFF] ^ rk[3];
    s0 = t0; s1 = t1; s2 = t2; s3 = t3;
  }
  rk += 4;
  auto fin = [](uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return (static_cast<uint32_t>(SBOX[a >> 24]) << 24) | (static_cast<uint32_t>(SBOX[(b >> 16) & 0xFF]) << 16) |
           (static_cast<uint32_t>(SBOX[(c >> 8) & 0xFF]) << 8) | SBOX[d & 0xFF];
  };
  store_be(out,      fin(s0, s1, s2, s3) ^ rk[0]);
  store_be(out + 4,  fin(s1, s2, s3, s0) ^ rk[1]);
  store_be(out + 8,  fin(s2, s3, s0, s1) ^ rk[2]);
  store_be(out + 12, fin(s3, s0, s1, s2) ^ rk[3]);
}

}  // namespace

// ---------------------------------------------------------------- public S-box API
uint8_t sbox_table(uint8_t x) { return tables::SBOX[x]; }

uint8_t sbox_computed(uint8_t x) {
  const uint8_t i = gf_inv_ct(x);
  return static_cast<uint8_t>(i ^ rotl8(i, 1) ^ rotl8(i, 2) ^ rotl8(i, 3) ^ rotl8(i, 4) ^ 0x63);
}

uint8_t inv_sbox_computed(uint8_t x) {
  const uint8_t b = static_cast<uint8_t>(rotl8(x, 1) ^ rotl8(x, 3) ^ rotl8(x, 6) ^ 0x05);
  return gf_inv_ct(b);
}

const char* backend_name(Backend b) {
  switch (b) {
    case Backend::SBoxTable: return "sbox-table";
    case Backend::SBoxComputed: return "sbox-computed";
    case Backend::TTable: return "ttable";
  }
  return "?";
}

Backend parse_backend(const std::string& s) {
  if (s == "table" || s == "sbox-table") return Backend::SBoxTable;
  if (s == "computed" || s == "sbox-computed") return Backend::SBoxComputed;
  if (s == "ttable") return Backend::TTable;
  throw Error(ExitCode::Usage, "unknown S-box backend '" + s + "' (use table|computed|ttable)");
}

// ---------------------------------------------------------------- Aes128
Aes128::Aes128(const uint8_t* key, std::size_t key_len, Backend backend) : backend_(backend) {
  if (key == nullptr || key_len != kKeySize)
    throw Error(ExitCode::InvalidInput, "invalid key length: AES-128 requires exactly 16 bytes");
  if (backend == Backend::SBoxComputed)
    key_expansion<ComputedSbox>(key, rk_.data());
  else
    key_expansion<TableSbox>(key, rk_.data());
  for (std::size_t i = 0; i < 44; ++i) rkw_[i] = load_be(rk_.data() + 4 * i);
}

Aes128::~Aes128() {
  secure_zero(rk_.data(), rk_.size());
  secure_zero(rkw_.data(), rkw_.size() * sizeof(uint32_t));
}

void Aes128::encrypt_block(const uint8_t in[16], uint8_t out[16]) const {
  switch (backend_) {
    case Backend::SBoxTable: cipher<TableSbox>(rk_.data(), in, out); break;
    case Backend::SBoxComputed: cipher<ComputedSbox>(rk_.data(), in, out); break;
    case Backend::TTable: cipher_ttable(rkw_.data(), in, out); break;
  }
}

void Aes128::decrypt_block(const uint8_t in[16], uint8_t out[16]) const {
  if (backend_ == Backend::SBoxComputed)
    inv_cipher<ComputedSbox>(rk_.data(), in, out);
  else
    inv_cipher<TableSbox>(rk_.data(), in, out);  // TTable backend decrypts with the byte-wise path
}

}  // namespace aesp
