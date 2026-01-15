#include "serial_writer.hpp"

#ifdef _WIN32
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

SerialWriter::~SerialWriter() { close(); }

bool SerialWriter::isOpen() const {
#ifdef _WIN32
    return m_h != nullptr;
#else
    return m_fd >= 0;
#endif
}

void SerialWriter::close() {
#ifdef _WIN32
    if (m_h) {
        CloseHandle((HANDLE)m_h);
        m_h = nullptr;
    }
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

#ifdef _WIN32

static std::string normalize_com(const std::string& port) {
    // COM10+ -> \\.\COM10
    if (port.rfind("COM", 0) == 0 && port.size() > 4) return "\\\\.\\" + port;
    return port;
}

bool SerialWriter::open(const std::string& portName, int baud) {
    close();

    std::string p = normalize_com(portName);

    HANDLE h = CreateFileA(
        p.c_str(),
        GENERIC_WRITE,
        0, nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    if (h == INVALID_HANDLE_VALUE) return false;

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); return false; }

    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(h, &dcb)) { CloseHandle(h); return false; }

    m_h = h;
    return true;
}

bool SerialWriter::writeLine(const std::string& line) {
    if (!m_h) return false;
    std::string s = line;
    s.push_back('\n');

    DWORD written = 0;
    BOOL ok = WriteFile((HANDLE)m_h, s.data(), (DWORD)s.size(), &written, nullptr);
    return ok && written == s.size();
}

#else // POSIX (Linux/macOS)

static speed_t baud_to_flag(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B9600;
    }
}

bool SerialWriter::open(const std::string& portName, int baud) {
    close();

    int fd = ::open(portName.c_str(), O_WRONLY | O_NOCTTY);
    if (fd < 0) return false;

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) { ::close(fd); return false; }

    cfsetispeed(&tty, baud_to_flag(baud));
    cfsetospeed(&tty, baud_to_flag(baud));

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) { ::close(fd); return false; }

    m_fd = fd;
    return true;
}

bool SerialWriter::writeLine(const std::string& line) {
    if (m_fd < 0) return false;
    std::string s = line;
    s.push_back('\n');

    ssize_t n = ::write(m_fd, s.data(), s.size());
    return n == (ssize_t)s.size();
}

#endif
