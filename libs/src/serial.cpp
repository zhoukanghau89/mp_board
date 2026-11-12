#include "mp/serial.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "mp/crc.h"
#include "mp/protocol.h"

namespace mp {
namespace {

speed_t ToSpeed(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 460800: return B460800;
        default: return B115200;
    }
}

}  // namespace

bool SerialPort::Open(const std::string& dev, int baud) {
    Close();
    fd_ = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) return false;

    termios tio{};
    if (::tcgetattr(fd_, &tio) != 0) {
        Close();
        return false;
    }
    ::cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    const speed_t sp = ToSpeed(baud);
    ::cfsetispeed(&tio, sp);
    ::cfsetospeed(&tio, sp);

    if (::tcsetattr(fd_, TCSANOW, &tio) != 0) {
        Close();
        return false;
    }
    ::tcflush(fd_, TCIOFLUSH);
    return true;
}

void SerialPort::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

int SerialPort::Read(void* buf, size_t len, int timeout_ms) {
    if (fd_ < 0) return -1;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    const int rc = ::select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (rc == 0) return 0;
    if (rc < 0) return (errno == EINTR) ? 0 : -1;

    const ssize_t n = ::read(fd_, buf, len);
    if (n < 0) return (errno == EAGAIN || errno == EINTR) ? 0 : -1;
    return static_cast<int>(n);
}

int SerialPort::Write(const void* buf, size_t len) {
    if (fd_ < 0) return -1;
    const ssize_t n = ::write(fd_, buf, len);
    return static_cast<int>(n);
}

int SerialSimulator::Fill(void* buf, size_t len) {
    // 帧格式：55 AA | mod_id | len | payload | crc16(低字节在前)
    auto* p = static_cast<uint8_t*>(buf);
    ++tick_;

    uint8_t payload[16];
    uint8_t plen = 0;

    if (mod_id_ == 0x02) {  // 电池：电压/电流/电量/状态
        BatteryPayload b{};
        b.voltage_mv = 24500;
        b.current_ma = static_cast<int16_t>(-800 - static_cast<int>(tick_ % 200) * 5);
        b.soc = static_cast<uint8_t>(90 - (tick_ % 40));
        b.status = 1;
        std::memcpy(payload, &b, sizeof(b));
        plen = sizeof(b);
    } else if (mod_id_ == 0x04) {  // 刀盘
        CutterPayload c{};
        c.rpm = 2800;
        c.on = 1;
        c.err = 0;
        std::memcpy(payload, &c, sizeof(c));
        plen = sizeof(c);
    } else {  // 轮速
        WheelPayload w{};
        w.left_rpm = 30.0f + static_cast<float>(tick_ % 20);
        w.right_rpm = 30.0f - static_cast<float>(tick_ % 15);
        w.voltage = 24.0f + 0.01f * static_cast<float>(tick_ % 100);
        std::memcpy(payload, &w, sizeof(w));
        plen = sizeof(w);
    }

    const size_t total = 4 + plen + 2;
    if (len < total) return 0;

    p[0] = 0x55;
    p[1] = 0xAA;
    p[2] = mod_id_;
    p[3] = plen;
    std::memcpy(p + 4, payload, plen);

    const uint16_t crc = Crc16(p, 4 + plen);
    p[4 + plen] = static_cast<uint8_t>(crc & 0xFF);
    p[5 + plen] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    return static_cast<int>(total);
}

}  // namespace mp
