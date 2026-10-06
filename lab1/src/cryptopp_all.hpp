// Gom tat ca header Crypto++ o mot cho
// Windows (flat include/): <aes.h>, Ubuntu/vcpkg: <cryptopp/aes.h> hoặc <crypto++/aes.h>
#pragma once
#if __has_include(<aes.h>)
  #include <aes.h>
  #include <modes.h>
  #include <gcm.h>
  #include <ccm.h>
  #include <xts.h>
  #include <filters.h>
  #include <osrng.h>
  #include <hex.h>
  #include <base64.h>
  #include <sha.h>
  #include <secblock.h>
  #include <cryptlib.h>
#elif __has_include(<cryptopp/aes.h>)
  #include <cryptopp/aes.h>
  #include <cryptopp/modes.h>
  #include <cryptopp/gcm.h>
  #include <cryptopp/ccm.h>
  #include <cryptopp/xts.h>
  #include <cryptopp/filters.h>
  #include <cryptopp/osrng.h>
  #include <cryptopp/hex.h>
  #include <cryptopp/base64.h>
  #include <cryptopp/sha.h>
  #include <cryptopp/secblock.h>
  #include <cryptopp/cryptlib.h>
#else
  #include <crypto++/aes.h>
  #include <crypto++/modes.h>
  #include <crypto++/gcm.h>
  #include <crypto++/ccm.h>
  #include <crypto++/xts.h>
  #include <crypto++/filters.h>
  #include <crypto++/osrng.h>
  #include <crypto++/hex.h>
  #include <crypto++/base64.h>
  #include <crypto++/sha.h>
  #include <crypto++/secblock.h>
  #include <crypto++/cryptlib.h>
#endif
