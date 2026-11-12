// IoT 进程：订阅设备状态 -> 组 JSON 上云；接收云端指令 -> 转内部事件
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>

#include "mp/bus.h"
#include "mp/config.h"
#include "mp/event.h"
#include "mp/log.h"
#include "mp/protocol.h"
#include "mp/shm.h"
#include "mqtt.h"

namespace {
volatile sig_atomic_t g_running = 1;
void OnSignal(int) { g_running = 0; }

struct TaskCmd {
    uint8_t  action;  // 1 开始割草 2 回充 3 暂停
    uint8_t  reserved[3];
};

std::string BuildStatusJson(const mp::ModSnapshot& s) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "{\"ts\":%u,\"left_rpm\":%.2f,\"right_rpm\":%.2f,"
                  "\"voltage\":%.2f,\"soc\":%u,\"battery\":%u,"
                  "\"cutter_rpm\":%u,\"cutter_on\":%u}",
                  s.stamp_ms, s.left_rpm, s.right_rpm, s.voltage, s.soc,
                  s.battery_status, s.cutter_rpm, s.cutter_on);
    return std::string(buf);
}
}  // namespace

int main(int argc, char** argv) {
    const std::string cfg_path = argc > 1 ? argv[1] : "config/default.conf";
    mp::Config cfg;
    cfg.Load(cfg_path);

    mp::LogConfig lc;
    lc.dir    = cfg.GetStr("common", "log_dir", "/tmp/mp_board/logs");
    lc.prefix = "network";
    lc.level  = static_cast<mp::LogLevel>(cfg.GetInt("common", "log_level", 1));
    mp::Logger::Instance().Init(lc, "iot");

    const int pub_port = cfg.GetInt("bus", "pub_port", 27788);
    const int sub_port = cfg.GetInt("bus", "sub_port", 27789);
    const std::string sn = cfg.GetStr("network", "sn", "MPSN0001");

    mp::Publisher pub;
    mp::Subscriber sub;
    if (!pub.Connect(mp::MakeEndpoint(sub_port))) {
        MP_LOG_ERROR("connect pub failed");
        return 1;
    }
    if (!sub.Connect(mp::MakeEndpoint(pub_port)) || !sub.Subscribe("")) {
        MP_LOG_ERROR("connect sub failed");
        return 1;
    }

    mp::MqttStub mqtt;
    if (!mqtt.Connect(cfg.GetStr("network", "host", "127.0.0.1"),
                      cfg.GetInt("network", "port", 1883),
                      "mp_board_" + sn)) {
        MP_LOG_ERROR("mqtt connect failed");
        return 1;
    }

    mqtt.Subscribe("dev/" + sn + "/cmd", [&](const mp::MqttClient::Message& msg) {
      MP_LOG_INFO("downlink %s -> %s", msg.topic.c_str(), msg.payload.c_str());
      TaskCmd cmd{};
      cmd.action = 1;
      mp::Event ev = mp::Event::Make(mp::kEvTaskCmd, cmd);
      pub.Publish(ev);
    });

    {
        struct NetStatus {
            uint8_t online;
            uint8_t reserved[3];
        } st{1, {0, 0, 0}};
        mp::Event ev = mp::Event::Make(mp::kEvNetStatus, st);
        pub.Publish(ev);
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    mp::ModSnapshot snap{};
    auto last_report = std::chrono::steady_clock::now();

    while (g_running) {
        mp::Event ev;
        while (sub.Poll(&ev, 0)) {
            if (ev.id == mp::kEvModUpdate) ev.As(&snap);
        }

        mqtt.Poll(100);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_report >= std::chrono::seconds(2)) {
            last_report = now;
            const std::string topic = "dev/" + sn + "/status";
            mqtt.Publish(topic, BuildStatusJson(snap));
            MP_LOG_DEBUG("report %s", BuildStatusJson(snap).c_str());
        }
    }

    mqtt.Disconnect();
    MP_LOG_INFO("network exit");
    mp::Logger::Instance().Shutdown();
    return 0;
}
