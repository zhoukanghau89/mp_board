// 事件总线中枢：XSUB/XPUB 代理 + 流量统计
#include <zmq.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "mp/bus.h"
#include "mp/config.h"
#include "mp/event.h"
#include "mp/log.h"

namespace {
void* g_ctx = nullptr;
std::atomic<bool> g_running{true};

void OnSignal(int) { g_running = false; }
}  // namespace

int main(int argc, char** argv) {
    const std::string cfg_path = argc > 1 ? argv[1] : "config/default.conf";
    mp::Config cfg;
    cfg.Load(cfg_path);

    int pub_port = cfg.GetInt("bus", "pub_port", 27788);
    int sub_port = cfg.GetInt("bus", "sub_port", 27789);

    mp::LogConfig lc;
    lc.dir = cfg.GetStr("common", "log_dir", "/tmp/mp_board/logs");
    lc.prefix = "zmq_hub";
    mp::Logger::Instance().Init(lc, "zhub");

    g_ctx = zmq_ctx_new();
    void* xpub = zmq_socket(g_ctx, ZMQ_XPUB);
    void* xsub = zmq_socket(g_ctx, ZMQ_XSUB);
    int linger = 0;
    zmq_setsockopt(xpub, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(xsub, ZMQ_LINGER, &linger, sizeof(linger));

    const std::string pub_ep = mp::MakeEndpoint(pub_port);
    const std::string sub_ep = mp::MakeEndpoint(sub_port);
    if (zmq_bind(xpub, pub_ep.c_str()) != 0) {
        MP_LOG_ERROR("bind pub failed: %s", zmq_strerror(zmq_errno()));
        return 1;
    }
    if (zmq_bind(xsub, sub_ep.c_str()) != 0) {
        MP_LOG_ERROR("bind sub failed: %s", zmq_strerror(zmq_errno()));
        return 1;
    }
    MP_LOG_INFO("hub started pub=%s sub=%s", pub_ep.c_str(), sub_ep.c_str());

    std::thread proxy([&]() {
      zmq_proxy(xsub, xpub, nullptr);
      MP_LOG_INFO("hub proxy exited");
    });

    // 自己订阅一份总线流量用于统计
    mp::Subscriber tap;
    if (tap.Connect(pub_ep) && tap.Subscribe("")) {
        MP_LOG_INFO("hub tap ready");
    } else {
        MP_LOG_WARN("hub tap connect failed");
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    std::atomic<uint64_t> frames{0};
    std::thread stats([&]() {
      uint64_t last = 0;
      while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        const uint64_t now = frames.load();
        MP_LOG_INFO("frames total=%llu delta=%llu",
                    static_cast<unsigned long long>(now),
                    static_cast<unsigned long long>(now - last));
        last = now;
      }
    });

    while (g_running.load()) {
        mp::Event ev;
        if (tap.Poll(&ev, 200)) frames.fetch_add(1);
    }

    MP_LOG_INFO("hub stopping");
    tap.Close();
    zmq_close(xpub);
    zmq_close(xsub);
    zmq_ctx_shutdown(g_ctx);
    g_running.store(false);
    if (stats.joinable()) stats.join();
    if (proxy.joinable()) proxy.join();
    zmq_ctx_term(g_ctx);
    mp::Logger::Instance().Shutdown();
    return 0;
}
