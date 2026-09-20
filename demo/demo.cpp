#include <iostream>
#include <string>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>

int main() {
    try {
        std::cout << "=== CRYPTO++ TEST DEMO ===" << std::endl;

        // 1. Tạo ngẫu nhiên Key (AES-128: 16 bytes) và IV (16 bytes)
        CryptoPP::AutoSeededRandomPool prng;
        CryptoPP::byte key[CryptoPP::AES::DEFAULT_KEYLENGTH];
        CryptoPP::byte iv[CryptoPP::AES::BLOCKSIZE];

        prng.GenerateBlock(key, sizeof(key));
        prng.GenerateBlock(iv, sizeof(iv));

        // In Key và IV ra màn hình
        std::string hexKey, hexIV;
        CryptoPP::StringSource(key, sizeof(key), true,
            new CryptoPP::HexEncoder(new CryptoPP::StringSink(hexKey))
        );
        CryptoPP::StringSource(iv, sizeof(iv), true,
            new CryptoPP::HexEncoder(new CryptoPP::StringSink(hexIV))
        );

        std::cout << "[+] Key (Hex): " << hexKey << std::endl;
        std::cout << "[+] IV  (Hex): " << hexIV << std::endl;

        // 2. Dữ liệu thử nghiệm
        std::string plainText = "Hello Crypto++ from MinGW64!";
        std::cout << "[+] Plaintext: " << plainText << std::endl;

        // 3. Mã hóa AES-CBC
        std::string cipherText;
        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption encryptor;
        encryptor.SetKeyWithIV(key, sizeof(key), iv);

        CryptoPP::StringSource(plainText, true,
            new CryptoPP::StreamTransformationFilter(encryptor,
                new CryptoPP::StringSink(cipherText)
            )
        );

        std::string hexCipher;
        CryptoPP::StringSource(cipherText, true,
            new CryptoPP::HexEncoder(new CryptoPP::StringSink(hexCipher))
        );
        std::cout << "[+] Ciphertext (Hex): " << hexCipher << std::endl;

        // 4. Giải mã AES-CBC
        std::string recoveredText;
        CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption decryptor;
        decryptor.SetKeyWithIV(key, sizeof(key), iv);

        CryptoPP::StringSource(cipherText, true,
            new CryptoPP::StreamTransformationFilter(decryptor,
                new CryptoPP::StringSink(recoveredText)
            )
        );

        std::cout << "[+] Recovered: " << recoveredText << std::endl;

        if (plainText == recoveredText) {
            std::cout << "\n>>> TEST SUCCESSFUL: Crypto++ is linked and working properly! <<<" << std::endl;
        } else {
            std::cerr << "\n>>> TEST FAILED: Decrypted data mismatch! <<<" << std::endl;
            return 1;
        }

    } catch (const CryptoPP::Exception& e) {
        std::cerr << "Crypto++ Exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}