#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace mp {

// MQTT 抽象：真机接 libmosquitto / aws-crt，本仓库默认用 stub 跑通链路
class MqttClient {
 public:
  struct Message {
    std::string topic;
    std::string payload;
  };

  using Handler = std::function<void(const Message&)>;

  virtual ~MqttClient() = default;
  virtual bool Connect(const std::string& host, int port,
                       const std::string& client_id) = 0;
  virtual void Disconnect() = 0;
  virtual bool is_connected() const = 0;
  virtual bool Publish(const std::string& topic, const std::string& payload) = 0;
  virtual bool Subscribe(const std::string& topic, Handler handler) = 0;
  // 驱动内部事件循环，返回处理的消息数
  virtual int Poll(int timeout_ms) = 0;
};

// 默认实现：不依赖外部库，打印收发内容并模拟下行指令
class MqttStub : public MqttClient {
 public:
  bool Connect(const std::string& host, int port,
               const std::string& client_id) override;
  void Disconnect() override;
  bool is_connected() const override { return connected_; }
  bool Publish(const std::string& topic, const std::string& payload) override;
  bool Subscribe(const std::string& topic, Handler handler) override;
  int Poll(int timeout_ms) override;

 private:
  bool connected_ = false;
  std::string client_id_;
  std::string sub_topic_;
  Handler handler_;
  uint64_t last_downlink_ms_ = 0;
  int downlink_count_ = 0;
};

}  // namespace mp
