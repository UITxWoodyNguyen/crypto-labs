#include <iostream>
#include <string>
#include <cryptopp/rsa.h>
#include <cryptopp/osrng.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <cryptopp/files.h>

void print_hex(const std::string& label, const std::string& data) {
    std::string encoded;
    CryptoPP::StringSource(data, true,
        new CryptoPP::HexEncoder(new CryptoPP::StringSink(encoded))
    );
    std::cout << label << ": " << encoded << std::endl;
}

bool keys_equal(const CryptoPP::RSA::PublicKey& a, const CryptoPP::RSA::PublicKey& b) {
    return a.GetModulus() == b.GetModulus() && a.GetPublicExponent() == b.GetPublicExponent();
}

bool keys_equal(const CryptoPP::RSA::PrivateKey& a, const CryptoPP::RSA::PrivateKey& b) {
    return a.GetModulus() == b.GetModulus() 
        && a.GetPublicExponent() == b.GetPublicExponent()
        && a.GetPrivateExponent() == b.GetPrivateExponent();
}

int main() {
    try {
        std::cout << "=== CRYPTO++ RSA DEMO (Encryption + Digital Signature) ===" << std::endl;

        // 1. Generate RSA key pair (2048-bit)
        std::cout << "\n[*] 1. Generating RSA-2048 key pair..." << std::endl;
        CryptoPP::AutoSeededRandomPool prng;
        CryptoPP::RSA::PrivateKey privateKey;
        privateKey.GenerateRandomWithKeySize(prng, 2048);
        CryptoPP::RSA::PublicKey publicKey(privateKey);

        std::cout << "[+] Key pair generated successfully" << std::endl;
        std::cout << "    Modulus size: " << publicKey.GetModulus().ByteCount() << " bytes (" 
                  << publicKey.GetModulus().ByteCount() * 8 << " bits)" << std::endl;

        // 2. RSA Encryption/Decryption (OAEP padding with SHA-256)
        std::cout << "\n[*] 2. RSA-OAEP Encryption/Decryption (SHA-256)..." << std::endl;
        std::string plainText = "Secret message for RSA encryption!";
        std::cout << "[+] Plaintext: " << plainText << std::endl;

        CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(publicKey);
        CryptoPP::RSAES_OAEP_SHA_Decryptor decryptor(privateKey);

        std::string cipherText;
        CryptoPP::StringSource(plainText, true,
            new CryptoPP::PK_EncryptorFilter(prng, encryptor,
                new CryptoPP::StringSink(cipherText)
            )
        );
        print_hex("[+] Ciphertext", cipherText);

        std::string recoveredText;
        CryptoPP::StringSource(cipherText, true,
            new CryptoPP::PK_DecryptorFilter(prng, decryptor,
                new CryptoPP::StringSink(recoveredText)
            )
        );
        std::cout << "[+] Recovered: " << recoveredText << std::endl;

        if (plainText != recoveredText) {
            std::cerr << "[-] FAILURE: Decryption mismatch!" << std::endl;
            return 1;
        }
        std::cout << "[+] Encryption/Decryption: SUCCESS" << std::endl;

        // 3. Digital Signature (RSA-PKCS#1 v1.5 with SHA-256)
        std::cout << "\n[*] 3. RSA-PKCS#1 v1.5 Digital Signature (SHA-256)..." << std::endl;
        std::string message = "Important document to sign";
        std::cout << "[+] Message: " << message << std::endl;

        CryptoPP::RSASSA_PKCS1v15_SHA256_Signer signer(privateKey);
        CryptoPP::RSASSA_PKCS1v15_SHA256_Verifier verifier(publicKey);

        std::string signature;
        CryptoPP::StringSource(message, true,
            new CryptoPP::SignerFilter(prng, signer,
                new CryptoPP::StringSink(signature)
            )
        );
        print_hex("[+] Signature", signature);

        // Verify signature
        bool verified = false;
        CryptoPP::StringSource(signature + message, true,
            new CryptoPP::SignatureVerificationFilter(verifier,
                new CryptoPP::ArraySink((CryptoPP::byte*)&verified, sizeof(verified))
            )
        );

        if (verified) {
            std::cout << "[+] Signature verification: SUCCESS" << std::endl;
        } else {
            std::cerr << "[-] FAILURE: Signature verification failed!" << std::endl;
            return 1;
        }

        // 4. Test with tampered message
        std::cout << "\n[*] 4. Testing tampered message detection..." << std::endl;
        std::string tamperedMessage = message + " (tampered)";
        verified = false;
        CryptoPP::StringSource(signature + tamperedMessage, true,
            new CryptoPP::SignatureVerificationFilter(verifier,
                new CryptoPP::ArraySink((CryptoPP::byte*)&verified, sizeof(verified))
            )
        );

        if (!verified) {
            std::cout << "[+] Tampered message correctly rejected: SUCCESS" << std::endl;
        } else {
            std::cerr << "[-] FAILURE: Tampered message was accepted!" << std::endl;
            return 1;
        }

        // 5. Export/Import keys (DER format)
        std::cout << "\n[*] 5. Key Export/Import (DER format)..." << std::endl;
        std::string pubKeyDER, privKeyDER;
        publicKey.Save(CryptoPP::StringSink(pubKeyDER).Ref());
        privateKey.Save(CryptoPP::StringSink(privKeyDER).Ref());
        
        print_hex("[+] Public Key (DER)", pubKeyDER);
        print_hex("[+] Private Key (DER)", privKeyDER.substr(0, 64) + "..."); // truncate for display

        // Reload and verify
        CryptoPP::RSA::PublicKey loadedPubKey;
        loadedPubKey.Load(CryptoPP::StringSource(pubKeyDER, true, nullptr).Ref());
        
        CryptoPP::RSA::PrivateKey loadedPrivKey;
        loadedPrivKey.Load(CryptoPP::StringSource(privKeyDER, true, nullptr).Ref());

        if (keys_equal(loadedPubKey, publicKey) && keys_equal(loadedPrivKey, privateKey)) {
            std::cout << "[+] Key Export/Import: SUCCESS" << std::endl;
        } else {
            std::cerr << "[-] FAILURE: Key mismatch after reload!" << std::endl;
            return 1;
        }

        std::cout << "\n=========================================================" << std::endl;
        std::cout << ">>> ALL TESTS PASSED: RSA Demo completed successfully! <<<" << std::endl;
        std::cout << "=========================================================" << std::endl;

    } catch (const CryptoPP::Exception& e) {
        std::cerr << "Crypto++ Exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}