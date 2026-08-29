#include <iostream>

#include "vektoryum/version.hpp"

int main() {
    std::cout << "Vektoryum v2 core " << vektoryum::version_string() << '\n';
    return 0;
}
