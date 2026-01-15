#pragma once
#include <memory>
#include <string>

class LineReader {
public:
    virtual ~LineReader() = default;
    virtual bool readLine(std::string& line) = 0; // blocking; false=end/error
};

std::unique_ptr<LineReader> make_stdin_reader();
std::unique_ptr<LineReader> make_serial_reader(const std::string& port, int baud, bool& ok);
