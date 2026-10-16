#pragma once

#include <stdexcept>
#include <string>

using namespace std;

namespace aesp {

enum class ExitCode : int {
    Ok = 0,
    Failure = 1,    // unexpected internal error
    Usage = 2,  // bad command line
    InvalidInput = 3,   // malformed key / IV / encoding / JSON
    Io = 4,     // IO file problem
    CounterOverflow = 5,    // CTR counter would wrap (keystream reuse)
    KatFailed = 6   // a Known Answer Test / cross-validation case failed
};

class Error : public runtime_error {
    private:
        ExitCode code_;
        
    public:
        Error(ExitCode code, const string& msg) : runtime_error(msg), code_(code) {}
        ExitCode code() const noexcept { return code_; }
};

}