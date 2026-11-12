// 进程守护：拉起子进程，异常退出按退避重启
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "mp/config.h"
#include "mp/log.h"

namespace {
volatile sig_atomic_t g_running = 1;
void OnSignal(int) { g_running = 0; }

struct Child {
    std::string name;
    pid_t pid = -1;
    int restarts = 0;
    std::chrono::steady_clock::time_point next_try{};
};

std::string ExeDir() {
    char buf[1024];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = '\0';
    std::string path(buf);
    const size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? "." : path.substr(0, pos);
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string cfg_path = "config/default.conf";
    if (argc > 1) cfg_path = argv[1];

    mp::Config cfg;
    cfg.Load(cfg_path);

    mp::LogConfig lc;
    lc.dir    = cfg.GetStr("common", "log_dir", "/tmp/mp_board/logs");
    lc.prefix = "mpm";
    lc.level  = static_cast<mp::LogLevel>(cfg.GetInt("common", "log_level", 1));
    mp::Logger::Instance().Init(lc, "mpm");

    const std::string bin_dir = ExeDir();
    const std::string procs = cfg.GetStr("mpm", "procs",
                                         "zmq_hub,carrier,network,tasks,monitor");

    std::vector<Child> children;
    for (const auto& name : Split(procs, ',')) {
        Child c;
        c.name = name;
        children.push_back(c);
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);
    std::signal(SIGCHLD, SIG_DFL);

    MP_LOG_INFO("mpm start, %zu children, bin_dir=%s", children.size(),
                bin_dir.c_str());

    auto spawn = [&](Child* c) {
        const std::string path = bin_dir + "/" + c->name;
        const pid_t pid = ::fork();
        if (pid < 0) {
            MP_LOG_ERROR("fork %s failed", c->name.c_str());
            c->next_try = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            return;
        }
        if (pid == 0) {
            ::execl(path.c_str(), c->name.c_str(), cfg_path.c_str(),
                    static_cast<char*>(nullptr));
            // execl 失败直接退出，父进程会重启
            _exit(127);
        }
        c->pid = pid;
        MP_LOG_INFO("spawn %s pid=%d", c->name.c_str(), static_cast<int>(pid));
    };

    while (g_running) {
        const auto now = std::chrono::steady_clock::now();

        for (auto& c : children) {
            if (c.pid <= 0) {
                if (now >= c.next_try) spawn(&c);
                continue;
            }
            int status = 0;
            const pid_t r = ::waitpid(c.pid, &status, WNOHANG);
            if (r == c.pid) {
                const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                MP_LOG_WARN("%s exited code=%d, restart #%d", c.name.c_str(), code,
                            c.restarts + 1);
                c.pid = -1;
                ++c.restarts;
                const int backoff_s = std::min(30, 1 << std::min(c.restarts, 5));
                c.next_try = now + std::chrono::seconds(backoff_s);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    MP_LOG_INFO("mpm stopping children");
    for (auto& c : children) {
        if (c.pid > 0) ::kill(c.pid, SIGTERM);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (auto& c : children) {
        if (c.pid > 0) ::kill(c.pid, SIGKILL);
    }
    for (auto& c : children) {
        if (c.pid > 0) ::waitpid(c.pid, nullptr, 0);
    }

    MP_LOG_INFO("mpm exit");
    mp::Logger::Instance().Shutdown();
    return 0;
}
