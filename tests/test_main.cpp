#include <cstdlib>
#include <iostream>
#include <string_view>

#include "vektoryum/version.hpp"

namespace {

int failures = 0;

void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
        return;
    }
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    expect_true(vektoryum::version_major == 0, "version major is stable");
    expect_true(vektoryum::version_minor == 1, "version minor is stable");
    expect_true(vektoryum::version_patch == 0, "version patch is stable");
    expect_true(vektoryum::version_string() == "0.1.0", "version string matches components");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All tests passed\n";
    return EXIT_SUCCESS;
}
