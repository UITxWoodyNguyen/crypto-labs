#include "aescore.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "cryptopp_all.hpp"

using namespace CryptoPP;
using namespace std;

namespace aesl {
    static const CryptoPP::byte* P(const string& s) {
        return reinterpret_cast<const CryptoPP::byte*>(s.data());
    }

    // Encoding Functions
    string toHex(const string& s) {
        string res;
        StringSource(s, true, new HexEncoder(new StringSink(res), false));
        return res;
    }

    string fromHex(const string& in) {
        string h;
        for(char c : in)
            if (!isspace(static_cast<unsigned char>(c))) h.push_back(c);
        if (h.size() % 2) throw Error("invalid hex: odd length");

        for(char c : h)
            if (!isxdigit(static_cast<unsigned char>(c))) throw Error("invalid hex: bad character");

        string res;
        StringSource(h, true, new HexDecoder(new StringSink(res)));
        return res;
    }

    string toB64 (const string& s) {
        string res;
        StringSource(s, true, new Base64Encoder(new StringSink(res), false));
        return res;
    }

    string fromB64 (const string& in) {
        string b;
        for(char c : in)
            if (!isspace(static_cast<unsigned char>(c))) b.push_back(c);

        if (b.empty() || b.size() % 4) throw Error("invalid base64");
        for(char c : b)
            if (!isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '/' && c != '=') throw Error("invalid base64: bad character");
        string res;
        StringSource(b, true, new Base64Decoder(new StringSink(res)));
        return res;
    }

    string readFile (const string& path) {
        ifstream f(path, ios::binary);
        if (!f) throw Error("cannot open input file: " + path);

        ostringstream oss;
        oss << f.rdbuf();
        if (f.bad()) throw Error("cannot read input file: " + path);
        return oss.str();
    }

    void writeFile (const string& path, const string& data) {
        ofstream f(path, ios::binary | ios::trunc);
        if (!f) throw Error("cannot open output file: " + path);

        f.write(data.data(), static_cast<streamsize>(data.size()));
        if (!f) throw Error("cannot write output file: " + path);
    }

    bool fileExists (const string& path) {
        ifstream f(path);
        return f.good();
    }

    string randomBytes (size_t len) {
        string res;
        res.resize(len);

        AutoSeededRandomPool prng;
        if (len) prng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&res[0]), len);
        return res;
    }

bool validMode (const string& m) {
    static const char* modes[] = {"ecb", "cbc", "cfb", "ofb", "ctr", "xts", "ccm", "gcm"};
    for(auto x : modes) if (m == x) return true;
    return false;
}

    bool isAead (const string& m) {
        return m == "ccm" || m == "gcm";
    }

    bool keyLenOk (const string& m, size_t len) {
        if (m == "xts") return len == 32 || len == 64;
        return len == 16 || len == 24 || len == 32;
    }

    bool ivLenOk (const string& m, size_t len) {
        if (m == "ecb") return len == 0;
        if (m == "gcm") return len == 12;
        if (m == "ccm") return len >= 7 && len <= 13;
        return len == 16;
    }

    size_t defaultIvLen (const string& m) {
        if (m == "ecb") return 0;
        if (m == "gcm" || m == "ccm") return 12;
        return 16;
    }

    string algName (const string& m, size_t keyLen) {
        string u = m;
        transform(u.begin(), u.end(), u.begin(), ::toupper);
        size_t bits = (m == "xts" ? keyLen / 2 : keyLen) * 8;
        return "AES-" + to_string(bits) + "-" + u;
    }

    // Core Functions
    static void validate (const Opts& o) {
        if (!validMode(o.mode)) throw Error("invalid mode: " + o.mode);
        if (!keyLenOk(o.mode, o.key.size())) throw Error("invalid key length for mode " + o.mode);
        if (!ivLenOk(o.mode, o.iv.size())) throw Error("invalid iv length for mode " + o.mode);
        if (isAead(o.mode) && o.tagLen != 4 && o.tagLen != 8 && o.tagLen != 16)
            throw Error("invalid tag length for AEAD mode " + o.mode);
    }

    template <class Mode>
    static string cryptT (bool enc, const Opts& o, const string& in, StreamTransformationFilter::BlockPaddingScheme pad) {
        string out;
        const bool needIv = (o.mode != "ecb");

        if (enc) {
            typename Mode::Encryption m;
            if (needIv) m.SetKeyWithIV(P(o.key), o.key.size(), P(o.iv), o.iv.size());
            else m.SetKey(P(o.key), o.key.size());

            StringSource(in, true, new StreamTransformationFilter(m, new StringSink(out), pad));
        } else {
            typename Mode::Decryption m;
            if (needIv) m.SetKeyWithIV(P(o.key), o.key.size(), P(o.iv), o.iv.size());
            else m.SetKey(P(o.key), o.key.size());

            StringSource(in, true, new StreamTransformationFilter(m, new StringSink(out), pad));
        }
        return out;
    }

    static string aeadEnc (AuthenticatedSymmetricCipher& e, const string& aad, const string& pt, size_t tagLen) {
        string out;
        AuthenticatedEncryptionFilter ef(e, new StringSink(out), false, static_cast<int>(tagLen));
        ef.ChannelPut(AAD_CHANNEL, P(aad), aad.size());
        ef.ChannelMessageEnd(AAD_CHANNEL);
        ef.ChannelPut(DEFAULT_CHANNEL, P(pt), pt.size());
        ef.ChannelMessageEnd(DEFAULT_CHANNEL);
        return out;
    }

    static string aeadDec (AuthenticatedSymmetricCipher& d, const string& aad, const string& ct, const string& tag) {
        string in = ct + tag;
        string out; 
        
        AuthenticatedDecryptionFilter df(d, new StringSink(out), AuthenticatedDecryptionFilter::DEFAULT_FLAGS, static_cast<size_t>(tag.size()));
        df.ChannelPut(AAD_CHANNEL, P(aad), aad.size());
        df.ChannelMessageEnd(AAD_CHANNEL);
        df.ChannelPut(DEFAULT_CHANNEL, P(in), in.size());
        df.ChannelMessageEnd(DEFAULT_CHANNEL);
        return out;
    }

    template <unsigned N>
    static Out ccmEnc(const Opts& o, const string& pt) {
        typename CCM<AES, N>::Encryption e;
        e.SetKeyWithIV(P(o.key), o.key.size(), P(o.iv), o.iv.size());
        e.SpecifyDataLengths(o.aad.size(), pt.size(), 0);
        string x = aeadEnc(e, o.aad, pt, N);
        return {x.substr(0, x.size() - N), x.substr(x.size() - N)};
    }

    template <unsigned N>
    static string ccmDec(const Opts& o, const string& ct, const string& tag) {
        typename CCM<AES, N>::Decryption d;
        d.SetKeyWithIV(P(o.key), o.key.size(), P(o.iv), o.iv.size());
        d.SpecifyDataLengths(o.aad.size(), ct.size(), 0);
        return aeadDec(d, o.aad, ct, tag);
    }

    Out encrypt(const Opts& o, const string& pt) {
        validate(o);
        try {
            const auto PKCS = o.pkcs ? StreamTransformationFilter::PKCS_PADDING
                                    : StreamTransformationFilter::NO_PADDING;
            const auto NOPAD = StreamTransformationFilter::NO_PADDING;
            if (o.mode == "ecb") return {cryptT<ECB_Mode<AES>>(true, o, pt, PKCS), ""};
            if (o.mode == "cbc") return {cryptT<CBC_Mode<AES>>(true, o, pt, PKCS), ""};
            if (o.mode == "ofb") return {cryptT<OFB_Mode<AES>>(true, o, pt, NOPAD), ""};
            if (o.mode == "cfb") return {cryptT<CFB_Mode<AES>>(true, o, pt, NOPAD), ""};
            if (o.mode == "ctr") return {cryptT<CTR_Mode<AES>>(true, o, pt, NOPAD), ""};
            if (o.mode == "xts") {
                if (pt.size() < 16) throw Error("XTS needs at least 16 bytes of input");
                return {cryptT<XTS_Mode<AES>>(true, o, pt, NOPAD), ""};
            }
            if (o.mode == "gcm") {
                GCM<AES>::Encryption e;
                e.SetKeyWithIV(P(o.key), o.key.size(), P(o.iv), o.iv.size());
                string x = aeadEnc(e, o.aad, pt, 16);
                return {x.substr(0, x.size() - 16), x.substr(x.size() - 16)};
            }
            if (o.tagLen == 4) return ccmEnc<4>(o, pt);
            if (o.tagLen == 8) return ccmEnc<8>(o, pt);
            return ccmEnc<16>(o, pt);
        } catch (const Error&) {
            throw;
        } catch (const CryptoPP::Exception& e) {
            throw Error(string("encryption failed: ") + e.what());
        }
    }

    string decrypt(const Opts& o, const string& ct, const string& tag) {
        validate(o);
        try {
            const auto PKCS = o.pkcs ? StreamTransformationFilter::PKCS_PADDING
                                    : StreamTransformationFilter::NO_PADDING;
            const auto NOPAD = StreamTransformationFilter::NO_PADDING;
            if (o.mode == "ecb") return cryptT<ECB_Mode<AES>>(false, o, ct, PKCS);
            if (o.mode == "cbc") return cryptT<CBC_Mode<AES>>(false, o, ct, PKCS);
            if (o.mode == "ofb") return cryptT<OFB_Mode<AES>>(false, o, ct, NOPAD);
            if (o.mode == "cfb") return cryptT<CFB_Mode<AES>>(false, o, ct, NOPAD);
            if (o.mode == "ctr") return cryptT<CTR_Mode<AES>>(false, o, ct, NOPAD);
            if (o.mode == "xts") {
                if (ct.size() < 16) throw Error("XTS needs at least 16 bytes of input");
                return cryptT<XTS_Mode<AES>>(false, o, ct, NOPAD);
            }
            if (tag.size() != (o.mode == "gcm" ? 16u : o.tagLen)) throw Error("invalid tag length");
            if (o.mode == "gcm") {
                GCM<AES>::Decryption d;
                d.SetKeyWithIV(P(o.key), o.key.size(), P(o.iv), o.iv.size());
                return aeadDec(d, o.aad, ct, tag);
            }
            if (o.tagLen == 4) return ccmDec<4>(o, ct, tag);
            if (o.tagLen == 8) return ccmDec<8>(o, ct, tag);
            return ccmDec<16>(o, ct, tag);
        } catch (const Error&) {
            throw;
        } catch (const CryptoPP::Exception&) {
            throw Error("decryption failed");
        }
    }

    // Key file
    string loadKeyFile(const string& path) {
        string d = readFile(path);
        const string H = "AESKEY-HEX";
        if (d.compare(0, H.size(), H) == 0) return fromHex(d.substr(H.size()));
        return d;  // raw binary
    }

    string makeHexKeyFile(const string& key) { return "AESKEY-HEX\n" + toHex(key) + "\n"; }

    static string nonceId(const Opts& o) {
        string in = o.key + "|" + o.mode + "|" + o.iv, d;
        SHA256 h;
        StringSource(in, true, new HashFilter(h, new HexEncoder(new StringSink(d), false)));
        return d;
    }

    bool nonceUsed(const string& dbPath, const Opts& o) {
        if (!(o.mode == "ctr" || o.mode == "ccm" || o.mode == "gcm")) return false;
        if (!fileExists(dbPath)) return false;
        std::istringstream ss(readFile(dbPath));
        string line, id = nonceId(o);
        while (std::getline(ss, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line == id) return true;
        }
        return false;
    }
    void nonceRecord(const string& dbPath, const Opts& o) {
        if (!(o.mode == "ctr" || o.mode == "ccm" || o.mode == "gcm")) return;
        std::ofstream f(dbPath, std::ios::app);
        if (!f) throw Error("cannot update nonce registry: " + dbPath);
        f << nonceId(o) << "\n";
    }

    // mini JSON parser
    vector<map<string, string>> parseJsonObjects(const string& s) {
        vector<map<string, string>> out;
        size_t i = 0;
        while ((i = s.find('{', i)) != string::npos) {
            size_t j = s.find('}', i);
            if (j == string::npos) throw Error("malformed JSON: missing '}'");
            string body = s.substr(i + 1, j - i - 1);
            map<string, string> kv;
            size_t p = 0;
            while (true) {
                size_t k1 = body.find('"', p);
                if (k1 == string::npos) break;
                size_t k2 = body.find('"', k1 + 1);
                if (k2 == string::npos) throw Error("malformed JSON key");
                string key = body.substr(k1 + 1, k2 - k1 - 1);
                size_t c = body.find(':', k2);
                if (c == string::npos) throw Error("malformed JSON: missing ':'");
                size_t v = c + 1;
                while (v < body.size() && std::isspace(static_cast<unsigned char>(body[v]))) ++v;
                string val;
                if (v < body.size() && body[v] == '"') {
                    size_t e = body.find('"', v + 1);
                    if (e == string::npos) throw Error("malformed JSON string");
                    val = body.substr(v + 1, e - v - 1);
                    p = e + 1;
                } else {
                    size_t e = body.find(',', v);
                    if (e == string::npos) e = body.size();
                    val = body.substr(v, e - v);
                    while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
                    p = e;
                }
                kv[key] = val;
            }
            out.push_back(kv);
            i = j + 1;
        }
        if (out.empty()) throw Error("malformed JSON: no objects");
        return out;
    }
    string toJson(const vector<std::pair<string, string>>& kv) {
        string s = "{\n";
        for (size_t i = 0; i < kv.size(); ++i) {
            s += "  \"" + kv[i].first + "\": \"" + kv[i].second + "\"";
            s += (i + 1 < kv.size()) ? ",\n" : "\n";
        }
        return s + "}\n";
    }
}