#include "mp/log.h"

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <mutex>

namespace mp {
namespace {
std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

bool MakeDir(const std::string& path) {
    if (path.empty()) return false;
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if (path[i] == '/' || i + 1 == path.size()) {
            if (cur == "/" || cur.empty()) continue;
            ::mkdir(cur.c_str(), 0755);
        }
    }
    return true;
}
}  // namespace

Logger& Logger::Instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() { Shutdown(); }

const char* Logger::LevelTag(LogLevel lv) {
    switch (lv) {
        case LogLevel::kDebug: return "D";
        case LogLevel::kInfo:  return "I";
        case LogLevel::kWarn:  return "W";
        case LogLevel::kError: return "E";
    }
    return "?";
}

void Logger::Init(const LogConfig& cfg, const std::string& module) {
    auto* mtx = static_cast<std::mutex*>(mutex_);
    if (mtx == nullptr) {
        mutex_ = new std::mutex();
        mtx = static_cast<std::mutex*>(mutex_);
    }
    std::lock_guard<std::mutex> lk(*mtx);

    dir_       = cfg.dir;
    prefix_    = cfg.prefix;
    module_    = module;
    max_bytes_ = cfg.max_bytes;
    keep_      = cfg.keep;
    console_   = cfg.console;
    level_     = cfg.level;

    MakeDir(dir_);

    if (fp_ != nullptr) {
        std::fclose(fp_);
        fp_ = nullptr;
    }
    const std::string path = JoinPath(dir_, prefix_ + ".log");
    fp_ = std::fopen(path.c_str(), "a");
    written_ = 0;
}

void Logger::Shutdown() {
    auto* mtx = static_cast<std::mutex*>(mutex_);
    if (mtx == nullptr) return;
    std::lock_guard<std::mutex> lk(*mtx);
    if (fp_ != nullptr) {
        std::fclose(fp_);
        fp_ = nullptr;
    }
}

void Logger::RotateIfNeeded(size_t incoming) {
    if (fp_ == nullptr) return;
    if (written_ + incoming <= max_bytes_) return;

    std::fclose(fp_);
    fp_ = nullptr;

    // 旧文件依次后移：app.log.4 -> 删除，app.log.3 -> app.log.4 ...
    for (int i = keep_ - 1; i >= 1; --i) {
        const std::string from = JoinPath(dir_, prefix_ + ".log." + std::to_string(i));
        const std::string to   = JoinPath(dir_, prefix_ + ".log." + std::to_string(i + 1));
        std::rename(from.c_str(), to.c_str());
    }
    const std::string cur = JoinPath(dir_, prefix_ + ".log");
    const std::string bak = JoinPath(dir_, prefix_ + ".log.1");
    std::rename(cur.c_str(), bak.c_str());

    fp_ = std::fopen(cur.c_str(), "a");
    written_ = 0;
}

void Logger::Write(LogLevel lv, const char* fmt, ...) {
    if (lv < level_) return;
    auto* mtx = static_cast<std::mutex*>(mutex_);
    if (mtx == nullptr) {
        // 未初始化时退化为控制台输出
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(stderr, fmt, ap);
        va_end(ap);
        std::fputc('\n', stderr);
        return;
    }

    char body[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);

    char head[64];
    std::snprintf(head, sizeof(head), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<int>(ms.count()));

    char line[2304];
    const int n = std::snprintf(line, sizeof(line), "%s %s/%s [%d] %s\n", head,
                                module_.c_str(), LevelTag(lv),
                                static_cast<int>(::getpid()), body);
    if (n <= 0) return;

    std::lock_guard<std::mutex> lk(*mtx);
    RotateIfNeeded(static_cast<size_t>(n));
    if (fp_ != nullptr) {
        std::fwrite(line, 1, static_cast<size_t>(n), fp_);
        std::fflush(fp_);
        written_ += static_cast<size_t>(n);
    }
    if (console_) std::fwrite(line, 1, static_cast<size_t>(n), stderr);
}

}  // namespace mp
