#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vektoryum::ml {

[[nodiscard]] std::string sha256_hex(const std::vector<std::uint8_t>& bytes);

}  // namespace vektoryum::ml
