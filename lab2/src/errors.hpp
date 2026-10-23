// Error type + process exit codes. All failures are reported by throwing aesp::Error;
// main() converts them into a short, non-leaky message and a non-zero exit code (fail closed).
#pragma once
#include <stdexcept>
#include <string>

using namespace std;

namespace aesp {

enum class ExitCode : int {
  Ok = 0,
  Failure = 1,          // unexpected internal error
  Usage = 2,            // bad command line
  InvalidInput = 3,     // malformed key / IV / encoding / JSON
  Io = 4,               // file I/O problem
  CounterOverflow = 5,  // CTR counter would wrap (keystream reuse)
  KatFailed = 6         // a Known Answer Test / cross-validation case failed
};

class Error : public std::runtime_error {
 public:
  Error(ExitCode code, const std::string& msg) : std::runtime_error(msg), code_(code) {}
  ExitCode code() const noexcept { return code_; }

 private:
  ExitCode code_;
};

}  // namespace aesp
