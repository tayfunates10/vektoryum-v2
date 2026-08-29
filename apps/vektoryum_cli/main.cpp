#include <iostream>
#include <string_view>

#include "vektoryum/api/stable_api.hpp"
#include "vektoryum/version.hpp"

namespace {

int print_version() {
    std::cout << "Vektoryum v2 core " << vektoryum::version_string() << '\n';
    return static_cast<int>(vektoryum::api::ExitCode::Success);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        return print_version();
    }

    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--version") {
            return print_version();
        }
        if (argument == "--help") {
            std::cout << "usage: vektoryum_cli [--version|--help]\n";
            return static_cast<int>(vektoryum::api::ExitCode::Success);
        }
    }

    std::cerr << "error: unsupported command line\n";
    return static_cast<int>(vektoryum::api::ExitCode::Usage);
}
