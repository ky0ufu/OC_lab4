#include "serial_writer.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>

static void usage() {
    std::cerr <<
      "temp_simulator:\n"
      "  [--interval 1] seconds\n"
      "  [--base 22.0] [--amp 2.0] [--noise 0.2]\n"
      "  [--out stdout|serial]\n"
      "  if --out serial: --port COM5|/dev/X --baud 9600\n";
}

int main(int argc, char** argv) {
    int intervalSec = 1;
    double base = 22.0;
    double amp  = 2.0;
    double noise = 0.2;

    std::string outMode = "stdout";
    std::string port;
    int baud = 9600;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << name << "\n"; std::exit(2); }
            return argv[++i];
        };

        if (a == "--interval") intervalSec = std::stoi(need("--interval"));
        else if (a == "--base") base = std::stod(need("--base"));
        else if (a == "--amp") amp = std::stod(need("--amp"));
        else if (a == "--noise") noise = std::stod(need("--noise"));
        else if (a == "--out") outMode = need("--out");
        else if (a == "--port") port = need("--port");
        else if (a == "--baud") baud = std::stoi(need("--baud"));
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { std::cerr << "Unknown arg: " << a << "\n"; usage(); return 2; }
    }

    SerialWriter sw;
    if (outMode == "serial") {
        if (port.empty()) { std::cerr << "Error: --port required for --out serial\n"; return 2; }
        if (!sw.open(port, baud)) { std::cerr << "Error: cannot open port for write: " << port << "\n"; return 2; }
        std::cerr << "temp_simulator writing to serial " << port << "\n";
    } else if (outMode == "stdout") {
        std::cerr << "temp_simulator writing to stdout\n";
    } else {
        std::cerr << "Error: unknown --out\n";
        return 2;
    }

    std::mt19937 rng{std::random_device{}()};
    std::normal_distribution<double> nd(0.0, noise);

    const double dayPeriod = 24.0 * 3600.0;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        double val = base + amp * std::sin(2.0 * M_PI * (t / dayPeriod)) + nd(rng);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << val;

        if (outMode == "stdout") {
            std::cout << oss.str() << "\n";
            std::cout.flush();
        } else { // serial
            if (!sw.writeLine(oss.str())) {
                std::cerr << "Write error\n";
                return 1;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
    }
}
