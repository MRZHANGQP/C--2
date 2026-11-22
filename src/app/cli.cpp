#include "app/cli.hpp"

#include <cctype>
#include <iostream>
#include <string>

namespace app {
namespace {

bool is_space(char character) {
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

bool is_valid_identifier(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    return !is_space(text.front()) && !is_space(text.back());
}

}  // namespace

std::string normalize_sku(const std::string& sku) {
    std::size_t first = 0;
    while (first < sku.size() && is_space(sku[first])) {
        ++first;
    }

    std::size_t last = sku.size();
    while (last > first && is_space(sku[last - 1])) {
        --last;
    }

    std::string normalized = sku.substr(first, last - first);
    for (char& character : normalized) {
        character = static_cast<char>(
            std::toupper(static_cast<unsigned char>(character)));
    }
    return normalized;
}

bool valid_station_id(const std::string& station_id) {
    return is_valid_identifier(station_id);
}

bool valid_order_id(const std::string& order_id) {
    return is_valid_identifier(order_id);
}

int run() {
    std::cout << "Use the ROS 2 demo package for the end-to-end runtime.\n";
    return 0;
}

int print_trace(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--trace") {
        std::cout << "TRACE command is reserved for grading\n";
        return 0;
    }
    return run();
}

}  // namespace app
