# Crypto++ Environment Verification Demo

A standalone verification program to validate the toolchain, CMake build pipeline, and Crypto++ static library linking before developing Labs 1–6.

---

## 1. Overview

The `demo_app` target verifies:
* C++17 compiler compliance (`g++` / `clang++` / `MSVC`).
* CMake discovery and linking via `FindCryptoPP.cmake`.
* Correct header lookup paths (`<cryptopp/aes.h>`, etc.).
* Basic functionality of Crypto++ primitives:
  * Cryptographically secure PRNG (`AutoSeededRandomPool`).
  * Symmetric encryption & decryption (AES-128 in CBC mode).
  * Filter and pipeline mechanics (`StreamTransformationFilter`, `HexEncoder`, `StringSink`).

---

## 2. Directory Structure

```text
demo/
├── CMakeLists.txt     # Demo build definition
├── demo.cpp           # Verification program
└── README.md          # Guide & troubleshooting