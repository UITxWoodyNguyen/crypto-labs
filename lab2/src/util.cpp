#include "util.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

#include "errors.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <sys/random.h>
#include <cerrno>
#include <unistd.h>
#else
#include <stdlib.h>
#endif

using namespace std;

namespace aesp {

void secure_zero(void* p, std::size_t n) noexcept {
  volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
  while (n--) *v++ = 0;
}

void secure_random(uint8_t* out, std::size_t n) {
  if (n == 0) return;
#if defined(_WIN32)
  while (n > 0) {
    const ULONG chunk = n > 0x40000000u ? 0x40000000u : static_cast<ULONG>(n);
    if (BCryptGenRandom(nullptr, out, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
      throw Error(ExitCode::Failure, "OS random number generator failed");
    out += chunk;
    n -= chunk;
  }
#elif defined(__linux__)
  while (n > 0) {
    const ssize_t r = getrandom(out, n > 256 ? 256 : n, 0);
    if (r < 0) {
      if (errno == EINTR) continue;
      throw Error(ExitCode::Failure, "OS random number generator failed");
    }
    out += r;
    n -= static_cast<std::size_t>(r);
  }
#else
  arc4random_buf(out, n);
#endif
}

Bytes random_bytes(std::size_t n) {
  Bytes b(n);
  secure_random(b.data(), n);
  return b;
}

// ------------------------------------------------------------------ hex / base64
std::string to_hex(const uint8_t* d, std::size_t n) {
  static const char* kHex = "0123456789abcdef";
  std::string s;
  s.reserve(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    s.push_back(kHex[d[i] >> 4]);
    s.push_back(kHex[d[i] & 0xF]);
  }
  return s;
}

static int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

Bytes from_hex(const std::string& s) {
  if (s.size() % 2 != 0) throw Error(ExitCode::InvalidInput, "malformed hex: odd length");
  Bytes out(s.size() / 2);
  for (std::size_t i = 0; i < out.size(); ++i) {
    const int hi = hex_val(s[2 * i]), lo = hex_val(s[2 * i + 1]);
    if (hi < 0 || lo < 0) throw Error(ExitCode::InvalidInput, "malformed hex: invalid character");
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return out;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string to_base64(const uint8_t* d, std::size_t n) {
  std::string s;
  s.reserve((n + 2) / 3 * 4);
  std::size_t i = 0;
  for (; i + 2 < n; i += 3) {
    const uint32_t v = (static_cast<uint32_t>(d[i]) << 16) | (static_cast<uint32_t>(d[i + 1]) << 8) | d[i + 2];
    s.push_back(kB64[v >> 18]);
    s.push_back(kB64[(v >> 12) & 63]);
    s.push_back(kB64[(v >> 6) & 63]);
    s.push_back(kB64[v & 63]);
  }
  if (i + 1 == n) {
    const uint32_t v = static_cast<uint32_t>(d[i]) << 16;
    s.push_back(kB64[v >> 18]);
    s.push_back(kB64[(v >> 12) & 63]);
    s += "==";
  } else if (i + 2 == n) {
    const uint32_t v = (static_cast<uint32_t>(d[i]) << 16) | (static_cast<uint32_t>(d[i + 1]) << 8);
    s.push_back(kB64[v >> 18]);
    s.push_back(kB64[(v >> 12) & 63]);
    s.push_back(kB64[(v >> 6) & 63]);
    s.push_back('=');
  }
  return s;
}

Bytes from_base64(const std::string& s) {
  if (s.size() % 4 != 0) throw Error(ExitCode::InvalidInput, "malformed base64: bad length");
  auto val = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  Bytes out;
  out.reserve(s.size() / 4 * 3);
  for (std::size_t i = 0; i < s.size(); i += 4) {
    int v[4];
    int pad = 0;
    for (int j = 0; j < 4; ++j) {
      const char c = s[i + j];
      if (c == '=') {
        // '=' only allowed in the last quartet, last two positions, and not followed by data.
        if (i + 4 != s.size() || j < 2) throw Error(ExitCode::InvalidInput, "malformed base64: bad padding");
        v[j] = 0;
        ++pad;
      } else {
        if (pad > 0) throw Error(ExitCode::InvalidInput, "malformed base64: bad padding");
        v[j] = val(c);
        if (v[j] < 0) throw Error(ExitCode::InvalidInput, "malformed base64: invalid character");
      }
    }
    const uint32_t w = (static_cast<uint32_t>(v[0]) << 18) | (static_cast<uint32_t>(v[1]) << 12) |
                       (static_cast<uint32_t>(v[2]) << 6) | static_cast<uint32_t>(v[3]);
    out.push_back(static_cast<uint8_t>(w >> 16));
    if (pad < 2) out.push_back(static_cast<uint8_t>(w >> 8));
    if (pad < 1) out.push_back(static_cast<uint8_t>(w));
  }
  return out;
}

Encoding parse_encoding(const std::string& s) {
  if (s == "hex") return Encoding::Hex;
  if (s == "base64") return Encoding::Base64;
  if (s == "raw") return Encoding::Raw;
  throw Error(ExitCode::Usage, "unsupported encoding '" + s + "' (use hex|base64|raw)");
}

std::string encode_bytes(const Bytes& b, Encoding e) {
  switch (e) {
    case Encoding::Hex: return to_hex(b);
    case Encoding::Base64: return to_base64(b.data(), b.size());
    case Encoding::Raw: break;
  }
  return std::string(b.begin(), b.end());
}

Bytes decode_text(const std::string& text, Encoding e) {
  if (e == Encoding::Raw) return Bytes(text.begin(), text.end());
  std::string t;
  t.reserve(text.size());
  for (char c : text)
    if (!std::isspace(static_cast<unsigned char>(c))) t.push_back(c);
  return e == Encoding::Hex ? from_hex(t) : from_base64(t);
}

std::size_t parse_size(const std::string& s) {
  std::size_t i = 0;
  unsigned long long v = 0;
  if (s.empty() || !std::isdigit(static_cast<unsigned char>(s[0])))
    throw Error(ExitCode::Usage, "invalid size '" + s + "'");
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
    if (v > (std::numeric_limits<unsigned long long>::max() - 9) / 10)
      throw Error(ExitCode::Usage, "size too large: '" + s + "'");
    v = v * 10 + static_cast<unsigned>(s[i] - '0');
    ++i;
  }
  const std::string suf = s.substr(i);
  unsigned long long mult = 1;
  if (suf.empty() || suf == "B") mult = 1;
  else if (suf == "KiB") mult = 1ULL << 10;
  else if (suf == "MiB") mult = 1ULL << 20;
  else if (suf == "GiB") mult = 1ULL << 30;
  else throw Error(ExitCode::Usage, "invalid size suffix in '" + s + "' (use B|KiB|MiB|GiB)");
  if (v > std::numeric_limits<std::size_t>::max() / mult)
    throw Error(ExitCode::Usage, "size too large: '" + s + "'");
  return static_cast<std::size_t>(v * mult);
}

// ------------------------------------------------------------------ files
std::filesystem::path to_path(const std::string& utf8) {
  return std::filesystem::u8path(utf8);
}

bool file_exists(const std::string& path) {
  std::error_code ec;
  return std::filesystem::exists(to_path(path), ec);
}

std::filesystem::path get_executable_dir() {
  std::error_code ec;
#if defined(_WIN32)
  wchar_t buf[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (len > 0 && len < MAX_PATH) {
    return std::filesystem::path(buf).parent_path();
  }
#elif defined(__linux__)
  char buf[1024];
  ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len > 0 && len < static_cast<ssize_t>(sizeof(buf))) {
    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path();
  }
#endif
  return std::filesystem::current_path(ec);
}

std::filesystem::path get_project_root() {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path cur = fs::current_path(ec);
  for (int i = 0; i < 6; ++i) {
    if (fs::exists(cur / "kat" / "vectors.json", ec) ||
        (fs::exists(cur / "CMakeLists.txt", ec) && fs::exists(cur / "src" / "aes128.cpp", ec))) {
      return cur;
    }
    if (cur == cur.parent_path()) break;
    cur = cur.parent_path();
  }

  fs::path exe = get_executable_dir();
  for (int i = 0; i < 6; ++i) {
    if (fs::exists(exe / "kat" / "vectors.json", ec) ||
        (fs::exists(exe / "CMakeLists.txt", ec) && fs::exists(exe / "src" / "aes128.cpp", ec))) {
      return exe;
    }
    if (exe == exe.parent_path()) break;
    exe = exe.parent_path();
  }

  return fs::current_path(ec);
}

std::filesystem::path get_os_output_dir() {
  namespace fs = std::filesystem;
#if defined(_WIN32)
  const char* os_name = "windows";
#else
  const char* os_name = "ubuntu";
#endif
  return get_project_root() / "Output" / os_name;
}

std::filesystem::path resolve_input_path(const std::string& path) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path p = to_path(path);
  if (fs::exists(p, ec) && !fs::is_directory(p, ec)) {
    return p;
  }
  if (p.is_relative()) {
    fs::path cand = p;
    for (int i = 0; i < 4; ++i) {
      cand = fs::path("..") / cand;
      if (fs::exists(cand, ec) && !fs::is_directory(cand, ec)) {
        return cand;
      }
    }

    fs::path exe_dir = get_executable_dir();
    fs::path base = exe_dir;
    for (int i = 0; i < 5; ++i) {
      fs::path p_cand = (base / p).lexically_normal();
      if (fs::exists(p_cand, ec) && !fs::is_directory(p_cand, ec)) {
        return p_cand;
      }
      if (base == base.parent_path()) break;
      base = base.parent_path();
    }

    fs::path root = get_project_root();
    fs::path p_root = (root / p).lexically_normal();
    if (fs::exists(p_root, ec) && !fs::is_directory(p_root, ec)) {
      return p_root;
    }

    fs::path stripped = p;
    while (!stripped.empty() && stripped.has_relative_path()) {
      auto it = stripped.begin();
      if (it != stripped.end() && *it == "..") {
        fs::path next;
        ++it;
        for (; it != stripped.end(); ++it) {
          next /= *it;
        }
        stripped = next;
        if (!stripped.empty() && fs::exists(root / stripped, ec) && !fs::is_directory(root / stripped, ec)) {
          return root / stripped;
        }
      } else {
        break;
      }
    }

    if (p.filename() == "vectors.json") {
      fs::path kat_vec = root / "kat" / "vectors.json";
      if (fs::exists(kat_vec, ec)) {
        return kat_vec;
      }
    }
  }
  return p;
}

Bytes read_file(const std::string& path, std::size_t max_bytes) {
  const std::filesystem::path resolved = resolve_input_path(path);
  std::ifstream f(resolved, std::ios::binary);
  if (!f) throw Error(ExitCode::Io, "cannot open input file '" + path + "'");
  f.seekg(0, std::ios::end);
  const std::streamoff size = f.tellg();
  if (size < 0) throw Error(ExitCode::Io, "cannot determine size of '" + path + "'");
  if (static_cast<unsigned long long>(size) > max_bytes)
    throw Error(ExitCode::InvalidInput, "input file too large for this operation");
  f.seekg(0, std::ios::beg);
  Bytes b(static_cast<std::size_t>(size));
  if (!b.empty()) f.read(reinterpret_cast<char*>(b.data()), size);
  if (!f) throw Error(ExitCode::Io, "read error on '" + path + "'");
  return b;
}

Bytes read_file_exact(const std::string& path, std::size_t expected, const char* what) {
  Bytes b = read_file(path, 1 << 20);
  if (b.size() != expected) {
    secure_zero(b.data(), b.size());
    throw Error(ExitCode::InvalidInput, std::string("invalid ") + what + " file: expected exactly " +
                                            std::to_string(expected) + " bytes");
  }
  return b;
}

void write_file(const std::string& path, const uint8_t* d, std::size_t n, bool overwrite) {
  namespace fs = std::filesystem;
  const fs::path out = to_path(path);
  std::error_code ec;
  if (!overwrite && fs::exists(out, ec))
    throw Error(ExitCode::Io, "output file '" + path + "' already exists (use --force to overwrite)");
  if (out.has_parent_path()) {
    fs::create_directories(out.parent_path(), ec);
  }
  fs::path tmp = out;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) throw Error(ExitCode::Io, "cannot create output file '" + path + "'");
    if (n) f.write(reinterpret_cast<const char*>(d), static_cast<std::streamsize>(n));
    f.flush();
    if (!f) {
      f.close();
      fs::remove(tmp, ec);
      throw Error(ExitCode::Io, "write error on '" + path + "'");
    }
  }
  fs::rename(tmp, out, ec);
  if (ec) {
    fs::remove(tmp, ec);
    throw Error(ExitCode::Io, "cannot finalize output file '" + path + "'");
  }
}

}  // namespace aesp
