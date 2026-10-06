#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "cryptopp_all.hpp"

using namespace std;

using namespace CryptoPP;

namespace aesl {
    struct Error : runtime_error {
        using runtime_error::runtime_error;
    };

    /* Encoding / IO */
    string toHex(const string& s);
    string fromHex(const string& h);
    string toB64(const string& s);
    string fromB64(const string& b);
    string readFile(const string& path);
    void writeFile(const string& path, const string& data);
    bool fileExists(const string& path);
    string randomBytes(size_t n);

    /* Mode Information */
    bool validMode(const string& m);
    bool isAead(const string& m);
    bool keyLenOk(const string& m, size_t n);
    bool ivLenOk(const string& m, size_t n);
    size_t defaultIvLen(const string& m);
    string algName(const string& m, size_t keyLen);

    /* core */
    struct Opts {
        string mode, key, iv, aad;
        size_t tagLen = 16;
        bool pkcs = true;
    };

    struct Out {
        string data, tag;
    };

    Out encrypt(const Opts& o, const string& pt);
    string decrypt(const Opts& o, const string& ct, const string& tag);

    /* key file */
    string loadKeyFile(const string& path);
    string makeHexKeyFile(const string& key);

    /* nonce-reuse registry (CTR/CCM/GCM) */
    bool nonceUsed(const string& dbPath, const Opts& o);
    void nonceRecord(const string& dbPath, const Opts& o);

    /* mini JSON (flat object, string/number values) */
    vector<map<string, string>> parseJsonObjects(const string& s);
    string toJson(const vector<pair<string, string>>& kv);
}