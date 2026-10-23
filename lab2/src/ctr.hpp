// AES-128-CTR per NIST SP 800-38A §6.5, built on the from-scratch Aes128.
//
// Counter format (documented as required by the lab):
//   * The 16-byte IV is the *initial counter block* T_1 (nonce || counter, 128 bits total).
//   * Standard incrementing function of SP 800-38A Appendix B.1 with m = 128:
//     the WHOLE block is a 128-bit BIG-ENDIAN integer, T_{i+1} = T_i + 1 (mod 2^128 in the
//     standard) -- byte 15 is the least-significant byte and carries propagate towards byte 0.
//   * Overflow behaviour (this tool): the counter is NEVER allowed to wrap. After block
//     0xFF..FF is used the stream is exhausted; asking for one more keystream block throws
//     Error(CounterOverflow) instead of silently repeating keystream (two-time pad).
//   * Keystream block S_i = AES_K(T_i); C = P xor S (also P = C xor S). No padding; the final
//     partial block uses only the needed leading keystream bytes.
//
// SECURITY: the IV/nonce MUST be unique per key. CTR gives confidentiality only -- there is no
// integrity: flipping ciphertext bit i flips exactly plaintext bit i.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "aes128.hpp"
#include "util.hpp"

using namespace std;

namespace aesp {

class CtrStream {
 public:
  // `aes` must outlive the stream.
  CtrStream(const Aes128& aes, const uint8_t iv[16]);
  ~CtrStream();
  CtrStream(const CtrStream&) = delete;
  CtrStream& operator=(const CtrStream&) = delete;

  // Encrypts/decrypts n bytes (same operation). in == out is allowed. Chunk sizes are arbitrary;
  // the keystream position is carried across calls. Throws Error(CounterOverflow) if the
  // counter would wrap.
  void process(const uint8_t* in, uint8_t* out, std::size_t n);

 private:
  void refill();
  const Aes128* aes_;
  uint8_t ctr_[16];
  uint8_t ks_[16];
  std::size_t pos_;
  bool exhausted_;
};

// One-shot helper (encrypt == decrypt).
Bytes ctr_crypt(const Aes128& aes, const uint8_t iv[16], const Bytes& data);

// Streaming file transform (1 MiB chunks, multi-GiB safe). Writes to "<out>.tmp" and renames on
// success; on any error the partial output is deleted. Refuses in == out.
void ctr_crypt_file(const Aes128& aes, const uint8_t iv[16], const std::string& in_path,
                    const std::string& out_path, bool overwrite);

// Increments a 16-byte big-endian counter; returns true if it wrapped (carry out of byte 0).
bool ctr_increment(uint8_t ctr[16]);

}  // namespace aesp
