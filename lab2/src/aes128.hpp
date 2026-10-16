#pragma once
#include <iostream>
#include <array>
#include <cstdint>
#include <string>
#include <cstddef>

using namespace std;

enum class Backend { SBoxTable, SBoxComputed, TTable };

const char* backend_name(Backend b);

// Acp: table | sbox-table | computed | sbox-computed | ttable   (throws Error on others)
Backend parse_backend(const string& s);

uint8_t sbox_table(uint8_t x);
uint8_t sbox_computed(uint8_t x);
uint8_t inv_sbox_computed(uint8_t x);

namespace aesp {

class Aes128 {
    public:
        static constexpr size_t kKeySize = 16;
        static constexpr size_t kBlockSize = 16;
        static constexpr size_t kRounds = 10;
        static constexpr size_t kRoundKeyBytes = kBlockSize * (kRounds + 1);

        Aes128(const uint8_t* key, size_t key_len, Backend backend = Backend::SBoxTable);
        Aes128(const Aes128&) = default;
        Aes128& operator=(const Aes128&) = default;
        ~Aes128();
    private:
        array<uint8_t, kRoundKeyBytes> rk_{};
        array<uint32_t, 44> rkw_{}; // big-endian words of rk_ (for the T-table backend)
        Backend backend_;
};

}