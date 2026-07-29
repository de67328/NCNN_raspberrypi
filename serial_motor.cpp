#include "serial_motor.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
// Windows 模拟：仅用于编译测试，不真正通信
#include <cstdio>
#else
// Linux (Raspberry Pi): 标准 termios
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace ctrl {

// ═══════════════════════════════════════════════════════════
SerialMotor::SerialMotor()
    : fd_(-1)
    , lastOk_(false)
    , trackedPosX10_(0)
{}

SerialMotor::~SerialMotor()
{
    close();
}

// ═══════════════════════════════════════════════════════════
bool SerialMotor::open(const std::string& portPath, int baudRate)
{
    if (fd_ >= 0) {
        lastError_ = "already open";
        return false;
    }

#ifdef _WIN32
    (void)portPath; (void)baudRate;
    lastError_ = "SerialMotor: Windows not supported (use Linux/Pi)";
    std::cerr << "[SerialMotor] " << lastError_ << std::endl;
    return false;
#else
    fd_ = ::open(portPath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        lastError_ = "open(" + portPath + "): " + std::strerror(errno);
        std::cerr << "[SerialMotor] " << lastError_ << std::endl;
        return false;
    }

    // 改回阻塞模式
    int flags = ::fcntl(fd_, F_GETFL, 0);
    ::fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);

    if (!configurePort(baudRate)) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // 清空缓冲区
    ::tcflush(fd_, TCIOFLUSH);

    std::cout << "[SerialMotor] opened " << portPath
              << " @ " << baudRate << " baud" << std::endl;
    return true;
#endif
}

void SerialMotor::close()
{
#ifdef _WIN32
    fd_ = -1;
#else
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        std::cout << "[SerialMotor] closed" << std::endl;
    }
#endif
}

#ifndef _WIN32
bool SerialMotor::configurePort(int baudRate)
{
    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));

    if (::tcgetattr(fd_, &tty) != 0) {
        lastError_ = "tcgetattr: " + std::string(std::strerror(errno));
        return false;
    }

    // 波特率
    speed_t speed = B115200;
    switch (baudRate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:     speed = B115200; break;
    }
    ::cfsetospeed(&tty, speed);
    ::cfsetispeed(&tty, speed);

    // 8N1
    tty.c_cflag &= ~PARENB;        // 无校验
    tty.c_cflag &= ~CSTOPB;        // 1 停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 数据位
    tty.c_cflag &= ~CRTSCTS;       // 无硬件流控
    tty.c_cflag |= CREAD | CLOCAL; // 启用接收，忽略调制解调器状态

    // 本地模式
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

    // 输入模式
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);                      // 无软件流控
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                    | INLCR | IGNCR | ICRNL);                    // 原始输入

    // 输出模式
    tty.c_oflag &= ~OPOST;       // 原始输出
    tty.c_oflag &= ~ONLCR;

    // 超时: 0.1s 字节间超时
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;         // 0.1s

    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
        lastError_ = "tcsetattr: " + std::string(std::strerror(errno));
        return false;
    }
    return true;
}
#endif // !_WIN32

// ═══════════════════════════════════════════════════════════
std::string SerialMotor::sendAndWait(const std::string& cmd, int timeoutMs)
{
    lastOk_ = false;
    lastError_.clear();

    if (fd_ < 0) {
        lastError_ = "port not open";
        return "";
    }

#ifdef _WIN32
    (void)cmd; (void)timeoutMs;
    lastError_ = "Windows stub: no serial";
    return "";
#else
    // 发送: cmd + "\n"
    std::string toSend = cmd + "\n";
    ssize_t written = ::write(fd_, toSend.c_str(), toSend.size());
    if (written < 0) {
        lastError_ = "write: " + std::string(std::strerror(errno));
        return "";
    }
    ::tcdrain(fd_);  // 等待发送完成

    // 接收: 按行读取，直到 '\n'
    std::string line;
    char buf[256];
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        ssize_t n = ::read(fd_, buf, sizeof(buf) - 1);
        if (n > 0) {
            for (ssize_t i = 0; i < n; ++i) {
                if (buf[i] == '\n') {
                    // 去掉末尾可能的 '\r'
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    goto done;
                }
                line += buf[i];
            }
        } else if (n == 0) {
            // EOF
            lastError_ = "EOF on serial port";
            return "";
        } else if (errno != EAGAIN && errno != EINTR) {
            lastError_ = "read: " + std::string(std::strerror(errno));
            return "";
        }
        // 等待一小段时间再读
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    lastError_ = "timeout (" + std::to_string(timeoutMs) + "ms): " + cmd;
    return "";

done:
    // 解析应答
    if (line.rfind("OK ", 0) == 0) {
        lastOk_ = true;
        return line;
    }
    if (line.rfind("POS ", 0) == 0) {
        lastOk_ = true;
        return line;
    }
    if (line.rfind("STATUS ", 0) == 0) {
        lastOk_ = true;
        return line;
    }
    if (line.rfind("ERR ", 0) == 0) {
        lastError_ = "STM32: " + line;
        return line;
    }

    lastError_ = "unexpected response: " + line;
    return line;
#endif
}

// ═══════════════════════════════════════════════════════════
// 控制命令
// ═══════════════════════════════════════════════════════════

bool SerialMotor::enable()
{
    auto resp = sendAndWait("ENABLE");
    return resp.find("OK") != std::string::npos;
}

bool SerialMotor::disable()
{
    auto resp = sendAndWait("DISABLE");
    return resp.find("OK") != std::string::npos;
}

bool SerialMotor::stop()
{
    auto resp = sendAndWait("STOP");
    return resp.find("OK") != std::string::npos;
}

bool SerialMotor::moveRelative(int pulses, int speed, int accel, int& posX10)
{
    std::ostringstream cmd;
    cmd << "MOVE " << pulses << " " << speed << " " << accel;
    auto resp = sendAndWait(cmd.str());

    // 期望: "OK MOVED <posX10>"
    if (resp.find("OK MOVED") == std::string::npos)
        return false;

    // 解析 posX10
    auto pos = resp.find("MOVED ");
    if (pos != std::string::npos) {
        try {
            posX10 = std::stoi(resp.substr(pos + 6));
            trackedPosX10_ = posX10;
            return true;
        } catch (...) {}
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
// 查询命令
// ═══════════════════════════════════════════════════════════

bool SerialMotor::readPosition(int& posX10)
{
    auto resp = sendAndWait("READPOS");

    // 期望: "POS <posX10>"
    if (resp.find("POS ") == std::string::npos)
        return false;

    try {
        posX10 = std::stoi(resp.substr(4));
        trackedPosX10_ = posX10;
        return true;
    } catch (...) {
        return false;
    }
}

bool SerialMotor::readStatus(uint8_t& flags)
{
    auto resp = sendAndWait("STATUS");

    if (resp.find("STATUS ") == std::string::npos)
        return false;

    try {
        int parsed = std::stoi(resp.substr(7));
        flags = static_cast<uint8_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool SerialMotor::zero()
{
    auto resp = sendAndWait("ZERO");
    if (resp.find("OK") != std::string::npos) {
        trackedPosX10_ = 0;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
// 便捷方法
// ═══════════════════════════════════════════════════════════

bool SerialMotor::readPositionDeg(double& deg)
{
    int posX10 = 0;
    if (!readPosition(posX10))
        return false;
    deg = static_cast<double>(posX10) / 10.0;
    return true;
}

} // namespace ctrl
