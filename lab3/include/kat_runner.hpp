/*
    File: kat_runner.hpp
    Usage: Run Known Answer Tests from JSON file: rsatool --kat vectors.json
*/

#pragma once
#include <string>
using namespace std;

namespace rsalab {
    int run_kat(const string& vectors_path, bool verbose);  // Print PASS/FAIL for each case and summary table. Return 0 if all cases PASS (and at least 1 case), else 1.
} // end namespace rsalab