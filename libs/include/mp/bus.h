#pragma once
#include <string>

#include "mp/event.h"

namespace mp {

// 事件总线：基于 ZMQ 的星型结构，所有进程连到 zmq_hub。
class Publisher {
  public:
    Publisher() = default;
    ~Publisher();
    bool Connect(const std::string& endpoint);
    void Close();
    bool Publish(const Event& ev);

  private:
    void* sock_ = nullptr;
};

class Subscriber {
  public:
    Subscriber() = default;
    ~Subscriber();
    bool Connect(const std::string& endpoint);
    // 订阅空串表示全部事件
    bool Subscribe(const std::string& topic);
    // 返回 true 表示取到事件；timeout_ms 为 -1 时阻塞
    bool Poll(Event* out, int timeout_ms);
    void Close();

  private:
    void* sock_ = nullptr;
};

// 由端口拼 endpoint：tcp://127.0.0.1:<port>
std::string MakeEndpoint(int port);

}  // namespace mp
