#pragma once
#include <cstddef>
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <cstdint>

using namespace std;

namespace aesp {
    struct RefImpl {
        string name;  // "openssl" | "cryptopp"

        // AES-128-CTR: out = in XOR keystream(key, iv). in == out allowed.
        function<void(const uint8_t* key, const uint8_t* iv, 
                    const uint8_t* in, uint8_t* out, size_t n)> ctr;
    };

    vector<RefImpl> available_references(const string& which);

    struct XvalSummary {
        size_t total = 0, failed = 0;
    };

    XvalSummary run_xval(const string& which, size_t random_cases, ostream& os);
}