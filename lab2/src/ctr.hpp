#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "aes128.hpp"
#include "util.hpp"

namespace aesp {

class CtrStream {
    private:
        void refill();
        const Aes128* aes_;
        uint8_t ctr_[16];
        uint8_t ks_[16];
        size_t pos_;
        bool exhausted_;

    public:
        CtrStream(const Aes128& aes, const uint8_t iv[16]);
        ~CtrStream();
        CtrStream(const CtrStream&) = delete;
        CtrStream& operator=(const CtrStream&) = delete;

        void process(const uint8_t* in, uint8_t* out, size_t n);
};

Bytes ctr_crypt(const Aes128& aes, const uint8_t iv[16], const Bytes& data);

void ctr_crypt_file(const Aes128& aes, const uint8_t iv[16], string& in_path, const string& out_path, bool overwrite);

bool ctr_increment(uint8_t ctr[16]);

}