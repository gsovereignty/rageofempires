#include <exception>
#include <iostream>

#include "aoe/sdl_app.hpp"

int main() {
    try {
        return aoe::SdlApp{}.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}

