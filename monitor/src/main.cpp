// 监控进程：事件计数 + 进程资源采样
#include <dirent.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unistd.h>

#include "mp/bus.h"
#include "mp/config.h"
#include "mp/event.h"
#include "mp/log.h"

namespace {
volatile sig_atomic_t g_running = 1;
void OnSignal(int) { g_running = 0; }

// 读取 /proc/<pid>/status 里的 VmRSS，单位 KB
int ReadRssKb(int pid) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* fp = std::fopen(path, "r");
    if (fp == nullptr) return -1;
    char line[256];
    int rss = -1;
    while (std::fgets(line, sizeof(line), fp)) {
        if (std::strncmp(line, "VmRSS:", 6) == 0) {
            std::sscanf(line + 6, "%d", &rss);
            break;
        }
    }
    std::fclose(fp);
    return rss;
}
}  // namespace

int main(int argc, char** argv) {
    const std::string cfg_path = argc > 1 ? argv[1] : "config/default.conf";
    mp::Config cfg;
    cfg.Load(cfg_path);

    mp::LogConfig lc;
    lc.dir    = cfg.GetStr("common", "log_dir", "/tmp/mp_board/logs");
    lc.prefix = "monitor";
    lc.level  = static_cast<mp::LogLevel>(cfg.GetInt("common", "log_level", 1));
    mp::Logger::Instance().Init(lc, "mpmt");

    const int pub_port = cfg.GetInt("bus", "pub_port", 27788);
    mp::Subscriber sub;
    if (!sub.Connect(mp::MakeEndpoint(pub_port)) || !sub.Subscribe("")) {
        MP_LOG_ERROR("connect sub failed");
        return 1;
    }
    MP_LOG_INFO("monitor started");

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    std::map<uint16_t, uint64_t> counters;
    uint64_t total = 0;
    auto last_report = std::chrono::steady_clock::now();
    int rss_min = 1 << 30;
    int rss_max = 0;

    while (g_running) {
        mp::Event ev;
        while (sub.Poll(&ev, 200)) {
            ++counters[ev.id];
            ++total;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_report >= std::chrono::seconds(5)) {
            last_report = now;

            const int rss = ReadRssKb(::getpid());
            if (rss > 0) {
                if (rss < rss_min) rss_min = rss;
                if (rss > rss_max) rss_max = rss;
            }
            MP_LOG_INFO("events total=%llu rss=%dKB(min=%d max=%d)",
                        static_cast<unsigned long long>(total), rss, rss_min, rss_max);

            char line[512];
            int off = std::snprintf(line, sizeof(line), "counters:");
            for (const auto& kv : counters) {
                off += std::snprintf(line + off, sizeof(line) - off, " 0x%04X=%llu",
                                     kv.first, static_cast<unsigned long long>(kv.second));
                if (off >= static_cast<int>(sizeof(line)) - 32) break;
            }
            MP_LOG_INFO("%s", line);
        }
    }

    MP_LOG_INFO("monitor exit");
    mp::Logger::Instance().Shutdown();
    return 0;
}
