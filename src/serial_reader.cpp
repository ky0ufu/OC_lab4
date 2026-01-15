#include "line_reader.hpp"
#include <string>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

#ifdef _WIN32

static std::string normalize_com(const std::string& port) {
    // COM10+ -> \\.\COM10
    if (port.rfind("COM", 0) == 0 && port.size() > 4) return "\\\\.\\" + port;
    return port;
}

class SerialReader : public LineReader {
public:
    SerialReader(const std::string& port, int baud, bool& ok) { ok = open(port, baud); }
    ~SerialReader() override { close(); }

    bool readLine(std::string& line) override {
        line.clear();
        if (!m_h) return false;

        while (true) {
            auto pos = m_buf.find('\n');
            if (pos != std::string::npos) {
                line = m_buf.substr(0, pos);
                m_buf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return true;
            }

            char tmp[256];
            DWORD got = 0;
            if (!ReadFile(m_h, tmp, (DWORD)sizeof(tmp), &got, nullptr)) return false;
            if (got == 0) continue;
            m_buf.append(tmp, tmp + got);
        }
    }

private:
    HANDLE m_h = nullptr;
    std::string m_buf;

    bool open(const std::string& port, int baud) {
        close();
        std::string p = normalize_com(port);
        HANDLE h = CreateFileA(p.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;

        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(h, &dcb)) { CloseHandle(h); return false; }

        dcb.BaudRate = baud;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        if (!SetCommState(h, &dcb)) { CloseHandle(h); return false; }

        COMMTIMEOUTS to{};
        to.ReadIntervalTimeout = 50;
        to.ReadTotalTimeoutConstant = 50;
        to.ReadTotalTimeoutMultiplier = 10;
        SetCommTimeouts(h, &to);

        m_h = h;
        return true;
    }

    void close() {
        if (m_h) { CloseHandle(m_h); m_h = nullptr; }
    }
};

#else // POSIX

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

class SerialReader : public LineReader {
public:
    SerialReader(const std::string& port, int baud, bool& ok) { ok = open(port, baud); }
    ~SerialReader() override { close(); }

    bool readLine(std::string& line) override {
        line.clear();
        if (m_fd < 0) return false;

        while (true) {
            auto pos = m_buf.find('\n');
            if (pos != std::string::npos) {
                line = m_buf.substr(0, pos);
                m_buf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return true;
            }

            char tmp[256];
            ssize_t n = ::read(m_fd, tmp, sizeof(tmp));
            if (n <= 0) return false;
            m_buf.append(tmp, tmp + n);
        }
    }

private:
    int m_fd = -1;
    std::string m_buf;

    bool open(const std::string& port, int baud) {
        close();
        int fd = ::open(port.c_str(), O_RDONLY | O_NOCTTY);
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

        tty.c_cc[VMIN]  = 1;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(fd, TCSANOW, &tty) != 0) { ::close(fd); return false; }

        m_fd = fd;
        return true;
    }

    void close() {
        if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
    }
};

#endif

std::unique_ptr<LineReader> make_serial_reader(const std::string& port, int baud, bool& ok) {
    return std::make_unique<SerialReader>(port, baud, ok);
}
