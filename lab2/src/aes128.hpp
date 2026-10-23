// AES-128 block cipher, implemented from scratch per FIPS-197 (no external crypto library).
//
// Three interchangeable SubBytes/round backends are provided so the lab report can compare
// them (table vs computed S-box, memory footprint, cache behaviour):
//
//   SBoxTable    byte-oriented reference implementation, 256-byte S-box lookup (FIPS-197 style)
//   SBoxComputed no lookup tables: S(x) = affine(x^254) using branch-free GF(2^8) arithmetic
//                (no secret-dependent memory index or branch; ~100x slower)
//   TTable       4 x 1 KiB T-tables, 32-bit words (fastest; classic cache-timing target)
//
// The key schedule is always FIPS-197 §5.2 (KeyExpansion), using the backend's S-box.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

using namespace std;

namespace aesp {

enum class Backend { SBoxTable, SBoxComputed, TTable };

const char* backend_name(Backend b);
// Accepts: table | sbox-table | computed | sbox-computed | ttable   (throws Error on others)
Backend parse_backend(const std::string& s);

uint8_t sbox_table(uint8_t x);         // FIPS-197 Figure 7 (table lookup)
uint8_t sbox_computed(uint8_t x);      // same function, computed without tables
uint8_t inv_sbox_computed(uint8_t x);  // inverse S-box, computed without tables

class Aes128 {
 public:
  static constexpr std::size_t kKeySize = 16;
  static constexpr std::size_t kBlockSize = 16;
  static constexpr std::size_t kRounds = 10;
  static constexpr std::size_t kRoundKeyBytes = kBlockSize * (kRounds + 1);  // 176

  // Throws Error(InvalidInput) unless key_len == 16.
  Aes128(const uint8_t* key, std::size_t key_len, Backend backend = Backend::SBoxTable);
  Aes128(const Aes128&) = default;
  Aes128& operator=(const Aes128&) = default;
  ~Aes128();  // wipes the round keys

  // Cipher / InvCipher of FIPS-197 §5.1 / §5.3. `in` and `out` may alias.
  void encrypt_block(const uint8_t in[16], uint8_t out[16]) const;
  void decrypt_block(const uint8_t in[16], uint8_t out[16]) const;

  Backend backend() const noexcept { return backend_; }
  const std::array<uint8_t, kRoundKeyBytes>& round_keys() const noexcept { return rk_; }

 private:
  std::array<uint8_t, kRoundKeyBytes> rk_{};
  std::array<uint32_t, 44> rkw_{};  // big-endian words of rk_ (for the T-table backend)
  Backend backend_;
};

}  // namespace aesp
