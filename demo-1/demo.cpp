#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

// Crypto++ Headers
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>

// OpenSSL Headers
#include <openssl/evp.h>
#include <openssl/rand.h>

void print_hex(const std::string& label, const unsigned char* data, size_t len) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    std::cout << std::dec << "\n";
}

int main() {
    std::cout << "=== DEMO-1: CRYPTO++ & OPENSSL VERIFICATION ===\n\n";

    // 1. OpenSSL: Tạo IV ngẫu nhiên 16 bytes bằng RAND_bytes
    std::cout << "[*] 1. OpenSSL: Generating 16-byte IV via RAND_bytes...\n";
    unsigned char iv[CryptoPP::AES::BLOCKSIZE];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        std::cerr << "[-] Error: OpenSSL RAND_bytes failed.\n";
        return 1;
    }
    print_hex("[+] OpenSSL IV", iv, sizeof(iv));

    // 2. Crypto++: Sinh key và mã hóa AES-128 CBC
    std::cout << "\n[*] 2. Crypto++: Generating key and encrypting with AES-CBC...\n";
    CryptoPP::AutoSeededRandomPool prng;
    CryptoPP::byte key[CryptoPP::AES::DEFAULT_KEYLENGTH];
    prng.GenerateBlock(key, sizeof(key));
    print_hex("[+] Crypto++ Key", key, sizeof(key));

    std::string plaintext = "Hello from demo-1 integrating Crypto++ and OpenSSL!";
    std::cout << "[+] Plaintext: " << plaintext << "\n";

    std::string ciphertext;
    try {
        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption encryptor;
        encryptor.SetKeyWithIV(key, sizeof(key), iv);

        CryptoPP::StringSource(plaintext, true,
            new CryptoPP::StreamTransformationFilter(encryptor,
                new CryptoPP::StringSink(ciphertext)
            )
        );
    } catch (const CryptoPP::Exception& e) {
        std::cerr << "[-] Crypto++ Encryption Error: " << e.what() << "\n";
        return 1;
    }

    print_hex("[+] Ciphertext (AES-CBC)", 
              reinterpret_cast<const unsigned char*>(ciphertext.data()), 
              ciphertext.size());

    // 3. OpenSSL: Tính hash SHA-256 của ciphertext bằng EVP API
    std::cout << "\n[*] 3. OpenSSL: Computing SHA-256 digest of ciphertext...\n";
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        std::cerr << "[-] Error: Failed to allocate EVP_MD_CTX.\n";
        return 1;
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(mdctx, ciphertext.data(), ciphertext.size()) != 1 ||
        EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        std::cerr << "[-] Error: OpenSSL EVP digest calculation failed.\n";
        EVP_MD_CTX_free(mdctx);
        return 1;
    }
    EVP_MD_CTX_free(mdctx);

    print_hex("[+] SHA-256 Digest", hash, hash_len);

    // 4. Crypto++: Giải mã và đối chứng dữ liệu gốc
    std::cout << "\n[*] 4. Crypto++: Decrypting ciphertext...\n";
    std::string recovered;
    try {
        CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption decryptor;
        decryptor.SetKeyWithIV(key, sizeof(key), iv);

        CryptoPP::StringSource(ciphertext, true,
            new CryptoPP::StreamTransformationFilter(decryptor,
                new CryptoPP::StringSink(recovered)
            )
        );
    } catch (const CryptoPP::Exception& e) {
        std::cerr << "[-] Crypto++ Decryption Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "[+] Recovered: " << recovered << "\n\n";

    if (recovered == plaintext) {
        std::cout << "=========================================================\n";
        std::cout << ">>> SUCCESS: demo-1 linked and executed cleanly! <<<\n";
        std::cout << "=========================================================\n";
    } else {
        std::cerr << "[-] FAILURE: Decrypted plaintext does not match original.\n";
        return 1;
    }

    return 0;
}