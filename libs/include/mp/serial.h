#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace mp {

// 串口封装：8N1，非阻塞读带超时。
class SerialPort {
  public:
    SerialPort() = default;
    ~SerialPort() { Close(); }

    bool Open(const std::string& dev, int baud);
    void Close();
    bool is_open() const { return fd_ >= 0; }

    // 返回实际读到的字节数，0 表示超时，<0 表示错误
    int Read(void* buf, size_t len, int timeout_ms);
    int Write(const void* buf, size_t len);

  private:
    int fd_ = -1;
};

// 无硬件时的模拟数据源：产生与真机同格式的帧
class SerialSimulator {
  public:
    explicit SerialSimulator(uint8_t mod_id) : mod_id_(mod_id) {}
    // 按 len 生成数据，返回生成字节数
    int Fill(void* buf, size_t len);

  private:
    uint8_t mod_id_;
    uint32_t tick_ = 0;
};

}  // namespace mp
