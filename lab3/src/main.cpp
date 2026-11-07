#include <cstdio>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>

#include "include/bench.hpp"
#include "include/hybrid_engine.hpp"
#include "include/key_manager.hpp"
#include "include/kat_runner.hpp"
#include "include/rsa_oaep.hpp"
#include "include/utils.hpp"

using namespace rsalab;

namespace {

const char* kUsage =
    "rsatool - RSA-OAEP (SHA-256, MGF1-SHA256) and hybrid envelope encryption (AES-256-GCM)\n"
    "\n"
    "Usage:\n"
    "  rsatool keygen  --bits 3072|4096 --pub pub.pem --priv priv.pem\n"
    "  rsatool encrypt (--in FILE | --text \"...\") --pub pub.pem [--out FILE] [--label L] [--encode hex|base64|raw]\n"
    "  rsatool decrypt (--in FILE | --text \"...\") --priv priv.pem [--out FILE] [--label L] [--encode hex|base64|raw]\n"
    "  rsatool bench   [--what rsa|hybrid|all] [--bits 3072|4096|both] [--sizes 1024,1048576,104857600]\n"
    "                  [--runs 30] [--keygen-runs 30] [--rsa-block 100] [--warmup 1.0] [--csv out.csv]\n"
    "  rsatool --kat path/to/vectors.json\n"
    "\n"
    "Common: --verbose, --help\n"
    "Notes:\n"
    "  * Plaintext <= k-2*hLen-2 bytes -> direct RSA-OAEP; larger -> hybrid JSON envelope (automatic).\n"
    "  * --encode applies to raw RSA-OAEP ciphertext (default: raw to file, hex on screen).\n"
    "  * decrypt auto-detects raw RSA-OAEP ciphertext vs JSON envelope.\n";

struct Args {
    std::string command;
    std::map<std::string, std::string> opt;
    std::set<std::string> flag;

    bool has(const std::string& k) const { return opt.count(k) != 0; }
    std::string get(const std::string& k, const std::string& def = "") const {
        auto it = opt.find(k);
        return it == opt.end() ? def : it->second;
    }
};

const std::set<std::string> kValueOpts = {"--in",    "--text",        "--out",        "--pub",       "--priv",
                                          "--label", "--encode",      "--bits",       "--kat",       "--what",
                                          "--sizes", "--data-size",   "--runs",       "--keygen-runs",
                                          "--rsa-block", "--warmup",  "--csv"};
const std::set<std::string> kFlagOpts = {"--verbose", "--help", "-h"};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string tok = argv[i];
        if (tok.rfind("--", 0) == 0 || tok == "-h") {
            const auto eq = tok.find('=');
            const std::string name = tok.substr(0, eq);
            if (kFlagOpts.count(name)) { a.flag.insert(name); continue; }
            if (!kValueOpts.count(name)) throw UsageError("unknown option: " + name);
            std::string val;
            if (eq != std::string::npos) {
                val = tok.substr(eq + 1);
            } else {
                if (i + 1 >= argc) throw UsageError("missing value for " + name);
                val = argv[++i];
            }
            if (a.opt.count(name)) throw UsageError("duplicate option: " + name);
            a.opt[name] = val;
        } else if (a.command.empty()) {
            a.command = tok;
        } else {
            throw UsageError("unexpected argument: " + tok);
        }
    }
    return a;
}

std::string require(const Args& a, const std::string& name) {
    if (!a.has(name)) throw UsageError("missing required option: " + name);
    return a.get(name);
}

Bytes read_input(const Args& a) {
    if (a.has("--in") && a.has("--text")) throw UsageError("use either --in or --text, not both");
    if (a.has("--text")) return to_bytes(a.get("--text"));  // UTF-8 (argv được truyền nguyên byte)
    if (a.has("--in")) return read_file(a.get("--in"));
    throw UsageError("missing input: provide --in FILE or --text \"...\"");
}

int parse_int(const std::string& s, const char* what) {
    try {
        std::size_t pos = 0;
        const int v = std::stoi(s, &pos);
        if (pos != s.size()) throw std::invalid_argument("x");
        return v;
    } catch (...) {
        throw UsageError(std::string("invalid number for ") + what);
    }
}

std::vector<std::size_t> parse_sizes(const std::string& s) {
    std::vector<std::size_t> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        const int v = parse_int(item, "--sizes");
        if (v <= 0) throw UsageError("invalid size in --sizes");
        out.push_back(static_cast<std::size_t>(v));
    }
    if (out.empty()) throw UsageError("empty --sizes");
    return out;
}

// ---------------- Commands ----------------

int cmd_keygen(const Args& a) {
    const int bits = parse_int(a.get("--bits", "3072"), "--bits");
    const std::string pub = require(a, "--pub");
    const std::string priv = require(a, "--priv");
    if (pub == priv) throw UsageError("--pub and --priv must be different files");

    PKeyPtr key = generate_rsa_key(bits);  // UsageError nếu bits không được hỗ trợ
    const SavedKeyFiles f = save_keypair(key.get(), pub, priv);
    std::cout << "Generated RSA-" << bits << " key pair:\n"
              << "  public  (PEM/SPKI) : " << f.pub_pem << "\n"
              << "  public  (DER)      : " << f.pub_der << "\n"
              << "  private (PEM/PKCS8): " << f.priv_pem << "\n"
              << "  private (DER)      : " << f.priv_der << "\n"
              << "  metadata           : " << f.metadata << "\n"
              << "Keep the private key secret (file permissions restricted where supported).\n";
    return 0;
}

int cmd_encrypt(const Args& a) {
    const bool verbose = a.flag.count("--verbose") != 0;
    const std::string label = a.get("--label", "");
    std::optional<Encoding> enc;
    if (a.has("--encode")) enc = parse_encoding(a.get("--encode"));

    PKeyPtr pub = load_public_key(require(a, "--pub"));
    const Bytes plaintext = read_input(a);
    const std::size_t limit = oaep_max_plaintext(pub.get());

    if (plaintext.size() <= limit) {
        // --- RSA-OAEP trực tiếp ---
        const Bytes ct = rsa_oaep_encrypt(pub.get(), plaintext, label);
        if (verbose)
            std::cerr << "[info] direct RSA-OAEP (" << plaintext.size() << " <= " << limit << " bytes)\n";
        if (a.has("--out")) {
            const Encoding e = enc.value_or(Encoding::Raw);
            if (e == Encoding::Raw) write_file(a.get("--out"), ct);
            else write_file(a.get("--out"), (e == Encoding::Hex ? hex_encode(ct) : base64_encode(ct)) + "\n");
        } else {
            const Encoding e = enc.value_or(Encoding::Hex);
            if (e == Encoding::Raw) throw UsageError("--encode raw requires --out");
            std::cout << (e == Encoding::Hex ? hex_encode(ct) : base64_encode(ct)) << "\n";
        }
    } else {
        // --- Hybrid envelope ---
        const Envelope env = hybrid_encrypt(pub.get(), plaintext, label);
        const std::string json = envelope_to_json(env);
        if (verbose)
            std::cerr << "[info] plaintext " << plaintext.size() << " > " << limit
                      << " bytes: hybrid mode (RSA-OAEP + AES-256-GCM); envelope is JSON, --encode ignored\n";
        if (a.has("--out")) write_file(a.get("--out"), json);
        else std::cout << json;
    }
    return 0;
}

int cmd_decrypt(const Args& a) {
    const std::string label = a.get("--label", "");
    std::optional<Encoding> enc;
    if (a.has("--encode")) enc = parse_encoding(a.get("--encode"));

    PKeyPtr priv = load_private_key(require(a, "--priv"));
    const Bytes data = read_input(a);
    const std::size_t k = static_cast<std::size_t>(EVP_PKEY_get_size(priv.get()));

    Bytes plaintext;
    if (data.size() != k && looks_like_envelope(data)) {
        const Envelope env = envelope_from_json(to_string(data));
        plaintext = hybrid_decrypt(priv.get(), env, label);
    } else {
        Bytes ct = data;
        const Encoding e = enc.value_or(Encoding::Raw);
        if (e == Encoding::Hex) ct = hex_decode(strip_ascii_whitespace(to_string(data)));
        else if (e == Encoding::Base64) ct = base64_decode(strip_ascii_whitespace(to_string(data)));
        plaintext = rsa_oaep_decrypt(priv.get(), ct, label);
    }

    // Chỉ ghi ra sau khi giải mã/xác thực thành công (không tạo file khi thất bại).
    if (a.has("--out")) {
        write_file(a.get("--out"), plaintext);
    } else {
        if (!plaintext.empty()) std::fwrite(plaintext.data(), 1, plaintext.size(), stdout);
        std::fflush(stdout);
    }
    secure_wipe(plaintext);
    return 0;
}

int cmd_bench(const Args& a) {
    BenchOptions o;
    if (a.has("--bits")) {
        const std::string b = a.get("--bits");
        if (b == "both") o.bits = {3072, 4096};
        else o.bits = {parse_int(b, "--bits")};
    }
    if (a.has("--sizes")) o.sizes = parse_sizes(a.get("--sizes"));
    if (a.has("--data-size")) o.sizes = parse_sizes(a.get("--data-size"));
    if (a.has("--runs")) o.runs = parse_int(a.get("--runs"), "--runs");
    if (a.has("--keygen-runs")) o.keygen_runs = parse_int(a.get("--keygen-runs"), "--keygen-runs");
    if (a.has("--rsa-block")) o.rsa_block = parse_int(a.get("--rsa-block"), "--rsa-block");
    if (a.has("--warmup")) {
        try { o.warmup_sec = std::stod(a.get("--warmup")); } catch (...) { throw UsageError("invalid --warmup"); }
    }
    if (a.has("--what")) o.what = a.get("--what");
    if (a.has("--csv")) o.csv_path = a.get("--csv");
    if (o.rsa_block < 1 || o.keygen_runs < 2) throw UsageError("--rsa-block must be >= 1 and --keygen-runs >= 2");
    return run_bench(o);
}

}  // namespace

int main(int argc, char** argv) {
    bool verbose = false;
    try {
        const Args a = parse_args(argc, argv);
        verbose = a.flag.count("--verbose") != 0;

        if (a.flag.count("--help") || a.flag.count("-h") || (a.command.empty() && !a.has("--kat"))) {
            std::cout << kUsage;
            return (a.command.empty() && !a.flag.count("--help") && !a.flag.count("-h")) ? 2 : 0;
        }
        if (a.has("--kat")) return run_kat(a.get("--kat"), verbose);

        if (a.command == "keygen") return cmd_keygen(a);
        if (a.command == "encrypt") return cmd_encrypt(a);
        if (a.command == "decrypt") return cmd_decrypt(a);
        if (a.command == "bench") return cmd_bench(a);
        throw UsageError("unknown command: " + a.command);
    } catch (const UsageError& e) {
        std::cerr << "Usage error: " << e.what() << "\n(run 'rsatool --help')\n";
        return 2;
    } catch (const CryptoError& e) {
        // Thông điệp chung, không nêu nguyên nhân chi tiết (chống oracle).
        std::cerr << "Error: " << e.what() << "\n";
        return 3;
    } catch (const FormatError& e) {
        std::cerr << "Error: malformed input" << (verbose ? std::string(" (") + e.what() + ")" : "") << "\n";
        return 1;
    } catch (const IoError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: internal error" << (verbose ? std::string(" (") + e.what() + ")" : "") << "\n";
        return 1;
    }
}
