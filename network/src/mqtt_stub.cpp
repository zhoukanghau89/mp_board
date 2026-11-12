#include "mqtt.h"

#include <chrono>
#include <thread>

#include "mp/event.h"
#include "mp/log.h"

namespace mp {

bool MqttStub::Connect(const std::string& host, int port,
                       const std::string& client_id) {
  client_id_ = client_id;
  MP_LOG_INFO("mqtt connect %s:%d client_id=%s (stub)", host.c_str(), port,
              client_id.c_str());
  connected_ = true;
  return true;
}

void MqttStub::Disconnect() {
  if (connected_) MP_LOG_INFO("mqtt disconnect");
  connected_ = false;
}

bool MqttStub::Publish(const std::string& topic, const std::string& payload) {
  if (!connected_) return false;
  // 真实实现这里走网络，stub 只打印长度，避免日志刷屏
  static uint32_t n = 0;
  if ((++n % 10) == 1) {
    MP_LOG_INFO("mqtt pub %s len=%zu", topic.c_str(), payload.size());
  }
  return true;
}

bool MqttStub::Subscribe(const std::string& topic, Handler handler) {
  if (!connected_) return false;
  sub_topic_ = topic;
  handler_ = std::move(handler);
  MP_LOG_INFO("mqtt sub %s", topic.c_str());
  return true;
}

int MqttStub::Poll(int timeout_ms) {
  if (!connected_) return 0;
  std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));

  // 每 30 秒模拟一条云端下行指令，用来打通反向链路
  const uint64_t now = NowMs();
  if (last_downlink_ms_ == 0) last_downlink_ms_ = now;
  if (now - last_downlink_ms_ < 30000) return 0;
  last_downlink_ms_ = now;

  if (!handler_) return 0;
  Message msg;
  msg.topic = sub_topic_;
  msg.payload = std::string("{\"cmd\":\"start_mow\",\"seq\":") +
                std::to_string(++downlink_count_) + "}";
  handler_(msg);
  return 1;
}

}  // namespace mp
