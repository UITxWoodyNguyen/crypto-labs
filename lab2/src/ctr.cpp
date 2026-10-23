#include "ctr.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "errors.hpp"

using namespace std;

namespace aesp {

bool ctr_increment(uint8_t ctr[16]) {
  for (int i = 15; i >= 0; --i) {
    if (++ctr[i] != 0) return false;  // no carry out of this byte
  }
  return true;  // all bytes were 0xFF -> wrapped to 0
}

CtrStream::CtrStream(const Aes128& aes, const uint8_t iv[16])
    : aes_(&aes), pos_(16), exhausted_(false) {
  std::memcpy(ctr_, iv, 16);
  std::memset(ks_, 0, sizeof ks_);
}

CtrStream::~CtrStream() {
  secure_zero(ctr_, sizeof ctr_);
  secure_zero(ks_, sizeof ks_);
}

void CtrStream::refill() {
  if (exhausted_)
    throw Error(ExitCode::CounterOverflow,
                "CTR counter overflow: message too long for this IV (keystream would repeat)");
  aes_->encrypt_block(ctr_, ks_);
  exhausted_ = ctr_increment(ctr_);  // wrapped => the next refill must fail
  pos_ = 0;
}

void CtrStream::process(const uint8_t* in, uint8_t* out, std::size_t n) {
  while (n > 0) {
    if (pos_ == 16) refill();
    const std::size_t take = std::min<std::size_t>(n, 16 - pos_);
    for (std::size_t i = 0; i < take; ++i) out[i] = static_cast<uint8_t>(in[i] ^ ks_[pos_ + i]);
    pos_ += take;
    in += take;
    out += take;
    n -= take;
  }
}

Bytes ctr_crypt(const Aes128& aes, const uint8_t iv[16], const Bytes& data) {
  Bytes out(data.size());
  CtrStream s(aes, iv);
  s.process(data.data(), out.data(), data.size());
  return out;
}

void ctr_crypt_file(const Aes128& aes, const uint8_t iv[16], const std::string& in_path,
                    const std::string& out_path, bool overwrite) {
  namespace fs = std::filesystem;
  const fs::path in = to_path(in_path), out = to_path(out_path);
  std::error_code ec;
  if (fs::exists(out, ec)) {
    if (fs::exists(in, ec) && fs::equivalent(in, out, ec))
      throw Error(ExitCode::InvalidInput, "input and output must be different files");
    if (!overwrite)
      throw Error(ExitCode::Io, "output file '" + out_path + "' already exists (use --force to overwrite)");
  }

  std::ifstream fin(in, std::ios::binary);
  if (!fin) throw Error(ExitCode::Io, "cannot open input file '" + in_path + "'");
  fs::path tmp = out;
  tmp += ".tmp";
  std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
  if (!fout) throw Error(ExitCode::Io, "cannot create output file '" + out_path + "'");

  try {
    CtrStream stream(aes, iv);
    Bytes buf(1 << 20);
    for (;;) {
      fin.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
      const std::streamsize got = fin.gcount();
      if (got > 0) {
        stream.process(buf.data(), buf.data(), static_cast<std::size_t>(got));
        fout.write(reinterpret_cast<const char*>(buf.data()), got);
        if (!fout) throw Error(ExitCode::Io, "write error on '" + out_path + "'");
      }
      if (!fin) break;
    }
    if (fin.bad()) throw Error(ExitCode::Io, "read error on '" + in_path + "'");
    fout.flush();
    if (!fout) throw Error(ExitCode::Io, "write error on '" + out_path + "'");
    fout.close();
    fin.close();
    secure_zero(buf.data(), buf.size());
    fs::rename(tmp, out, ec);
    if (ec) throw Error(ExitCode::Io, "cannot finalize output file '" + out_path + "'");
  } catch (...) {
    fout.close();
    fs::remove(tmp, ec);  // fail closed: never leave partial output behind
    throw;
  }
}

}  // namespace aesp
