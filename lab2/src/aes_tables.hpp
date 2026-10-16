#pragma once

#include <array>
#include <iostream>
#include <cstdint>
#include <cstddef>

using namespace std;

namespace aesp {
namespace tables {

    // xtime(x) = x * 2 in GF(2^8)
    constexpr uint8_t xtime(uint8_t x) {
        return static_cast<uint8_t>((x << 1) ^ ((x & 0x80) ? 0x1B : 0x00));
    }

    // rotl8(x, n) = x rotated left by n bits (mod 8)
    constexpr uint8_t rotl8(uint8_t x, int n) {
        return static_cast<uint8_t>((x << n) | (x >> (8 - n)));
    }

    // rotr32(x, n) = x rotated right by n bits (mod 32)
    constexpr uint32_t rotr32(uint32_t x, int n) { 
        return (x >> n) | (x << (32 - n)); 
    }

    constexpr array<uint8_t, 256> make_sbox() {
        array<uint8_t, 256> s{};
        uint8_t p = 1, q = 1;
        do {
            p = static_cast<uint8_t>(p ^ (p << 1) ^ ((p & 0x80) ? 0x1B : 0));  // p *= 3
            q = static_cast<uint8_t>(q ^ (q << 1));                            // q /= 3
            q = static_cast<uint8_t>(q ^ (q << 2));
            q = static_cast<uint8_t>(q ^ (q << 4));
            if (q & 0x80) q = static_cast<uint8_t>(q ^ 0x09);
            s[p] = static_cast<uint8_t>(q ^ rotl8(q, 1) ^ rotl8(q, 2) ^ rotl8(q, 3) ^ rotl8(q, 4) ^ 0x63);
        } while (p != 1);

        s[0] = 0x63;
        return s;
    }

    constexpr array<uint8_t, 256> make_inv_sbox(const array<uint8_t, 256>& s) {
        array<uint8_t, 256> r{};
        for (size_t i = 0; i < 256; ++i) r[s[i]] = static_cast<uint8_t>(i);
        return r;
    }
    
    /* SBOX and inverse SBOX */
    inline constexpr array<uint8_t, 256> SBOX = make_sbox();
    inline constexpr array<uint8_t, 256> INV_SBOX = make_inv_sbox(SBOX);

    static_assert(SBOX[0x00] == 0x63 && SBOX[0x01] == 0x7C && SBOX[0x53] == 0xED && SBOX[0xFF] == 0x16,
                  "S-box generation error");
    static_assert(INV_SBOX[0x63] == 0x00 && INV_SBOX[0x16] == 0xFF, "inverse S-box generation error");

    inline constexpr array<uint8_t, 10> RCON = {0x01, 0x02, 0x04, 0x08, 0x10,
                                                0x20, 0x40, 0x80, 0x1B, 0x36};

    /* Round constants */
    // Te0[x] = {2*S[x], S[x], S[x], 3*S[x]} (big-endian word); Te1..Te3 are byte rotations.
    constexpr array<uint32_t, 256> make_te(int rot_bytes) {
        array<uint32_t, 256> t{};
        for (size_t x = 0; x < 256; ++x) {
            const uint8_t s = SBOX[x];
            const uint8_t s2 = xtime(s);
            const uint8_t s3 = static_cast<uint8_t>(s2 ^ s);
            const uint32_t w = (static_cast<uint32_t>(s2) << 24) | (static_cast<uint32_t>(s) << 16) |
                               (static_cast<uint32_t>(s) << 8) | s3;
            t[x] = rot_bytes == 0 ? w : rotr32(w, 8 * rot_bytes);
        }
        return t;
    }

    inline constexpr array<uint32_t, 256> TE0 = make_te(0);
    inline constexpr array<uint32_t, 256> TE1 = make_te(1);
    inline constexpr array<uint32_t, 256> TE2 = make_te(2);
    inline constexpr array<uint32_t, 256> TE3 = make_te(3);
}
}