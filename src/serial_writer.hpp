#pragma once
#include <string>

class SerialWriter {
public:
    SerialWriter() = default;
    ~SerialWriter();

    bool open(const std::string& portName, int baud);
    void close();
    bool isOpen() const;

    bool writeLine(const std::string& line);

private:
#ifdef _WIN32
    void* m_h = nullptr; // HANDLE
#else
    int m_fd = -1;
#endif
};
