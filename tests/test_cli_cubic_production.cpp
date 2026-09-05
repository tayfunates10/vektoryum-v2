#include "tiff_baseline_fixtures.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void write_binary(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "fixture output must open");
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(out), "fixture output must write completely");
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(static_cast<bool>(in), "production SVG output must exist");
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

[[nodiscard]] std::string quote(const std::filesystem::path& path) {
    return std::string{"\""} + path.string() + "\"";
}

[[nodiscard]] int run_command(const std::string& command) {
#ifdef _WIN32
    const std::string wrapped = std::string{"\""} + command + "\"";
    return std::system(wrapped.c_str());
#else
    return std::system(command.c_str());
#endif
}

}  // namespace

int main(int argc, char** argv) {
    require(argc == 2, "test requires path to vektoryum_cli");
    const std::filesystem::path cli = argv[1];
    const auto dir = std::filesystem::temp_directory_path() / "vektoryum-u7-cli-cubic";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    ec.clear();
    require(std::filesystem::create_directories(dir, ec), "temporary fixture directory must be created");

    const auto input_path = dir / "smooth-rgba.tiff";
    const auto output_path = dir / "smooth-rgba.svg";
    write_binary(input_path, vektoryum_test_fixtures::tiff_rgba_13x9);

    const std::string command = quote(cli) + " --certified-convert " +
                                quote(input_path) + " " + quote(output_path) + " svg";
    require(run_command(command) == 0,
            "real certified CLI production path must accept the smooth vector fixture");

    const std::string svg = read_text(output_path);
    require(svg.find("<path") != std::string::npos,
            "production SVG must contain reconstructed path geometry");
    require(svg.find(" C") != std::string::npos,
            "production SVG must contain a certified cubic command");

    const auto certificate_path = std::filesystem::path(output_path.string() + ".quality-certificate");
    require(std::filesystem::exists(certificate_path),
            "production cubic output must retain the quality certificate boundary");

    std::filesystem::remove_all(dir, ec);
    std::cout << "U7 production CLI cubic regression passed\n";
    return 0;
}
