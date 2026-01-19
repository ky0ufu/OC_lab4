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

// Реализация чтения строк из последовательного порта для Windows
class SerialReader : public LineReader {
public:
    // Конструктор сразу открывает порт
    SerialReader(const std::string& port, int baud, bool& ok) { ok = open(port, baud); }

    // Деструктор закрывает порт.
    ~SerialReader() override { close(); }

     // Чтение одной строки.
    bool readLine(std::string& line) override {
        line.clear();
        if (!m_h) return false; // если порт не открыт

        while (true) {
            // пробуем найти '\n' в накопленном буфере
            auto pos = m_buf.find('\n');
            if (pos != std::string::npos) {
                line = m_buf.substr(0, pos);
                // Удаляем из буфера строку и '\n'
                m_buf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return true;
            }
            // Если '\n' нет, то читаем ещё из порта
            char tmp[256];
            DWORD got = 0;
            if (!ReadFile(m_h, tmp, (DWORD)sizeof(tmp), &got, nullptr)) return false;
            
            if (got == 0) continue;
            // Добавляем прочитанные байты в общий буфер m_buf
            m_buf.append(tmp, tmp + got);
        }
    }

private:
    HANDLE m_h = nullptr;   // дескриптор COM-порта Windows
    std::string m_buf;      // буфер для накопления байтов до символа '\n'

    // Открытие порта и настройка
    bool open(const std::string& port, int baud) {
        close();
        std::string p = normalize_com(port);
        // Открываем порт на чтение. Sharing=0.
        HANDLE h = CreateFileA(p.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;

        // Настраиваем линию
        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(h, &dcb)) { CloseHandle(h); return false; }

        dcb.BaudRate = baud;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        if (!SetCommState(h, &dcb)) { CloseHandle(h); return false; }

        // Таймауты чтения
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

// Перевод скорости baud в константы termios
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

            // Если '\n' нет то читаем ещё байты из fd
            char tmp[256];
            ssize_t n = ::read(m_fd, tmp, sizeof(tmp));
            if (n <= 0) return false;
            m_buf.append(tmp, tmp + n);
        }
    }

private:
    int m_fd = -1;  // файловый дескриптор устройства /dev/pts/X
    std::string m_buf;

    bool open(const std::string& port, int baud) {
        close();
        // Открываем устройство только на чтение
        // O_NOCTTY: чтобы порт не стал управляющим терминалом процесса
        int fd = ::open(port.c_str(), O_RDONLY | O_NOCTTY);
        if (fd < 0) return false;

        // настройки последовательного порта
        termios tty{};
        if (tcgetattr(fd, &tty) != 0) { ::close(fd); return false; }

        // скорость
        cfsetispeed(&tty, baud_to_flag(baud));
        cfsetospeed(&tty, baud_to_flag(baud));

        // 8 бит данных
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        // Включить приём данных (CREAD), локальный режим (CLOCAL)
        tty.c_cflag |= (CLOCAL | CREAD);

        // Выключить  чётность, 2 стоп-бита, аппаратный RTS/CTS
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        // Без специальной обработки входа/выхода
        tty.c_iflag = 0;
        tty.c_oflag = 0;
        tty.c_lflag = 0;

        // VMIN=1, VTIME=0 -> read() блокируется, пока не придёт хотя бы 1 байт.
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
// Создаёт SerialReader и возвращает как LineReader.
std::unique_ptr<LineReader> make_serial_reader(const std::string& port, int baud, bool& ok) {
    return std::make_unique<SerialReader>(port, baud, ok);
}
