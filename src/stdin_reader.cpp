#include "line_reader.hpp"
#include <iostream>

class StdinReader : public LineReader {
public:
    bool readLine(std::string& line) override {
        return static_cast<bool>(std::getline(std::cin, line));
    }
};

std::unique_ptr<LineReader> make_stdin_reader() {
    return std::make_unique<StdinReader>();
}
